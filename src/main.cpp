#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <string>
#include <vector>

namespace {
constexpr int W = 1600;
constexpr int H = 900;
constexpr float WORLD = 360.0f;
constexpr float ROAD = 18.0f;
constexpr float BLOCK = 54.0f;
constexpr float PI2 = 6.28318530718f;

float clampf(float v, float a, float b) { return std::max(a, std::min(v, b)); }
float frand(float a, float b) { return a + (b - a) * (static_cast<float>(std::rand()) / RAND_MAX); }
Vector3 v3(float x, float y, float z) { return {x, y, z}; }
Color alpha(Color c, unsigned char a) { c.a = a; return c; }

struct Vehicle {
    Vector3 p{};
    float yaw = 0;
    float speed = 0;
    float health = 100;
    bool occupied = false;
    bool police = false;
    Color body{200, 60, 55, 255};
};

struct Ped {
    Vector3 p{};
    Vector3 target{};
    float phase = 0;
    float hp = 40;
    bool alive = true;
    bool hostile = false;
};

struct Mission {
    std::string name;
    std::string objective;
    Vector3 target{};
    float radius = 6;
    bool active = false;
    bool complete = false;
    float timer = 0;
};

struct Pickup { Vector3 p{}; bool alive = true; int value = 100; };

struct Game {
    Vector3 player{0, 1, 20};
    float playerYaw = 0;
    float playerPitch = 0.28f;
    float health = 100;
    float armor = 25;
    int cash = 500;
    int wanted = 0;
    int missionIndex = 0;
    bool inCar = false;
    bool paused = false;
    bool aiming = false;
    float shootCooldown = 0;
    float muzzle = 0;
    float missionBanner = 5;
    float damageFlash = 0;
    float time = 0;
    float day = 0.35f;
    std::vector<Vehicle> cars;
    std::vector<Ped> peds;
    std::vector<Pickup> pickups;
    std::vector<Mission> missions;
    Vehicle* car = nullptr;
};

void AddMission(Game& g, const char* name, const char* objective, Vector3 target, float radius) {
    g.missions.push_back({name, objective, target, radius, false, false, 0});
}

void GenerateWorld(Game& g) {
    std::srand(1337);
    g.cars.clear(); g.peds.clear(); g.pickups.clear();
    const Color carColors[] = {{34,120,220,255},{220,55,55,255},{238,174,48,255},{55,185,110,255},{160,70,190,255},{225,225,225,255}};
    for (int i = 0; i < 42; ++i) {
        bool horizontal = (i % 2) == 0;
        float lane = (static_cast<int>(i / 2) % 9 - 4) * 36.0f + (horizontal ? 7.0f : -7.0f);
        float along = frand(-160, 160);
        Vehicle c;
        c.p = horizontal ? v3(along, 1.1f, lane) : v3(lane, 1.1f, along);
        c.yaw = horizontal ? ((i % 4 < 2) ? 0 : PI2 * 0.5f) : ((i % 4 < 2) ? PI2 * 0.5f : 0);
        c.speed = frand(6, 13);
        c.body = carColors[i % 6];
        c.police = i < 5;
        if (c.police) c.body = {235,235,245,255};
        g.cars.push_back(c);
    }
    for (int i = 0; i < 95; ++i) {
        Ped p;
        p.p = v3(frand(-165,165), 0, frand(-165,165));
        p.target = v3(clampf(p.p.x + frand(-35,35), -175,175), 0, clampf(p.p.z + frand(-35,35), -175,175));
        p.phase = frand(0, PI2);
        p.hostile = (i % 17 == 0);
        g.peds.push_back(p);
    }
    for (int i = 0; i < 28; ++i) g.pickups.push_back({v3(frand(-150,150), 1.0f, frand(-150,150)), true, 50 + (i%5)*50});
    AddMission(g, "HOT PACKAGE", "Take the package before the rival crew reaches it.", v3(-104,1,-102), 7);
    AddMission(g, "CLEAN GETAWAY", "Deliver the package to the safehouse.", v3(132,1,96), 9);
    AddMission(g, "HEAT CHECK", "Survive the police response for 45 seconds.", v3(0,1,0), 999);
}

float GroundHeight(float x, float z) {
    return 0.12f * std::sin(x * 0.025f) * std::cos(z * 0.018f);
}

void DrawRoads() {
    for (int i = -3; i <= 3; ++i) {
        float p = i * 54.0f;
        DrawCube(v3(0,0,p), WORLD, 0.08f, ROAD, {38,42,48,255});
        DrawCube(v3(p,0,0), ROAD, 0.08f, WORLD, {38,42,48,255});
        for (float q=-170; q<170; q+=18) {
            DrawCube(v3(q,0.07f,p), 7,0.03f,0.22f, {190,172,92,190});
            DrawCube(v3(p,0.07f,q), 0.22f,0.03f,7, {190,172,92,190});
        }
    }
}

void DrawBuilding(float x, float z, float w, float d, float h, Color c, int seed) {
    float y = h * 0.5f;
    DrawCube(v3(x,y,z), w,h,d,c);
    DrawCubeWires(v3(x,y,z),w,h,d,alpha({20,24,31,255},120));
    for (int row=0; row<std::max(1,static_cast<int>(h/5)); ++row) {
        float yy = 2.4f + row*5.0f;
        if (yy > h-1.0f) break;
        for (int col=0; col<std::max(1,static_cast<int>(w/4)); ++col) {
            if (((col*7 + row*13 + seed) % 5) == 0) continue;
            float xx = x - w*0.5f + 2.0f + col*4.0f;
            DrawCube(v3(xx,yy,z-d*0.501f),1.35f,1.35f,0.08f,alpha({105,190,220,255},static_cast<unsigned char>(150+((seed+row+col)%90))));
        }
    }
    DrawCube(v3(x,h+0.5f,z), w*0.82f,0.7f,d*0.82f, {27,31,40,255});
}

void DrawWorld(float t) {
    DrawPlane(v3(0,0,0), {WORLD,WORLD}, {24,31,38,255});
    DrawRoads();
    for (int bx=-4; bx<=4; ++bx) for (int bz=-4; bz<=4; ++bz) {
        float x=bx*54, z=bz*54;
        if (std::abs(bx)<=3 && std::abs(bz)<=3 && (bx==0 || bz==0)) continue;
        if ((bx+bz)%5==0) {
            DrawCube(v3(x,0.12f,z), BLOCK-5,0.18f,BLOCK-5,{40,74,53,255});
            for(int k=0;k<4;k++) DrawCube(v3(x+frand(-18,18),1.0f,z+frand(-18,18)),1.0f,2.0f,1.0f,{34,98,52,255});
        } else {
            float h=frand(9,34); float w=frand(25,45), d=frand(25,45);
            DrawBuilding(x+frand(-5,5),z+frand(-5,5),w,d,h,{static_cast<unsigned char>(42+bx*3+40),static_cast<unsigned char>(48+bz*2+30),static_cast<unsigned char>(60+((bx*bz+8)*3)),255}, bx*31+bz*17);
        }
    }
    // Landmark tower and radio mast
    DrawCylinder(v3(-108,19,-102), 11, 7, 38, 16, {52,61,75,255});
    DrawCylinder(v3(-108,40,-102), 0.8f,0.35f,22,8,{170,180,192,255});
    DrawSphere(v3(-108,52,-102),1.4f,{245,72,72,255});
    // Street lamps
    for (int i=-3;i<=3;i++) for(int k=-3;k<=3;k++) {
        float x=i*54+11,z=k*54+11;
        DrawCylinder(v3(x,2.8f,z),0.14f,0.14f,5.5f,8,{50,54,61,255});
        DrawSphere(v3(x,5.7f,z),0.38f,{255,210,130,255});
    }
    // distant skyline glow
    for(int i=0;i<14;i++) {
        float x=-175+i*27;
        float h=frand(18,52);
        DrawCube(v3(x,h*0.5f,-178),20,h,10,{30,38,53,255});
        DrawCube(v3(x,h*0.72f,-183),14,1,0.12f,{90,150,190,110});
    }
    (void)t;
}

void DrawVehicle(const Vehicle& c, float time) {
    Vector3 p=c.p;
    float glow = c.police ? (0.5f+0.5f*std::sin(time*12)) : 0;
    DrawCube(p+v3(0,0.65f,0),3.9f,1.0f,7.2f,c.body);
    DrawCube(p+v3(0,1.25f,0.25f),3.0f,0.85f,3.6f,{38,48,60,255});
    DrawCube(p+v3(0,1.28f,-1.55f),2.65f,0.55f,1.15f,{95,145,165,255});
    DrawCube(p+v3(0,1.28f,2.05f),2.65f,0.55f,1.15f,{50,70,83,255});
    for(int s=-1;s<=1;s+=2) for(int z=-1;z<=1;z+=2) {
        DrawCylinder(p+v3(s*2.0f,0.48f,z*2.3f),0.62f,0.62f,0.35f,12,{18,20,24,255});
        DrawCylinder(p+v3(s*2.0f,0.48f,z*2.3f),0.28f,0.28f,0.37f,12,{80,84,90,255});
    }
    DrawCube(p+v3(-1.75f,1.0f,-3.45f),0.7f,0.32f,0.1f,{255,220,150,255});
    DrawCube(p+v3(1.75f,1.0f,-3.45f),0.7f,0.32f,0.1f,{255,220,150,255});
    if(c.police){ DrawCube(p+v3(0,1.8f,0),1.2f,0.28f,0.55f,{220,220,225,255}); DrawCube(p+v3(-0.38f,1.98f,0),0.28f,0.12f,0.55f,alpha({235,50,65,255},static_cast<unsigned char>(100+glow*155))); DrawCube(p+v3(0.38f,1.98f,0),0.28f,0.12f,0.55f,alpha({55,105,245,255},static_cast<unsigned char>(100+(1-glow)*155))); }
}

void DrawPed(const Ped& p) {
    if(!p.alive) return;
    Color shirt=p.hostile?Color{205,65,62,255}:Color{65,105,150,255};
    DrawCylinder(p.p+v3(0,1.0f,0),0.36f,0.48f,1.4f,8,shirt);
    DrawSphere(p.p+v3(0,1.95f,0),0.33f,{190,145,112,255});
    DrawCube(p.p+v3(-0.18f,0.35f,0),0.15f,0.7f,0.15f,{34,38,45,255});
    DrawCube(p.p+v3(0.18f,0.35f,0),0.15f,0.7f,0.15f,{34,38,45,255});
}

Vector3 Forward(float yaw, float pitch=0) {
    return Vector3Normalize(v3(std::sin(yaw)*std::cos(pitch), -std::sin(pitch), std::cos(yaw)*std::cos(pitch)));
}

void CameraUpdate(Game& g, Camera3D& cam) {
    Vector3 target = g.inCar && g.car ? g.car->p+v3(0,1.2f,0) : g.player+v3(0,1.5f,0);
    Vector3 f=Forward(g.playerYaw,g.playerPitch*0.55f);
    float distance=g.inCar?12.0f:8.2f;
    cam.target=target;
    cam.position=target-f*distance+v3(0,2.2f,0);
    cam.up={0,1,0}; cam.fovy=63; cam.projection=CAMERA_PERSPECTIVE;
}

void UpdateTraffic(Game& g,float dt) {
    for(auto& c:g.cars){
        if(c.occupied) continue;
        Vector3 dir=v3(std::sin(c.yaw),0,std::cos(c.yaw));
        c.p += dir*c.speed*dt;
        if(c.p.x>180)c.p.x=-180; if(c.p.x<-180)c.p.x=180;
        if(c.p.z>180)c.p.z=-180; if(c.p.z<-180)c.p.z=180;
        for(const auto& o:g.cars) if(&o!=&c && Vector3Distance(c.p,o.p)<6){ c.speed=std::max(4.0f,c.speed-8*dt); }
        c.speed=clampf(c.speed+(frand(-1,1))*dt,5,14);
    }
}

void UpdatePeds(Game& g,float dt){
    for(auto& p:g.peds){
        if(!p.alive) continue;
        Vector3 d=p.target-p.p; d.y=0;
        if(Vector3Length(d)<2){p.target=v3(frand(-165,165),0,frand(-165,165)); d=p.target-p.p; d.y=0;}
        if(Vector3Length(d)>0.01f) p.p+=Vector3Normalize(d)*(p.hostile?1.9f:1.25f)*dt;
        p.p.y=GroundHeight(p.p.x,p.p.z);
    }
}

void StartNextMission(Game& g){
    if(g.missionIndex>=static_cast<int>(g.missions.size())) return;
    auto& m=g.missions[g.missionIndex]; m.active=true; m.timer=0; g.missionBanner=5;
}

void Shoot(Game& g, Camera3D& cam){
    if(g.shootCooldown>0) return;
    g.shootCooldown=0.16f; g.muzzle=0.08f;
    Ray ray=GetMouseRay(GetMousePosition(),cam);
    float best=1000; Ped* hit=nullptr;
    for(auto& p:g.peds){
        if(!p.alive) continue;
        RayCollision h=GetRayCollisionSphere(ray,p.p+v3(0,1,0),0.55f);
        if(h.hit && h.distance<best){best=h.distance;hit=&p;}
    }
    if(hit){hit->hp-=35; if(hit->hp<=0){hit->alive=false;g.cash+=25;} g.wanted=std::min(5,g.wanted+1);}
}

void EnterExit(Game& g){
    if(g.inCar){
        if(!g.car)return;
        g.player=g.car->p+v3(4,0,0); g.car->occupied=false; g.car=nullptr; g.inCar=false; return;
    }
    float best=5; Vehicle* near=nullptr;
    for(auto& c:g.cars){float d=Vector3Distance(g.player,c.p); if(d<best){best=d;near=&c;}}
    if(near){g.inCar=true;g.car=near;near->occupied=true;g.player=near->p;}
}

void Update(Game& g, float dt, Camera3D& cam){
    g.time+=dt; g.shootCooldown=std::max(0.0f,g.shootCooldown-dt); g.muzzle=std::max(0.0f,g.muzzle-dt); g.damageFlash=std::max(0.0f,g.damageFlash-dt); g.missionBanner=std::max(0.0f,g.missionBanner-dt);
    if(IsKeyPressed(KEY_ESCAPE)){g.paused=!g.paused; if(g.paused)EnableCursor();else DisableCursor();}
    if(g.paused)return;
    Vector2 md=GetMouseDelta();
    if(!IsCursorHidden()) DisableCursor();
    g.playerYaw += md.x*0.0028f;
    g.playerPitch=clampf(g.playerPitch-md.y*0.0022f,-0.7f,0.9f);
    g.aiming=IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    if(IsKeyPressed(KEY_E)) EnterExit(g);
    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) Shoot(g,cam);
    if(IsKeyPressed(KEY_R)) g.muzzle=0;

    UpdateTraffic(g,dt); UpdatePeds(g,dt);
    if(g.inCar && g.car){
        Vehicle& c=*g.car;
        float throttle=(IsKeyDown(KEY_W)?1:0)-(IsKeyDown(KEY_S)?1:0);
        float steer=(IsKeyDown(KEY_D)?1:0)-(IsKeyDown(KEY_A)?1:0);
        float maxSpeed=IsKeyDown(KEY_LEFT_SHIFT)?31:23;
        c.speed += throttle*26*dt;
        if(std::abs(throttle)<0.1f)c.speed*=std::pow(0.04f,dt);
        c.speed=clampf(c.speed,-10,maxSpeed);
        c.yaw += steer*dt*(1.9f+std::abs(c.speed)*0.04f)*(c.speed>=0?1:-1);
        c.p += v3(std::sin(c.yaw),0,std::cos(c.yaw))*c.speed*dt;
        c.p.x=clampf(c.p.x,-176,176); c.p.z=clampf(c.p.z,-176,176); c.p.y=GroundHeight(c.p.x,c.p.z)+1.0f;
        g.player=c.p;
        if(IsKeyPressed(KEY_SPACE)) c.speed*=0.88f;
    } else {
        Vector3 f=Forward(g.playerYaw,0); Vector3 r=v3(f.z,0,-f.x); Vector3 move{};
        if(IsKeyDown(KEY_W))move+=f; if(IsKeyDown(KEY_S))move-=f; if(IsKeyDown(KEY_D))move+=r; if(IsKeyDown(KEY_A))move-=r;
        float sp=IsKeyDown(KEY_LEFT_SHIFT)?11:6.3f;
        if(Vector3Length(move)>0.01f)g.player+=Vector3Normalize(move)*sp*dt;
        if(IsKeyPressed(KEY_SPACE) && g.player.y<1.6f) g.player.y=4.4f;
        g.player.y += (g.player.y>1.0f? -15.0f:0)*dt; if(g.player.y<1.0f)g.player.y=1.0f;
        g.player.x=clampf(g.player.x,-176,176);g.player.z=clampf(g.player.z,-176,176);
    }
    // Collisions and pickups.
    for(auto& p:g.pickups) if(p.alive && Vector3Distance(g.player,p.p)<2.2f){p.alive=false;g.cash+=p.value;}
    for(const auto& p:g.peds) if(p.alive && p.hostile && Vector3Distance(g.player,p.p)<4.5f && g.wanted>0){
        if(frand(0,1)<dt*0.45f){ if(g.armor>0)g.armor-=7;else g.health-=7;g.damageFlash=.15f; }
    }
    // Wanted decay only when the player is not causing fresh trouble.
    if(g.wanted>0 && g.time>8 && std::fmod(g.time,12.0f)<dt) g.wanted--;
    if(g.health<=0){g.health=100;g.armor=25;g.cash=std::max(0,g.cash-250);g.wanted=0;g.player=v3(0,1,20);g.inCar=false;g.car=nullptr;}

    if(g.missionIndex<static_cast<int>(g.missions.size())){
        Mission& m=g.missions[g.missionIndex];
        if(!m.active && !m.complete)StartNextMission(g);
        if(m.active){
            m.timer+=dt;
            if(g.missionIndex==2){
                if(m.timer>=45){m.complete=true;m.active=false;g.cash+=1500;g.missionIndex++;g.missionBanner=6;}
            } else if(Vector3Distance(g.player,m.target)<m.radius){
                m.complete=true;m.active=false;g.cash+=1000;g.wanted=std::max(0,g.wanted-1);g.missionIndex++;g.missionBanner=6;
            }
        }
    }
}

void DrawUI(const Game& g, int fps){
    DrawRectangle(0,0,W,64,alpha({8,11,17,255},205));
    DrawText("GROKTASTIC",28,18,26,{235,239,245,255});
    DrawText("CITY // AFTER DARK",225,22,16,{110,170,190,255});
    DrawText(TextFormat("$ %06d",g.cash),W-235,18,24,{115,230,160,255});
    for(int i=0;i<5;i++) DrawText(i<g.wanted?"★":"☆",W-245+i*28,48,20,i<g.wanted?Color{245,205,70,255}:Color{95,100,110,255});
    DrawRectangle(28,H-66,260,12,{35,39,45,255});DrawRectangle(28,H-66,260*(g.health/100),12,{220,66,68,255});
    DrawRectangle(28,H-46,260,9,{35,39,45,255});DrawRectangle(28,H-46,260*clampf(g.armor/100,0,1),9,{80,150,215,255});
    DrawText("HEALTH",295,H-70,14,{170,178,188,255});DrawText("ARMOR",295,H-50,14,{170,178,188,255});
    if(g.inCar)DrawText("E  EXIT VEHICLE   |   SPACE  HANDBRAKE",W/2-190,H-52,14,{210,214,220,255});
    else DrawText("WASD MOVE   SHIFT SPRINT   E ENTER CAR   LMB FIRE",W/2-240,H-52,14,{210,214,220,255});
    // minimap
    int mx=W-205,my=H-205;DrawRectangle(mx,my,180,180,{12,17,23,220});
    for(int i=-3;i<=3;i++){int x=mx+90+i*27;DrawRectangle(x,my,5,180,{40,45,51,255});DrawRectangle(mx,my+90+i*27,180,5,{40,45,51,255});}
    float px=mx+90+g.player.x/2,py=my+90+g.player.z/2;DrawCircle(static_cast<int>(px),static_cast<int>(py),5,{90,220,150,255});
    if(g.missionIndex<static_cast<int>(g.missions.size())){auto& m=g.missions[g.missionIndex];float tx=mx+90+m.target.x/2,ty=my+90+m.target.z/2;DrawCircle(static_cast<int>(tx),static_cast<int>(ty),7,{245,175,70,255});}
    DrawText(TextFormat("%d FPS",fps),W-80,20,12,{120,130,140,255});
    if(g.missionIndex<static_cast<int>(g.missions.size())){
        const Mission&m=g.missions[g.missionIndex];
        DrawRectangle(W/2-270,78,540,72,alpha({10,14,21,255},g.missionBanner>0?235:135));
        DrawText(m.name.c_str(),W/2-245,90,21,{245,190,80,255});
        DrawText(m.objective.c_str(),W/2-245,118,15,{215,220,225,255});
    }
    if(g.aiming){DrawLine(W/2-9,H/2,W/2+9,H/2,{235,240,245,230});DrawLine(W/2,H/2-9,W/2,H/2+9,{235,240,245,230});}
    if(g.missionIndex>=static_cast<int>(g.missions.size())) DrawText("CITY MASTERED",W/2-130,92,25,{115,230,160,255});
    if(g.paused){DrawRectangle(0,0,W,H,alpha({3,5,8,255},185));DrawText("PAUSED",W/2-72,H/2-30,42,{240,242,245,255});DrawText("ESC TO RETURN",W/2-76,H/2+20,15,{160,170,180,255});}
}
}

int main(){
    SetConfigFlags(FLAG_MSAA_4X_HINT|FLAG_VSYNC_HINT|FLAG_WINDOW_RESIZABLE|FLAG_WINDOW_HIGHDPI);
    InitWindow(W,H,"GROKTASTIC // Open World Sandbox");
    SetTargetFPS(120);
    Game g; GenerateWorld(g);
    Camera3D cam{}; CameraUpdate(g,cam);
    RenderTexture2D scene=LoadRenderTexture(W,H);
    Shader post=LoadShader(0,"assets/post.fs");
    int timeLoc=GetShaderLocation(post,"time");
    DisableCursor();
    while(!WindowShouldClose()){
        float dt=std::min(GetFrameTime(),0.033f);
        Update(g,dt,cam); CameraUpdate(g,cam);
        BeginTextureMode(scene);ClearBackground({10,15,24,255});
        BeginMode3D(cam);
        float sun=0.25f+0.5f*std::sin(g.time*0.035f);
        Color sky={static_cast<unsigned char>(14+35*sun),static_cast<unsigned char>(20+45*sun),static_cast<unsigned char>(34+65*sun),255};
        DrawWorld(g.time);
        for(const auto& p:g.pickups) if(p.alive){DrawSphere(p.p+v3(0,0.5f,0),0.32f,{90,235,155,255});DrawRing(p.p+v3(0,0.5f,0),0.5f,0.65f,0,360,16,alpha({100,240,170,255},160));}
        for(const auto& p:g.peds)DrawPed(p);
        for(const auto& c:g.cars)DrawVehicle(c,g.time);
        if(!g.inCar){DrawCylinder(g.player+v3(0,1,0),0.42f,0.5f,1.65f,10,{66,130,180,255});DrawSphere(g.player+v3(0,2.05f,0),0.34f,{198,151,118,255});}
        if(g.missionIndex<static_cast<int>(g.missions.size()) && g.missionIndex!=2){Vector3 t=g.missions[g.missionIndex].target;DrawCylinder(t,1.5f,0.3f,0.3f,24,alpha({245,178,68,255},170));DrawRing(t+v3(0,0.2f,0),2.0f,2.3f,0,360,32,alpha({245,178,68,255},180));}
        if(g.muzzle>0){Vector3 f=Forward(g.playerYaw,g.playerPitch);Vector3 o=(g.inCar?g.car->p:g.player)+v3(0,1.5f,0)+f*2;DrawSphere(o,0.25f,{255,220,120,255});}
        DrawGrid(72,5.0f,{120,130,145,18});
        EndMode3D();EndTextureMode();
        BeginDrawing();ClearBackground(BLACK);
        SetShaderValue(post,timeLoc,&g.time,SHADER_UNIFORM_FLOAT);
        BeginShaderMode(post);DrawTextureRec(scene.texture,{0,0,(float)scene.texture.width,-(float)scene.texture.height},{0,0},WHITE);EndShaderMode();
        DrawUI(g,GetFPS());
        if(g.damageFlash>0)DrawRectangle(0,0,W,H,alpha({235,40,40,255},static_cast<unsigned char>(g.damageFlash*1100)));
        DrawRectangle(0,0,W,8,{8,11,16,230});DrawRectangle(0,H-8,W,8,{8,11,16,230});
        EndDrawing();
    }
    UnloadShader(post);UnloadRenderTexture(scene);CloseWindow();return 0;
}
