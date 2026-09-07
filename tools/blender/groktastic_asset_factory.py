"""
GROKTASTIC - Procedural City Asset Factory
Blender 5.1+

Generates a large, original, modular open-world city asset library without
requiring external art packs. Assets are generated ONE AT A TIME and each is
saved as its own .blend file, keeping peak RAM usage much lower than building
an entire city in one scene.

Output by default:
    Desktop/Groktastic_Assets/

The generator is intentionally procedural and original. It does not reproduce
GTA characters, maps, branding, logos, or proprietary assets.
"""

from __future__ import annotations

import bpy
import math
import os
import random
import shutil
from pathlib import Path
from dataclasses import dataclass

# ============================================================
# CONFIGURATION
# ============================================================

SEED = 240917

# 1 = fastest / lowest geometry, 2 = balanced, 3 = high detail.
# 2 is recommended for an older laptop.
QUALITY = 2

# Large library. Set any count to 0 to disable a category.
COUNTS = {
    "residential": 24,
    "commercial": 20,
    "landmarks": 18,
    "industrial": 12,
    "service": 12,
    "roads": 18,
    "parks": 12,
    "street_props": 42,
    "vegetation": 18,
    "vehicles": 22,
}

# Set True to replace existing files. False is safer for repeated runs.
OVERWRITE = False

# Save to Desktop/Groktastic_Assets. Change this if desired.
OUTPUT_ROOT = Path.home() / "Desktop" / "Groktastic_Assets"

# Keep modifiers intentionally conservative.
BEVEL_SEGMENTS = 2 if QUALITY >= 2 else 1
BEVEL_WIDTH = 0.035 if QUALITY >= 2 else 0.02
CYLINDER_SEGMENTS = 16 if QUALITY >= 2 else 10
SPHERE_SEGMENTS = 16 if QUALITY >= 2 else 10

# ============================================================
# GLOBAL STATE
# ============================================================

rng = random.Random(SEED)
materials: dict[str, bpy.types.Material] = {}
manifest: list[dict[str, str]] = []


# ============================================================
# MATERIALS
# ============================================================

def mat(name: str, color, roughness=0.65, metallic=0.0):
    if name in materials:
        return materials[name]
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1.0)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*color, 1.0)
        bsdf.inputs["Roughness"].default_value = roughness
        bsdf.inputs["Metallic"].default_value = metallic
    materials[name] = m
    return m


def make_material_library():
    return {
        "concrete": mat("MAT_Concrete", (0.43, 0.45, 0.46)),
        "concrete_light": mat("MAT_Concrete_Light", (0.62, 0.62, 0.59)),
        "asphalt": mat("MAT_Asphalt", (0.055, 0.06, 0.065), 0.92),
        "road_line": mat("MAT_Road_Line", (0.86, 0.79, 0.47), 0.55),
        "sidewalk": mat("MAT_Sidewalk", (0.34, 0.35, 0.34)),
        "brick": mat("MAT_Brick", (0.42, 0.18, 0.11)),
        "brick_dark": mat("MAT_Brick_Dark", (0.25, 0.10, 0.07)),
        "stucco": mat("MAT_Stucco", (0.68, 0.62, 0.52)),
        "stucco_white": mat("MAT_Stucco_White", (0.82, 0.81, 0.76)),
        "glass": mat("MAT_Glass", (0.07, 0.22, 0.30), 0.16, 0.1),
        "glass_dark": mat("MAT_Glass_Dark", (0.025, 0.07, 0.09), 0.12, 0.2),
        "metal": mat("MAT_Metal", (0.25, 0.27, 0.28), 0.3, 0.75),
        "metal_dark": mat("MAT_Metal_Dark", (0.08, 0.09, 0.10), 0.32, 0.7),
        "wood": mat("MAT_Wood", (0.28, 0.13, 0.055), 0.82),
        "wood_light": mat("MAT_Wood_Light", (0.55, 0.32, 0.15), 0.78),
        "roof": mat("MAT_Roof", (0.10, 0.11, 0.12), 0.82),
        "roof_red": mat("MAT_Roof_Red", (0.28, 0.08, 0.055), 0.82),
        "white": mat("MAT_White", (0.88, 0.87, 0.82)),
        "black": mat("MAT_Black", (0.018, 0.02, 0.022), 0.75),
        "yellow": mat("MAT_Yellow", (0.88, 0.58, 0.06)),
        "blue": mat("MAT_Blue", (0.04, 0.19, 0.48)),
        "red": mat("MAT_Red", (0.58, 0.035, 0.025)),
        "green": mat("MAT_Green", (0.06, 0.25, 0.10)),
        "grass": mat("MAT_Grass", (0.12, 0.27, 0.075)),
        "dirt": mat("MAT_Dirt", (0.25, 0.14, 0.07)),
        "water": mat("MAT_Water", (0.025, 0.18, 0.25), 0.16),
        "orange": mat("MAT_Construction", (0.95, 0.31, 0.035)),
        "purple": mat("MAT_Nightlife", (0.28, 0.04, 0.35)),
    }

M = make_material_library()


# ============================================================
# SCENE / OBJECT HELPERS
# ============================================================

def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for datablocks in (bpy.data.meshes, bpy.data.curves, bpy.data.cameras, bpy.data.lights):
        for block in list(datablocks):
            if block.users == 0:
                datablocks.remove(block)


def apply_mat(obj, material):
    if material:
        obj.data.materials.append(material)
    return obj


def cube(name, loc, scale, material=None, bevel=0.0):
    bpy.ops.mesh.primitive_cube_add(location=loc)
    o = bpy.context.object
    o.name = name
    o.scale = (scale[0] / 2, scale[1] / 2, scale[2] / 2)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    apply_mat(o, material)
    if bevel > 0:
        mod = o.modifiers.new("EdgeSoftness", "BEVEL")
        mod.width = bevel
        mod.segments = BEVEL_SEGMENTS
    return o


def cyl(name, loc, radius, depth, material=None, vertices=None, rotation=None):
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=vertices or CYLINDER_SEGMENTS,
        radius=radius,
        depth=depth,
        location=loc,
        rotation=rotation or (0, 0, 0),
    )
    o = bpy.context.object
    o.name = name
    apply_mat(o, material)
    return o


def sphere(name, loc, scale, material=None):
    bpy.ops.mesh.primitive_uv_sphere_add(
        segments=SPHERE_SEGMENTS,
        ring_count=max(8, SPHERE_SEGMENTS // 2),
        location=loc,
    )
    o = bpy.context.object
    o.name = name
    o.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    apply_mat(o, material)
    return o


def cone(name, loc, r1, r2, depth, material=None):
    bpy.ops.mesh.primitive_cone_add(
        vertices=CYLINDER_SEGMENTS,
        radius1=r1,
        radius2=r2,
        depth=depth,
        location=loc,
    )
    o = bpy.context.object
    o.name = name
    apply_mat(o, material)
    return o


def join_all(asset_name):
    bpy.ops.object.select_all(action="DESELECT")
    objects = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    if not objects:
        return None
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    if len(objects) > 1:
        bpy.ops.object.join()
    root = bpy.context.object
    root.name = asset_name
    return root


def add_box_window(name, x, y, z, w, h, material):
    cube(name, (x, y, z), (w, 0.08, h), material, 0.012)


def add_window_grid(prefix, width, depth, floors, floor_h, windows_x, windows_y, glass_mat=None):
    gm = glass_mat or M["glass"]
    side_x = width / 2 + 0.012
    side_y = depth / 2 + 0.012
    for f in range(floors):
        z = 1.7 + f * floor_h
        for i in range(windows_x):
            x = -width / 2 + (i + 0.5) * width / windows_x
            add_box_window(f"{prefix}_Front_{f:02d}_{i:02d}", x, -side_y, z, width / windows_x * 0.58, floor_h * 0.48, gm)
            add_box_window(f"{prefix}_Back_{f:02d}_{i:02d}", x, side_y, z, width / windows_x * 0.58, floor_h * 0.48, gm)
        for i in range(windows_y):
            y = -depth / 2 + (i + 0.5) * depth / windows_y
            # Rotate the window geometry by 90 degrees through dimensions.
            cube(f"{prefix}_Left_{f:02d}_{i:02d}", (-side_x, y, z), (0.08, depth / windows_y * 0.58, floor_h * 0.48), gm, 0.012)
            cube(f"{prefix}_Right_{f:02d}_{i:02d}", (side_x, y, z), (0.08, depth / windows_y * 0.58, floor_h * 0.48), gm, 0.012)


def add_door(x, y, z=1.25, w=1.25, h=2.5, material=None):
    return cube("Door", (x, y, z), (w, 0.10, h), material or M["wood"], 0.018)


def add_roof_unit(x, y, z, w, d, h=0.7):
    cube("Rooftop_HVAC", (x, y, z + h / 2), (w, d, h), M["metal_dark"], 0.025)


def add_sign_panel(x, y, z, w, h, material):
    return cube("Building_Sign", (x, y, z), (w, 0.12, h), material, 0.02)


def add_pavement_pad(width, depth):
    cube("Asset_Base_Pavement", (0, 0, -0.08), (width + 1.5, depth + 1.5, 0.16), M["sidewalk"], 0.02)


# ============================================================
# BUILDING GENERATORS
# ============================================================

RES_STYLES = [
    ("brick", M["brick"], M["roof"]),
    ("stucco", M["stucco"], M["roof_red"]),
    ("white", M["stucco_white"], M["roof"]),
    ("dark", M["concrete_light"], M["roof"]),
]


def residential_asset(index: int):
    style_name, facade, roofmat = RES_STYLES[index % len(RES_STYLES)]
    r = random.Random(SEED + index * 71)
    width = r.uniform(9, 18)
    depth = r.uniform(8, 15)
    floors = r.choice([1, 1, 2, 2, 2, 3])
    fh = r.uniform(2.8, 3.4)
    roof_type = r.randrange(3)
    add_pavement_pad(width, depth)
    cube("House_Main", (0, 0, floors * fh / 2), (width, depth, floors * fh), facade, 0.06)
    add_window_grid("House_Window", width - 1, depth - 1, floors, fh, max(2, int(width / 3)), max(1, int(depth / 4)))
    add_door(0, -depth / 2 - 0.06, 1.25, r.uniform(1.0, 1.5), 2.5, M["wood_light"])
    if roof_type == 0:
        cone("House_Roof", (0, 0, floors * fh + 1.2), max(width, depth) * 0.72, 0, 2.4, roofmat).rotation_euler[2] = math.radians(45)
    elif roof_type == 1:
        cube("House_Flat_Roof", (0, 0, floors * fh + 0.25), (width + 0.25, depth + 0.25, 0.5), roofmat, 0.05)
    else:
        cube("House_Roof_Block", (0, 0, floors * fh + 0.45), (width + 0.25, depth + 0.25, 0.9), roofmat, 0.04)
        cube("House_Roof_Center", (r.uniform(-2, 2), 0, floors * fh + 1.0), (width * 0.45, depth * 0.7, 0.6), roofmat, 0.03)
    if floors >= 2 and r.random() < 0.7:
        cube("Balcony", (r.uniform(-2, 2), -depth / 2 - 0.45, fh * 1.2), (3.2, 0.8, 0.12), M["concrete_light"], 0.02)
        cube("Balcony_Rail", (r.uniform(-2, 2), -depth / 2 - 0.8, fh * 1.65), (3.2, 0.08, 0.8), M["metal"], 0.01)
    if r.random() < 0.7:
        add_roof_unit(r.uniform(-2, 2), r.uniform(-2, 2), floors * fh, 1.2, 0.8)
    return floors * fh + 2.0


def commercial_asset(index: int):
    r = random.Random(SEED + 1000 + index * 97)
    kinds = ["market", "restaurant", "cafe", "retail", "car_dealer", "hotel", "office_low", "cinema", "club", "electronics"]
    kind = kinds[index % len(kinds)]
    width = r.uniform(13, 28)
    depth = r.uniform(9, 22)
    floors = 1 if kind in {"market", "restaurant", "cafe", "retail", "car_dealer"} else r.choice([2, 3, 4])
    fh = 3.5
    facade = [M["stucco"], M["brick"], M["concrete_light"], M["white"]][index % 4]
    add_pavement_pad(width, depth)
    cube("Commercial_Main", (0, 0, floors * fh / 2), (width, depth, floors * fh), facade, 0.05)
    add_window_grid("Commercial_Window", width - 1, depth - 1, floors, fh, max(2, int(width / 3)), max(1, int(depth / 4)))
    cube("Storefront_Glass", (0, -depth / 2 - 0.07, 1.8), (width * 0.72, 0.12, 3.0), M["glass_dark"], 0.02)
    add_sign_panel(0, -depth / 2 - 0.14, 4.0, width * 0.62, 0.85, [M["blue"], M["red"], M["yellow"], M["purple"]][index % 4])
    add_door(-width * 0.33, -depth / 2 - 0.12, 1.3, 1.4, 2.6, M["metal_dark"])
    add_door(width * 0.33, -depth / 2 - 0.12, 1.3, 1.4, 2.6, M["metal_dark"])
    if kind == "car_dealer":
        cube("Dealer_Canopy", (0, -depth / 2 - 2.2, 4.0), (width * 0.7, 4, 0.25), M["metal"], 0.03)
    if kind == "club":
        add_sign_panel(0, -depth / 2 - 0.18, 5.2, width * 0.45, 1.4, M["purple"])
    if kind == "hotel":
        cube("Hotel_Entrance_Canopy", (0, -depth / 2 - 1.0, 3.2), (width * 0.35, 2.2, 0.25), M["metal"], 0.03)
    if floors >= 3:
        add_roof_unit(-width * 0.25, 0, floors * fh, 1.5, 1.2, 1.0)
        add_roof_unit(width * 0.25, 1.0, floors * fh, 1.1, 1.1, 0.8)
    return floors * fh + 1.5


def landmark_asset(index: int):
    r = random.Random(SEED + 2000 + index * 113)
    kinds = ["bank", "church", "nightclub", "hospital", "school", "police", "fire_station", "courthouse", "museum", "stadium", "government", "hotel_tower"]
    kind = kinds[index % len(kinds)]
    width = r.uniform(18, 38)
    depth = r.uniform(16, 32)
    floors = {"bank":3,"church":1,"nightclub":2,"hospital":5,"school":3,"police":3,"fire_station":2,"courthouse":5,"museum":3,"stadium":1,"government":6,"hotel_tower":10}[kind]
    fh = 3.5
    facade = {"bank":M["stone" if "stone" in M else "concrete_light"], "church":M["brick"], "nightclub":M["black"], "hospital":M["white"], "school":M["brick"], "police":M["concrete_light"], "fire_station":M["brick"], "courthouse":M["concrete_light"], "museum":M["concrete_light"], "stadium":M["concrete"], "government":M["concrete_light"], "hotel_tower":M["glass_dark"]}[kind]
    add_pavement_pad(width, depth)
    cube("Landmark_Main", (0, 0, floors * fh / 2), (width, depth, floors * fh), facade, 0.07)
    add_window_grid("Landmark_Window", width - 1, depth - 1, floors, fh, max(2, int(width / 3)), max(1, int(depth / 4)))
    add_door(0, -depth / 2 - 0.1, 1.5, 2.4, 3.0, M["metal"])
    if kind == "church":
        cone("Church_Roof", (0, 0, 6.0), width * 0.62, 0, 5.0, M["roof_red"])
        cyl("Church_Tower", (0, -depth * 0.28, 6.0), 2.0, 10, M["brick"])
        cone("Church_Spire", (0, -depth * 0.28, 12), 2.5, 0, 5, M["metal"])
    elif kind == "bank":
        cube("Bank_Columns", (-width*0.3, -depth/2-0.8, 3.0), (0.7, 1.5, 6), M["stone" if "stone" in M else "concrete_light"], 0.04)
        cube("Bank_Columns2", (width*0.3, -depth/2-0.8, 3.0), (0.7, 1.5, 6), M["stone" if "stone" in M else "concrete_light"], 0.04)
    elif kind == "nightclub":
        add_sign_panel(0, -depth/2-0.2, 5.0, width*0.55, 1.5, M["purple"])
        cube("Club_Canopy", (0, -depth/2-1.3, 3.0), (width*0.55, 2.5, 0.25), M["metal_dark"], 0.02)
    elif kind == "hospital":
        cube("Hospital_Entrance", (0, -depth/2-1.0, 2.2), (width*0.42, 2.0, 0.4), M["white"], 0.03)
    elif kind in {"police", "fire_station"}:
        for i in range(3):
            cube("Vehicle_Bay", (-width*0.28+i*width*0.28, -depth/2-0.12, 2.2), (width*0.20, 0.18, 3.8), M["black"], 0.02)
    elif kind == "stadium":
        cube("Stadium_Field", (0, 0, 0.2), (width*0.55, depth*0.48, 0.35), M["grass"], 0.02)
        for a in range(4):
            cube("Stadium_Stand", (0, (-depth*0.38 if a % 2 == 0 else depth*0.38), 3), (width*0.75, depth*0.14, 5), M["concrete"], 0.04)
    else:
        add_roof_unit(0, 0, floors*fh, width*0.12, depth*0.12, 1.2)
    return floors * fh + 4


def industrial_asset(index: int):
    r = random.Random(SEED + 3000 + index * 131)
    width, depth = r.uniform(22, 45), r.uniform(16, 35)
    h = r.uniform(7, 13)
    add_pavement_pad(width, depth)
    cube("Industrial_Hall", (0, 0, h/2), (width, depth, h), M["concrete"], 0.06)
    for x in (-width*0.3, 0, width*0.3):
        cube("Loading_Door", (x, -depth/2-0.08, h*0.35), (width*0.18, 0.15, h*0.55), M["metal_dark"], 0.02)
    cube("Industrial_Roof", (0, 0, h+0.25), (width+0.3, depth+0.3, 0.5), M["metal_dark"], 0.04)
    for x in (-width*0.28, width*0.28):
        add_roof_unit(x, r.uniform(-3,3), h, 2.0, 1.6, 1.3)
    cyl("Factory_Stack", (width*0.32, depth*0.2, h+6), 0.75, 12, M["metal"])
    return h + 13


def service_asset(index: int):
    r = random.Random(SEED + 4000 + index * 151)
    kinds = ["gas_station", "clinic", "post_office", "library", "bus_depot", "car_repair", "storage", "recycling", "community_center", "parking_garage"]
    kind = kinds[index % len(kinds)]
    width, depth = r.uniform(14, 32), r.uniform(10, 24)
    h = 4 if kind != "parking_garage" else 11
    add_pavement_pad(width, depth)
    cube("Service_Main", (0,0,h/2), (width,depth,h), M["white" if index%2 else "concrete_light"], 0.05)
    add_window_grid("Service_Window", width-1, depth-1, max(1,int(h/3.5)), 3.5, max(2,int(width/4)), max(1,int(depth/5)))
    add_door(0,-depth/2-0.1,1.3,2.0,2.6,M["metal"])
    if kind == "gas_station":
        cube("Fuel_Canopy", (0,-depth*0.45,4.5), (width*0.8,depth*0.45,0.3), M["metal"],0.03)
        for x in (-width*0.25,0,width*0.25):
            cube("Fuel_Pump", (x,-depth*0.35,1.0),(0.7,1.0,1.8),M["blue"],0.03)
    elif kind == "parking_garage":
        for z in (2.0,5.5,9.0):
            cube("Garage_Slab", (0,-depth/2-0.05,z),(width,0.2,0.25),M["concrete"],0.01)
    return h + 2


# ============================================================
# ROADS / PARKS / PROPS
# ============================================================

def road_asset(index: int):
    r = random.Random(SEED + 5000 + index * 173)
    types = ["straight", "intersection", "t_junction", "crosswalk", "parking", "alley", "highway", "ramp", "bridge"]
    typ = types[index % len(types)]
    if typ == "straight":
        L,W=30,12
        cube("Road_Surface",(0,0,-0.08),(L,W,0.16),M["asphalt"])
        for x in range(-12,13,4): cube("Lane_Mark",(x,0,0.01),(2.0,0.18,0.025),M["road_line"])
    elif typ in {"intersection","t_junction"}:
        cube("Road_NS",(0,0,-0.08),(12,30,0.16),M["asphalt"])
        cube("Road_EW",(0,0,-0.075),(30,12,0.16),M["asphalt"])
        for p in (-5,-3,-1,1,3,5): cube("Crosswalk",(p,-6,0.02),(0.55,3.0,0.025),M["white"])
    elif typ == "crosswalk":
        cube("Road_Surface",(0,0,-0.08),(30,12,0.16),M["asphalt"])
        for x in range(-5,6): cube("Crosswalk_Stripe",(x*1.1,0,0.02),(0.65,8,0.03),M["white"])
    elif typ == "parking":
        cube("Parking_Surface",(0,0,-0.08),(26,22,0.16),M["asphalt"])
        for x in range(-10,11,4): cube("Parking_Line",(x,0,0.02),(0.08,9,0.025),M["white"])
    elif typ == "alley":
        cube("Alley",(0,0,-0.08),(28,6,0.16),M["asphalt"])
        for y in (-4,4): cube("Alley_Sidewalk",(0,y,0),(28,1.8,0.18),M["sidewalk"])
    elif typ == "highway":
        cube("Highway",(0,0,-0.08),(45,18,0.18),M["asphalt"])
        cube("Highway_Line",(0,0,0.03),(45,0.18,0.03),M["road_line"])
        for y in (-9,9): cube("Guardrail",(0,y,0.8),(45,0.18,1.5),M["metal"],0.03)
    elif typ == "ramp":
        cube("Ramp",(0,0,0.8),(30,8,1.6),M["asphalt"])
        bpy.context.object.rotation_euler[1]=math.radians(7)
    else:
        cube("Bridge_Deck",(0,0,2),(35,10,1.0),M["concrete"],0.04)
        for x in range(-15,16,5): cyl("Bridge_Pillar",(x,0,-2),0.65,8,M["concrete"])
        for y in (-5,5): cube("Bridge_Railing",(0,y,3),(35,0.15,1.3),M["metal"],0.02)
    return 1


def park_asset(index: int):
    r = random.Random(SEED + 6000 + index * 191)
    types=["neighborhood","large_city","waterfront","dog","playground","basketball","tennis","skate","memorial","botanical","plaza","cemetery"]
    typ=types[index%len(types)]
    w,d=r.uniform(18,36),r.uniform(16,32)
    cube("Park_Ground",(0,0,-0.08),(w,d,0.16),M["grass"])
    if typ=="waterfront": cube("Water",(w*0.2,0,0),(w*0.35,d*0.8,0.12),M["water"])
    if typ=="basketball":
        cube("Court",(0,0,0.04),(14,9,0.1),M["concrete"])
        for x in (-6.2,6.2):
            cyl("Hoop_Post",(x,0,1.8),0.08,3.6,M["metal"])
            cyl("Hoop",(x,0,3.4),0.35,0.08,M["orange"])
    elif typ=="tennis": cube("Tennis_Court",(0,0,0.04),(20,10,0.1),M["green"])
    elif typ=="playground":
        cube("Play_Surface",(0,0,0.05),(12,9,0.1),M["dirt"])
        for x in (-4,4): cube("Playground_Post",(x,0,1.8),(0.2,0.2,3.6),M["metal"])
        cube("Playground_Bar",(0,0,3.4),(8,0.2,0.2),M["metal"])
    elif typ=="plaza":
        cube("Plaza_Paving",(0,0,0.04),(w*0.75,d*0.75,0.1),M["sidewalk"])
        cyl("Plaza_Fountain",(0,0,0.45),2.2,0.8,M["water"])
    elif typ=="cemetery":
        for x in range(-int(w/3),int(w/3)+1,3):
            for y in range(-int(d/3),int(d/3)+1,3):
                cube("Grave",(x,y,0.45),(0.8,0.25,0.9),M["concrete_light"],0.02)
    else:
        for _ in range(7 + index%5):
            x=r.uniform(-w/2+2,w/2-2); y=r.uniform(-d/2+2,d/2-2)
            tree(x,y,r.uniform(0.75,1.3))
    for _ in range(4):
        x=r.uniform(-w/2+2,w/2-2); y=r.uniform(-d/2+2,d/2-2)
        bench(x,y)
    return 1


def tree(x=0,y=0,s=1.0):
    cyl("Tree_Trunk",(x,y,1.8*s),0.25*s,3.6*s,M["wood"],12)
    sphere("Tree_Canopy",(x,y,4.1*s),(1.5*s,1.5*s,1.6*s),M["green"])


def bench(x=0,y=0):
    cube("Bench_Seat",(x,y,0.75),(2.0,0.55,0.16),M["wood_light"],0.025)
    for dx in (-0.7,0.7): cube("Bench_Leg",(x+dx,y,0.35),(0.12,0.45,0.7),M["metal"])
    cube("Bench_Back",(x,y+0.23,1.25),(2.0,0.12,0.9),M["wood_light"],0.02)


def prop_asset(index: int):
    r=random.Random(SEED+7000+index*211)
    kinds=["streetlight","trafficlight","hydrant","bin","dumpster","bench","mailbox","atm","parking_meter","bus_stop","utility_pole","billboard","phone_booth","vending_machine","road_sign","barrier","cone","manhole","transformer","newspaper_box","shopping_cart","pallet","crate","barrel","fence","gate","bollard","planter","fountain","statue","bench_table","umbrella"]
    k=kinds[index%len(kinds)]
    if k=="streetlight":
        cyl("Pole",(0,0,3.2),0.09,6.4,M["metal"]); cyl("Lamp",(0.75,0,6.25),0.12,1.5,M["metal"],rotation=(0,math.pi/2,0)); sphere("Light",(1.5,0,6.25),(0.22,0.22,0.12),M["yellow"])
    elif k=="trafficlight":
        cyl("Pole",(0,0,2.5),0.1,5,M["metal"]); cube("Signal",(0,-0.1,4.6),(0.45,0.3,1.5),M["black"],0.03)
        for z in (4.2,4.6,5.0): sphere("Signal_Lamp",(0,-0.28,z),(0.1,0.05,0.1),[M["red"],M["yellow"],M["green"]][int((z-4.2)*5)])
    elif k=="hydrant": cyl("Hydrant",(0,0,0.65),0.3,1.3,M["red"]); sphere("Hydrant_Cap",(0,0,1.35),(0.35,0.35,0.18),M["red"])
    elif k=="bin": cube("Trash_Bin",(0,0,0.65),(0.8,0.7,1.3),M["green"],0.06)
    elif k=="dumpster": cube("Dumpster",(0,0,0.9),(2.4,1.5,1.8),M["green"],0.08)
    elif k=="bench": bench()
    elif k=="mailbox": cube("Mailbox",(0,0,1.1),(0.8,0.55,0.9),M["blue"],0.08); cyl("Post",(0,0,0.5),0.07,1,M["metal"])
    elif k=="atm": cube("ATM",(0,0,1.1),(0.8,0.5,1.8),M["blue"],0.04); cube("Screen",(0,-0.27,1.35),(0.35,0.03,0.25),M["glass"])
    elif k=="parking_meter": cyl("Meter",(0,0,0.85),0.09,1.7,M["metal"]); cube("Meter_Head",(0,0,1.65),(0.3,0.2,0.3),M["metal"],0.03)
    elif k=="bus_stop": cube("Shelter_Roof",(0,0,2.7),(3.5,1.7,0.2),M["metal"]); cube("Shelter_Back",(0,0.75,1.4),(3.5,0.08,2.6),M["glass"])
    elif k=="utility_pole": cyl("Pole",(0,0,4),0.18,8,M["wood"]); cube("Crossbar",(0,0,7),(3,0.18,0.16),M["wood"])
    elif k=="billboard": cube("Board",(0,0,4),(5,0.25,2.5),M["blue"],0.04); cyl("Support",(0,0,1.7),0.15,3.4,M["metal"])
    elif k=="phone_booth": cube("Booth",(0,0,1.4),(1.2,1.0,2.8),M["glass"],0.03); cube("Roof",(0,0,2.9),(1.3,1.1,0.15),M["metal"])
    elif k=="vending_machine": cube("Machine",(0,0,1.0),(0.8,0.5,2),M["blue"],0.03); cube("Display",(0,-0.27,1.35),(0.45,0.03,0.6),M["glass"])
    elif k=="road_sign": cyl("Post",(0,0,1.4),0.06,2.8,M["metal"]); cube("Sign",(0,0,2.7),(1.5,0.08,0.7),M["blue"],0.02)
    elif k in {"barrier","gate","fence"}: cube("Barrier",(0,0,0.8),(3.0,0.15,1.0),M["orange" if k=="barrier" else "metal"],0.02)
    elif k=="cone": cone("Traffic_Cone",(0,0,0.45),0.35,0.06,0.9,M["orange"])
    elif k=="manhole": cyl("Manhole",(0,0,0.03),0.7,0.06,M["metal"],24)
    elif k=="transformer": cube("Transformer",(0,0,1.1),(1.5,1.2,2.2),M["metal_dark"],0.05)
    elif k=="newspaper_box": cube("News_Box",(0,0,0.9),(0.7,0.5,1.5),M["red"],0.03)
    elif k=="shopping_cart": cube("Cart",(0,0,0.7),(1.3,0.8,0.8),M["metal"],0.02)
    elif k in {"pallet","crate","barrel"}: cube(k.title(),(0,0,0.4),(1.4,1.0,0.8),M["wood"],0.03)
    elif k=="planter": cube("Planter",(0,0,0.45),(1.4,1.4,0.9),M["concrete_light"],0.05); sphere("Plant",(0,0,1.2),(0.6,0.6,0.8),M["green"])
    elif k=="fountain": cyl("Basin",(0,0,0.25),1.5,0.5,M["stone" if "stone" in M else "concrete_light"]); cyl("Water",(0,0,0.52),1.15,0.08,M["water"])
    elif k=="statue": cyl("Pedestal",(0,0,0.6),0.8,1.2,M["stone" if "stone" in M else "concrete_light"]); sphere("Statue",(0,0,1.7),(0.55,0.45,1.0),M["metal"])
    elif k=="bench_table": bench(); cube("Table",(0,1.8,0.9),(2,1,0.15),M["wood"])
    elif k=="umbrella": cyl("Umbrella_Pole",(0,0,1.7),0.06,3.4,M["metal"]); cone("Umbrella",(0,0,3.4),1.5,0,0.6,M["red"])
    return 1


def vegetation_asset(index: int):
    r=random.Random(SEED+8000+index*223)
    k=index%6
    if k==0: tree(0,0,r.uniform(0.8,1.5))
    elif k==1:
        cyl("Palm_Trunk",(0,0,2.7),0.22,5.4,M["wood"])
        for a in range(7):
            ang=a*math.tau/7
            cube("Palm_Leaf",(math.cos(ang)*1.5,math.sin(ang)*1.5,5.5),(3,0.22,0.12),M["green"]).rotation_euler[2]=ang
    elif k==2: sphere("Shrub",(0,0,0.8),(1.3,1.0,0.9),M["green"])
    elif k==3: sphere("Small_Bush",(0,0,0.5),(0.8,0.7,0.55),M["green"])
    elif k==4: cube("Hedge",(0,0,0.7),(3,1,1.4),M["green"],0.15)
    else:
        for x in (-0.8,0,0.8):
            sphere("Flower_Bush",(x,0,0.35),(0.45,0.45,0.35),[M["red"],M["yellow"],M["purple"]][x.__hash__()%3])
    return 1


def vehicle_asset(index: int):
    r=random.Random(SEED+9000+index*239)
    types=["sedan","suv","pickup","sports","luxury","van","taxi","police","ambulance","fire","delivery","bus","motorcycle","bicycle"]
    k=types[index%len(types)]
    length={"sedan":4.6,"suv":4.9,"pickup":5.2,"sports":4.4,"luxury":5.0,"van":5.0,"taxi":4.6,"police":4.8,"ambulance":6.0,"fire":7.0,"delivery":6.0,"bus":11.0,"motorcycle":2.3,"bicycle":1.8}[k]
    width=1.9 if k not in {"bus","fire"} else 2.5
    height=1.4 if k in {"sedan","sports","luxury","taxi","police"} else 2.1
    bodymat=[M["blue"],M["red"],M["white"],M["black"],M["green"]][index%5]
    cube("Vehicle_Body",(0,0,height*0.55),(length,width,height),bodymat,0.14)
    cube("Vehicle_Cabin",(0,0,height*1.15),(length*0.52,width*0.82,height*0.85),M["glass_dark"],0.12)
    if k in {"motorcycle","bicycle"}:
        # Replace the car-like body visually with a simple two-wheel layout.
        clear_vehicle_meshes()
        cyl("Frame",(0,0,0.9),0.06,length*0.55,M["metal"] ,12,rotation=(0,math.pi/2,0))
        for x in (-length*0.38,length*0.38): cyl("Wheel",(x,0,0.65),0.45,0.16,M["black"],16,rotation=(math.pi/2,0,0))
        return 1
    for x in (-length*0.36,length*0.36):
        for y in (-width*0.58,width*0.58):
            cyl("Wheel",(x,y,0.52),0.36,0.18,M["black"],16,rotation=(math.pi/2,0,0))
    cube("Front_Lights",(length*0.51,0,0.85),(0.08,width*0.45,0.28),M["white"],0.03)
    cube("Rear_Lights",(-length*0.51,0,0.85),(0.08,width*0.45,0.25),M["red"],0.03)
    if k in {"police","taxi"}:
        cube("Roof_Sign",(0,0,height*1.7),(1.2,0.35,0.25),M["blue" if k=="police" else "yellow"],0.04)
    if k=="pickup": cube("Pickup_Bed",(-length*0.25,0,height*0.95),(length*0.42,width*0.8,0.5),M["metal_dark"],0.04)
    if k=="ambulance": cube("Emergency_Box",(-length*0.05,0,height*1.2),(length*0.5,width*0.8,height*0.9),M["white"],0.08)
    if k=="bus":
        for x in (-3.5,-1.2,1.2,3.5): cube("Bus_Window",(x,-width*0.51,2.2),(1.3,0.05,1.0),M["glass"])
    return 2


def clear_vehicle_meshes():
    for o in list(bpy.context.scene.objects):
        if o.type == "MESH":
            bpy.data.objects.remove(o, do_unlink=True)


# ============================================================
# SAVE / GENERATION
# ============================================================

def safe_filename(name: str):
    return "".join(c if c.isalnum() or c in "_-" else "_" for c in name)


def save_asset(asset_name: str, category: str):
    root = OUTPUT_ROOT / category
    root.mkdir(parents=True, exist_ok=True)
    path = root / f"{safe_filename(asset_name)}.blend"
    if path.exists() and not OVERWRITE:
        manifest.append({"name": asset_name, "category": category, "file": str(path), "status": "existing"})
        return
    root_obj = join_all(asset_name)
    if root_obj is None:
        manifest.append({"name": asset_name, "category": category, "file": str(path), "status": "empty"})
        return
    root_obj["groktastic_asset"] = True
    root_obj["asset_category"] = category
    root_obj["asset_name"] = asset_name
    bpy.context.scene["asset_name"] = asset_name
    bpy.context.scene["asset_category"] = category
    bpy.ops.wm.save_as_mainfile(filepath=str(path), check_existing=False)
    manifest.append({"name": asset_name, "category": category, "file": str(path), "status": "generated"})


def generate_category(category, count, generator, prefix):
    for i in range(count):
        clear_scene()
        asset_name = f"{prefix}_{i+1:03d}"
        try:
            generator(i)
            save_asset(asset_name, category)
        except Exception as exc:
            print(f"[GROKTASTIC] FAILED {asset_name}: {exc}")
            manifest.append({"name": asset_name, "category": category, "file": "", "status": f"ERROR: {exc}"})
        finally:
            clear_scene()


def write_manifest():
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    manifest_path = OUTPUT_ROOT / "ASSET_MANIFEST.csv"
    with manifest_path.open("w", encoding="utf-8") as f:
        f.write("name,category,file,status\n")
        for row in manifest:
            vals=[row.get("name",""),row.get("category",""),row.get("file",""),row.get("status","")]
            f.write(",".join('"'+v.replace('"','""')+'"' for v in vals)+"\n")
    readme = OUTPUT_ROOT / "README.txt"
    readme.write_text(
        "GROKTASTIC PROCEDURAL ASSET LIBRARY\n\n"
        "Generated one asset at a time for low peak RAM usage.\n"
        "Every .blend is intentionally standalone.\n"
        "Assets are original procedural geometry intended as a foundation for the game.\n",
        encoding="utf-8",
    )


def main():
    print("=" * 60)
    print("GROKTASTIC ASSET FACTORY - START")
    print(f"Quality: {QUALITY}")
    print(f"Output: {OUTPUT_ROOT}")
    print("Assets are generated sequentially to reduce RAM pressure.")
    print("=" * 60)
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)

    generate_category("Buildings/Residential", COUNTS["residential"], residential_asset, "Building_Residential")
    generate_category("Buildings/Commercial", COUNTS["commercial"], commercial_asset, "Building_Commercial")
    generate_category("Buildings/Landmarks", COUNTS["landmarks"], landmark_asset, "Building_Landmark")
    generate_category("Buildings/Industrial", COUNTS["industrial"], industrial_asset, "Building_Industrial")
    generate_category("Buildings/Services", COUNTS["service"], service_asset, "Building_Service")
    generate_category("Roads", COUNTS["roads"], road_asset, "Road")
    generate_category("Parks", COUNTS["parks"], park_asset, "Park")
    generate_category("Street_Props", COUNTS["street_props"], prop_asset, "Prop_Street")
    generate_category("Vegetation", COUNTS["vegetation"], vegetation_asset, "Vegetation")
    generate_category("Vehicles", COUNTS["vehicles"], vehicle_asset, "Vehicle")
    write_manifest()

    clear_scene()
    print("=" * 60)
    print(f"DONE. Generated/processed {len(manifest)} assets.")
    print(f"Library: {OUTPUT_ROOT}")
    print("=" * 60)


if __name__ == "__main__":
    main()
