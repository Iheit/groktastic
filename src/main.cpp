#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

constexpr int SW = 1600;
constexpr int SH = 900;
constexpr float CITY = 360.0f;
constexpr float PI = 3.14159265359f;

template <typename T> T Clamp(T v, T a, T b) { return std::max(a, std::min(v, b)); }
float Rnd(float a, float b) { return a + (b - a) * (float(std::rand()) / float(RAND_MAX)); }
Vector3 V(float x, float y, float z) { return {x, y, z}; }
Vector3 Flat(Vector3 v) { v.y = 0; return v; }
float Dist(Vector3 a, Vector3 b) { return Vector3Length(Flat(a - b)); }

struct Station { Vector3 p; };
enum class PedState { Neutral, Fleeing, Dead };
struct Ped {
    Vector3 p{}, goal{};
    float hp = 100.0f;
    float speed = 1.35f;
    float panic = 0.0f;
    float facing = 0.0f;
    int style = 0;
    PedState state = PedState::Neutral;
};
struct Car {
    Vector3 p{};
    float yaw = 0.0f;
    float speed = 0.0f;
    float hp = 100.0f;
    bool player = false;
    bool police = false;
    bool dispatch = false;
    Color body{180, 50, 45, 255};
};
struct Mission {
    std::string name, text;
    Vector3 target{};
    float radius = 8.0f;
    float timer = 0.0f;
    bool active = false, done = false;
};
struct Game {
    Vector3 p{0, 1.0f, 20};
    float yaw = 0.0f, pitch = 0.16f;
    float time = 0.0f;
    float hp = 100.0f, armor = 30.0f;
    int cash = 500, wanted = 0, mission = 0;
    bool inCar = false, paused = false, aim = false;
    bool firstPerson = false;
    float shotCooldown = 0.0f, damageFlash = 0.0f;
    Car* vehicle = nullptr;
    std::vector<Car> cars;
    std::vector<Ped> peds;
    std::vector<Mission> missions;
    std::vector<Station> stations;
};

float Ground(float x, float z) { return 0.05f * sinf(x * 0.025f) * cosf(z * 0.02f); }
Vector3 Forward(float yaw, float pitch = 0.0f) {
    return Vector3Normalize(V(sinf(yaw) * cosf(pitch), -sinf(pitch), cosf(yaw) * cosf(pitch)));
}

void AddMission(Game& g, const char* name, const char* text, Vector3 target, float radius) {
    g.missions.push_back({name, text, target, radius});
}

void BuildGame(Game& g) {
    std::srand(1337);
    g.cars.clear(); g.peds.clear(); g.missions.clear();
    g.stations = {{V(-135,0,-135)}, {V(135,0,-135)}, {V(-135,0,135)}, {V(135,0,135)}};

    const Color carColors[] = {
        {38,76,112,255}, {155,43,38,255}, {174,118,42,255},
        {42,108,78,255}, {76,70,104,255}, {178,178,170,255},
        {54,54,58,255}, {120,120,126,255}
    };
    for (int i = 0; i < 52; ++i) {
        Car c;
        bool horizontal = i % 2 == 0;
        float lane = (i / 2 % 9 - 4) * 36.0f + (horizontal ? 6.0f : -6.0f);
        float q = Rnd(-170, 170);
        c.p = horizontal ? V(q, 1, lane) : V(lane, 1, q);
        c.yaw = horizontal ? (i % 4 < 2 ? 0.0f : PI) : (i % 4 < 2 ? PI * 0.5f : -PI * 0.5f);
        c.speed = Rnd(7, 14);
        c.body = i < 6 ? Color{205,210,214,255} : carColors[i % 8];
        c.police = i < 6;
        g.cars.push_back(c);
    }
    for (int i = 0; i < 135; ++i) {
        Ped p;
        p.p = V(Rnd(-166,166), 0, Rnd(-166,166));
        p.goal = V(Clamp(p.p.x + Rnd(-35,35), -174.0f, 174.0f), 0,
                   Clamp(p.p.z + Rnd(-35,35), -174.0f, 174.0f));
        p.style = i % 6;
        p.speed = Rnd(1.1f, 1.65f);
        g.peds.push_back(p);
    }
    AddMission(g, "HOT PACKAGE", "Reach the rooftop relay at the old tower.", V(-108,1,-102), 9);
    AddMission(g, "CLEAN GETAWAY", "Take the score to the safehouse.", V(132,1,96), 11);
    AddMission(g, "HEAT CHECK", "Survive the police response for 45 seconds.", V(0,1,0), 999);
}

void DrawRoads() {
    for (int i = -3; i <= 3; ++i) {
        float q = i * 54.0f;
        DrawCube(V(0,-0.02f,q), CITY, 0.16f, 19.0f, {31,34,38,255});
        DrawCube(V(q,-0.02f,0), 19.0f, 0.16f, CITY, {31,34,38,255});
        DrawCube(V(0,0.045f,q-9.2f), CITY, 0.03f, 0.11f, {165,145,82,210});
        DrawCube(V(q-9.2f,0.045f,0), 0.11f, 0.03f, CITY, {165,145,82,210});
        for (float s = -168; s < 168; s += 18) {
            DrawCube(V(s,0.06f,q), 7.0f, 0.035f, 0.16f, {205,193,146,190});
            DrawCube(V(q,0.06f,s), 0.16f, 0.035f, 7.0f, {205,193,146,190});
        }
    }
}

void DrawTree(Vector3 p, float scale) {
    DrawCylinder(p + V(0,1.6f*scale,0), 0.28f*scale, 0.38f*scale, 3.2f*scale, 8, {74,53,38,255});
    DrawSphere(p + V(0,4.0f*scale,0), 1.65f*scale, {42,78,51,255});
    DrawSphere(p + V(-0.7f,4.25f*scale,0.25f), 1.05f*scale, {50,92,56,255});
    DrawSphere(p + V(0.75f,4.2f*scale,-0.2f), 1.15f*scale, {37,70,47,255});
}

void DrawBuilding(float x, float z, float w, float d, float h, int seed) {
    Color concrete = {(unsigned char)Clamp(58 + seed % 24,25,95),
                      (unsigned char)Clamp(63 + (seed*3)%27,30,105),
                      (unsigned char)Clamp(68 + (seed*5)%34,35,125),255};
    DrawCube(V(x,h*0.5f,z), w,h,d, concrete);
    DrawCube(V(x,h+0.28f,z), w*0.93f,0.56f,d*0.93f, {29,32,38,255});
    DrawCubeWires(V(x,h*0.5f,z), w,h,d, {20,23,28,110});

    int rows = std::max(1, int(h / 4.3f));
    int cols = std::max(1, int(w / 4.0f));
    for (int r = 0; r < rows; ++r) {
        for (int col = 0; col < cols; ++col) {
            if ((col*13 + r*7 + seed) % 6 == 0) continue;
            float xx = x - w*0.5f + 1.8f + col*4.0f;
            float yy = 2.2f + r*4.25f;
            Color glass = ((r + col + seed) % 4 == 0) ? Color{188,157,94,210} : Color{79,116,139,215};
            DrawCube(V(xx,yy,z-d*0.501f), 1.55f, 1.65f, 0.07f, glass);
        }
    }
    DrawCube(V(x,h*0.48f,z-d*0.505f), w*0.045f,h*0.95f,0.035f,{34,38,44,255});
}

void DrawStreetLamp(Vector3 p) {
    DrawCylinder(p + V(0,2.9f,0), 0.10f, 0.13f, 5.8f, 8, {57,61,66,255});
    DrawSphere(p + V(0,5.85f,0), 0.27f, {230,215,173,255});
}

void DrawWorld(const Game& g) {
    ClearBackground({105,124,142,255});
    DrawPlane(V(0,0,0), {CITY,CITY}, {49,65,53,255});
    DrawRoads();

    for (int x=-4; x<=4; ++x) for (int z=-4; z<=4; ++z) {
        if (abs(x)<=3 && abs(z)<=3 && (x==0 || z==0)) continue;
        float px=x*54.0f, pz=z*54.0f;
        if ((x+z)%5==0) {
            DrawCube(V(px,0.05f,pz), 49,0.12f,49,{43,70,49,255});
            for (int k=0;k<6;++k) DrawTree(V(px+Rnd(-18,18),0,pz+Rnd(-18,18)), Rnd(0.7f,1.15f));
        } else {
            DrawBuilding(px+Rnd(-4,4), pz+Rnd(-4,4), Rnd(26,45), Rnd(26,45), Rnd(10,38), x*31+z*17);
        }
    }

    for (int x=-3;x<=3;++x) for (int z=-3;z<=3;++z) {
        if ((x+z)%2==0) DrawStreetLamp(V(x*54.0f+12,0,z*54.0f+12));
    }

    // Landmark tower with layered architectural detail.
    Vector3 tower = V(-108,0,-102);
    DrawCylinder(tower+V(0,19,0), 11,8,38,24,{62,70,80,255});
    DrawCylinder(tower+V(0,39,0), 1.0f,0.42f,24,16,{177,183,188,255});
    DrawSphere(tower+V(0,52,0), 1.55f, {210,64,54,255});
    DrawCylinder(tower+V(0,8,0), 13,11,2,24,{31,36,42,255});

    // Low skyline silhouettes at the city boundary.
    for (int i=0;i<14;++i) {
        float h=18+float((i*17)%35);
        DrawCube(V(-175+i*27,h*0.5f,-178),20,h,10,{35,42,51,255});
    }
    (void)g;
}

void DrawCar(const Car& c) {
    Vector3 p=c.p;
    float s = c.player ? 1.03f : 1.0f;
    Color trim{27,30,34,255};
    Color glass{63,86,99,245};
    DrawCube(p+V(0,0.58f,0), 4.05f*s,1.05f*s,7.35f*s,c.body);
    DrawCube(p+V(0,1.15f,0.35f), 3.05f*s,0.92f*s,3.7f*s,trim);
    DrawCube(p+V(0,1.19f,-0.62f), 2.72f*s,0.58f*s,1.38f*s,glass);
    DrawCube(p+V(0,1.19f,1.72f), 2.72f*s,0.58f*s,1.05f*s,glass);
    DrawCube(p+V(0,0.42f,-3.65f),3.55f*s,0.22f*s,0.13f*s,{205,205,190,255});
    DrawCube(p+V(0,0.42f,3.65f),3.55f*s,0.22f*s,0.13f*s,{195,56,44,255});
    for (int side=-1;side<=1;side+=2) for (int z=-1;z<=1;z+=2)
        DrawCylinder(p+V(side*2.05f*s,0.48f,z*2.45f*s),0.66f*s,0.66f*s,0.36f*s,16,{16,18,20,255});
    if (c.police) {
        DrawCube(p+V(0,1.84f,0),1.38f,0.18f,0.48f,{205,208,207,255});
        // Steady emergency lights. No seizure-inducing strobe circus.
        DrawCube(p+V(-0.38f,1.98f,0),0.30f,0.12f,0.46f,{188,65,61,255});
        DrawCube(p+V(0.38f,1.98f,0),0.30f,0.12f,0.46f,{67,96,176,255});
        DrawCube(p+V(0,1.02f,0),3.2f,0.07f,0.12f,{225,225,225,220});
    }
}

void DrawPed(const Ped& p) {
    if (p.state == PedState::Dead) return;
    Color shirts[]={{58,86,111,255},{121,66,57,255},{75,105,73,255},{104,84,116,255},{145,112,65,255},{65,65,70,255}};
    Color pants[]={{36,39,43,255},{63,70,76,255},{45,52,48,255},{72,61,52,255}};
    Color skin[]={{184,145,112,255},{130,91,67,255},{206,164,126,255},{109,73,53,255}};
    Color shirt=p.state==PedState::Fleeing?Color{76,96,124,255}:shirts[p.style%6];
    Vector3 p0=p.p;
    float bob = sinf(GetTime()*5.0f + p.p.x)*0.025f;
    DrawCylinder(p0+V(0,0.68f+bob,0),0.27f,0.34f,1.25f,10,pants[p.style%4]);
    DrawCube(p0+V(0,1.32f+bob,0),0.62f,0.72f,0.36f,shirt);
    DrawSphere(p0+V(0,1.91f+bob,0),0.29f,skin[p.style%4]);
    // Arms and legs give pedestrians a silhouette rather than two cylinders and a prayer.
    float swing=sinf(GetTime()*7.0f + p.p.z)*0.16f;
    DrawCylinderEx(p0+V(-0.34f,1.48f+bob,0),p0+V(-0.34f+swing,0.83f+bob,0),0.09f,0.075f,8,skin[p.style%4]);
    DrawCylinderEx(p0+V(0.34f,1.48f+bob,0),p0+V(0.34f-swing,0.83f+bob,0),0.09f,0.075f,8,skin[p.style%4]);
    DrawCylinderEx(p0+V(-0.16f,0.73f+bob,0),p0+V(-0.16f-swing,0.08f,0),0.11f,0.09f,8,pants[p.style%4]);
    DrawCylinderEx(p0+V(0.16f,0.73f+bob,0),p0+V(0.16f+swing,0.08f,0),0.11f,0.09f,8,pants[p.style%4]);
}

bool ClearLOS(Vector3 a, Vector3 b) {
    // Buildings are arranged on a coarse grid. Keep the gameplay readable by using a distance test,
    // while avoiding the old always-on police wallhack feel.
    return Dist(a,b) < 72.0f;
}

int ClosestStation(const Game& g, Vector3 p) {
    int best=0; float bd=1e9f;
    for (int i=0;i<(int)g.stations.size();++i) { float d=Dist(g.stations[i].p,p); if(d<bd){bd=d;best=i;} }
    return best;
}

void DispatchPolice(Game& g, Vector3 crime) {
    int station=ClosestStation(g,crime), spawned=0;
    for (auto& c:g.cars) if(c.police&&!c.dispatch&&Dist(c.p,g.stations[station].p)<78) {
        c.dispatch=true; c.speed=16; ++spawned; if(spawned>=3) break;
    }
    if(spawned<3) for(auto& c:g.cars) if(c.police&&!c.dispatch) {
        c.p=g.stations[station].p+V(Rnd(-8,8),1,Rnd(-8,8)); c.dispatch=true; c.speed=16;
        if(++spawned>=3) break;
    }
    g.wanted=Clamp(g.wanted+1,1,5);
}

void Witnesses(Game& g, Vector3 crime) {
    bool witnessed=false;
    for(auto& p:g.peds) if(p.state!=PedState::Dead && Dist(p.p,crime)<34.0f) {
        p.state=PedState::Fleeing; p.panic=6;
        Vector3 away=Vector3Normalize(Flat(p.p-crime));
        if(Vector3Length(away)<0.1f) away=V(1,0,0);
        p.goal=p.p+away*45.0f; witnessed=true;
    }
    if(witnessed) DispatchPolice(g,crime);
}

void Traffic(Game& g,float dt) {
    for(auto& c:g.cars) if(!c.player) {
        Vector3 d=V(sinf(c.yaw),0,cosf(c.yaw));
        if(c.dispatch) {
            Vector3 to=Flat(g.p-c.p);
            if(Vector3Length(to)>1) {
                Vector3 n=Vector3Normalize(to);
                c.yaw=atan2f(n.x,n.z);
                c.speed=Clamp(c.speed+24*dt,12.0f,25.0f);
                d=V(sinf(c.yaw),0,cosf(c.yaw));
            }
        }
        c.p+=d*c.speed*dt;
        c.p.y=Ground(c.p.x,c.p.z)+1.0f;
        if(c.p.x>181)c.p.x=-181; if(c.p.x<-181)c.p.x=181;
        if(c.p.z>181)c.p.z=-181; if(c.p.z<-181)c.p.z=181;
        if(!c.dispatch)c.speed=Clamp(c.speed+Rnd(-0.8f,0.8f)*dt,6.0f,15.0f);
    }
}

void UpdatePeds(Game& g,float dt) {
    for(auto& p:g.peds) if(p.state!=PedState::Dead) {
        if(p.state==PedState::Fleeing) {
            Vector3 d=Flat(p.goal-p.p);
            if(Vector3Length(d)>1) { d=Vector3Normalize(d); p.p+=d*5.7f*dt; p.facing=atan2f(d.x,d.z); }
            p.panic-=dt;
            if(p.panic<=0 || Dist(p.p,g.p)>80) p.state=PedState::Neutral;
        } else {
            Vector3 d=Flat(p.goal-p.p);
            if(Vector3Length(d)<2) p.goal=V(Rnd(-165,165),0,Rnd(-165,165));
            d=Flat(p.goal-p.p);
            if(Vector3Length(d)>0.1f) { d=Vector3Normalize(d); p.p+=d*p.speed*dt; p.facing=atan2f(d.x,d.z); }
        }
        p.p.y=Ground(p.p.x,p.p.z);
    }
}

void EnterExitCar(Game& g) {
    if(g.vehicle) {
        g.p=g.vehicle->p+V(4,0,0); g.vehicle->player=false; g.vehicle=nullptr; g.inCar=false; return;
    }
    float best=5.2f; Car* near=nullptr;
    for(auto& c:g.cars) if(!c.police) { float d=Dist(g.p,c.p); if(d<best){best=d;near=&c;} }
    if(near) { g.inCar=true; g.vehicle=near; near->player=true; g.p=near->p; }
}

void AttackPed(Game& g, Ped& hit) {
    if(hit.state==PedState::Dead) return;
    hit.hp-=34;
    Witnesses(g,hit.p);
    if(hit.hp<=0) hit.state=PedState::Dead;
}

void Shoot(Game& g, Camera3D& cam) {
    if(g.shotCooldown>0) return;
    g.shotCooldown=0.16f;
    Ray ray=GetMouseRay(GetMousePosition(),cam);
    float best=1e9f; Ped* hit=nullptr;
    for(auto& p:g.peds) if(p.state!=PedState::Dead) {
        RayCollision c=GetRayCollisionSphere(ray,p.p+V(0,1.25f,0),0.55f);
        if(c.hit&&c.distance<best){best=c.distance;hit=&p;}
    }
    if(hit) AttackPed(g,*hit);
}

void PoliceResponse(Game& g,float dt) {
    for(auto& c:g.cars) if(c.police&&c.dispatch) {
        float d=Dist(c.p,g.p);
        if(d<25 && ClearLOS(c.p+V(0,1,0),g.p+V(0,1,0)) && Rnd(0,1)<dt*0.85f) {
            if(g.armor>0) g.armor=std::max(0.0f,g.armor-6.0f); else g.hp-=6.0f;
            g.damageFlash=0.06f;
        }
    }
}

void BuildCamera(const Game& g, Camera3D& cam) {
    Vector3 target;
    if(g.inCar && g.vehicle) target=g.vehicle->p+V(0,1.35f,0);
    else target=g.p+V(0,1.58f,0);
    if(g.firstPerson) {
        cam.position=target+V(0,0.08f,0);
        cam.target=cam.position+Forward(g.yaw,g.pitch);
        cam.fovy=g.aim?58.0f:72.0f;
    } else {
        Vector3 f=Forward(g.yaw,g.pitch*0.55f);
        float distance=g.inCar?11.5f:6.8f;
        cam.position=target-f*distance+V(0,2.1f,0);
        cam.target=target;
        cam.fovy=62.0f;
    }
    cam.up={0,1,0}; cam.projection=CAMERA_PERSPECTIVE;
}

void Update(Game& g,float dt,Camera3D& cam) {
    g.time+=dt;
    g.shotCooldown=std::max(0.0f,g.shotCooldown-dt);
    g.damageFlash=std::max(0.0f,g.damageFlash-dt);

    if(IsKeyPressed(KEY_ESCAPE)) { g.paused=!g.paused; if(g.paused) EnableCursor(); else DisableCursor(); }
    if(g.paused) return;

    if(IsKeyPressed(KEY_V)) g.firstPerson=!g.firstPerson;
    Vector2 mouse=GetMouseDelta();
    g.yaw+=mouse.x*0.0027f;
    g.pitch=Clamp(g.pitch-mouse.y*0.0021f,-0.72f,0.82f);
    g.aim=IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    if(IsKeyPressed(KEY_E)) EnterExitCar(g);
    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) Shoot(g,cam);

    Traffic(g,dt);
    UpdatePeds(g,dt);

    if(g.inCar&&g.vehicle) {
        Car& c=*g.vehicle;
        float throttle=(IsKeyDown(KEY_W)?1.0f:0.0f)-(IsKeyDown(KEY_S)?1.0f:0.0f);
        float steer=(IsKeyDown(KEY_D)?1.0f:0.0f)-(IsKeyDown(KEY_A)?1.0f:0.0f);
        float maxSpeed=IsKeyDown(KEY_LEFT_SHIFT)?34.0f:25.0f;
        c.speed=Clamp(c.speed+throttle*28.0f*dt,-11.0f,maxSpeed);
        if(std::fabs(throttle)<0.1f)c.speed*=pow(0.045f,dt);
        c.yaw+=steer*dt*(1.75f+std::fabs(c.speed)*0.045f)*(c.speed>=0?1:-1);
        c.p+=V(sinf(c.yaw),0,cosf(c.yaw))*c.speed*dt;
        c.p.x=Clamp(c.p.x,-176.0f,176.0f); c.p.z=Clamp(c.p.z,-176.0f,176.0f);
        c.p.y=Ground(c.p.x,c.p.z)+1.0f; g.p=c.p;
        if(IsKeyPressed(KEY_SPACE)) c.speed*=0.82f;
    } else {
        Vector3 f=Forward(g.yaw), r=V(f.z,0,-f.x), move{};
        if(IsKeyDown(KEY_W))move+=f; if(IsKeyDown(KEY_S))move-=f;
        if(IsKeyDown(KEY_D))move+=r; if(IsKeyDown(KEY_A))move-=r;
        move.y=0;
        if(Vector3Length(move)>0.01f) g.p+=Vector3Normalize(move)*(IsKeyDown(KEY_LEFT_SHIFT)?9.8f:5.7f)*dt;
        if(IsKeyPressed(KEY_SPACE)&&g.p.y<=Ground(g.p.x,g.p.z)+1.05f) g.p.y+=3.4f;
        g.p.y=std::max(Ground(g.p.x,g.p.z)+1.0f,g.p.y-14.0f*dt);
        g.p.x=Clamp(g.p.x,-176.0f,176.0f); g.p.z=Clamp(g.p.z,-176.0f,176.0f);
    }

    PoliceResponse(g,dt);

    if(g.hp<=0) {
        g.hp=100; g.armor=30; g.cash=std::max(0,g.cash-250); g.wanted=0;
        g.p=V(0,1,20); g.inCar=false; g.vehicle=nullptr;
        for(auto& c:g.cars) if(c.police)c.dispatch=false;
    }

    if(g.mission<(int)g.missions.size()) {
        Mission& m=g.missions[g.mission];
        if(!m.active&&!m.done) {
            m.active=true;
            if(g.mission==2) { g.wanted=3; for(auto& c:g.cars)if(c.police)c.dispatch=true; }
        }
        if(m.active) {
            m.timer+=dt;
            if(g.mission==2 && m.timer>=45) {
                m.done=true; m.active=false; g.cash+=1500; ++g.mission; g.wanted=0;
                for(auto& c:g.cars)if(c.police)c.dispatch=false;
            } else if(g.mission<2 && Dist(g.p,m.target)<m.radius) {
                m.done=true; m.active=false; g.cash+=1000; ++g.mission;
            }
        }
    }
}

void DrawCrosshair(const Game& g) {
    int cx=SW/2, cy=SH/2;
    float gap=g.aim?5.0f:9.0f;
    DrawLine(cx-12,cy,cx-gap,cy,{230,235,235,220});
    DrawLine(cx+gap,cy,cx+12,cy,{230,235,235,220});
    DrawLine(cx,cy-12,cx,cy-gap,{230,235,235,220});
    DrawLine(cx,cy+gap,cx,cy+12,{230,235,235,220});
}

void DrawUI(const Game& g) {
    DrawRectangle(0,0,SW,66,{8,11,15,225});
    DrawText("GROKTASTIC",28,18,26,{232,235,238,255});
    DrawText(g.firstPerson?"FIRST PERSON":"THIRD PERSON",240,23,15,{133,169,185,255});
    DrawText(TextFormat("$ %06d",g.cash),SW-230,18,24,{124,216,160,255});

    DrawText("HP",28,SH-91,15,{214,218,222,255});
    DrawRectangle(28,SH-70,260,11,{34,38,43,255});
    DrawRectangle(28,SH-70,int(260*g.hp/100),11,{195,64,60,255});
    DrawText("ARMOR",28,SH-48,13,{161,170,178,255});
    DrawRectangle(88,SH-49,200,9,{34,38,43,255});
    DrawRectangle(88,SH-49,int(200*g.armor/30),9,{74,126,173,255});

    DrawText("WANTED",SW-230,SH-78,14,{210,214,219,255});
    for(int i=0;i<5;++i) DrawText(i<g.wanted?"*":"-",SW-135+i*24,SH-81,19,i<g.wanted?Color{220,183,76,255}:Color{93,98,105,255});

    if(g.mission<(int)g.missions.size()) {
        const Mission& m=g.missions[g.mission];
        DrawRectangle(30,92,610,86,{8,11,15,205});
        DrawText(m.name.c_str(),50,108,21,{226,190,96,255});
        DrawText(m.text.c_str(),50,140,16,{220,224,228,255});
        if(g.mission==2) DrawText(TextFormat("%02d s",std::max(0,45-int(m.timer))),550,108,21,{216,91,80,255});
    }

    if(g.wanted>0) DrawText("POLICE RESPONSE IN PROGRESS",SW/2-180,89,17,{203,93,86,255});
    DrawText("WASD move/drive   E enter/exit   V camera   LMB fire   RMB aim   SHIFT sprint/boost   SPACE jump/brake   ESC pause",30,SH-18,14,{153,160,168,255});
    DrawCrosshair(g);
}

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(SW,SH,"Groktastic | Urban Crime Prototype");
    SetTargetFPS(120);
    DisableCursor();

    Game g;
    BuildGame(g);
    Camera3D cam{};

    while(!WindowShouldClose()) {
        float dt=Clamp(GetFrameTime(),0.0f,0.033f);
        Update(g,dt,cam);
        BuildCamera(g,cam);

        BeginDrawing();
        BeginMode3D(cam);
        DrawWorld(g);
        for(const auto& c:g.cars) DrawCar(c);
        for(const auto& p:g.peds) DrawPed(p);
        EndMode3D();
        DrawUI(g);
        if(g.damageFlash>0) DrawRectangle(0,0,SW,SH,{150,35,30,30});
        if(g.paused) {
            DrawRectangle(0,0,SW,SH,{0,0,0,150});
            DrawText("PAUSED",SW/2-78,SH/2-20,40,{235,238,240,255});
            DrawText("V switches camera perspective",SW/2-140,SH/2+28,16,{170,178,184,255});
        }
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
