#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

constexpr int SW = 1600;
constexpr int SH = 900;
constexpr float WORLD = 1800.0f;
constexpr float HALF = WORLD * 0.5f;
constexpr float PI = 3.14159265359f;

template <typename T> T Clamp(T v,T a,T b){return std::max(a,std::min(v,b));}
float Rnd(float a,float b){return a+(b-a)*(float(std::rand())/float(RAND_MAX));}
Vector3 V(float x,float y,float z){return{x,y,z};}
Vector3 Flat(Vector3 v){v.y=0;return v;}
float Dist(Vector3 a,Vector3 b){return Vector3Length(Flat(a-b));}

struct Station{Vector3 p;};
enum class PedState{Neutral,Fleeing,Dead};
enum class District{Downtown,Suburbs,Industrial,Commercial,Park,Waterfront,Highway,Outskirts};
struct Ped{Vector3 p{},goal{};float hp=100,speed=1.35f,panic=0,facing=0;int style=0;PedState state=PedState::Neutral;};
struct Car{Vector3 p{};float yaw=0,speed=0,hp=100;bool player=false,police=false,dispatch=false;Color body{180,50,45,255};};
struct Mission{std::string name,text;Vector3 target{};float radius=8,timer=0;bool active=false,done=false;};
struct Game{
    Vector3 p{0,1,30};float yaw=0,pitch=.16f,time=0,hp=100,armor=30;int cash=500,wanted=0,mission=0;
    bool inCar=false,paused=false,aim=false,firstPerson=false;float shotCooldown=0,damageFlash=0;
    Car* vehicle=nullptr;std::vector<Car> cars;std::vector<Ped> peds;std::vector<Mission> missions;std::vector<Station> stations;
};

float Ground(float x,float z){return .25f*sinf(x*.006f)*cosf(z*.005f)+.10f*sinf((x+z)*.012f);}
Vector3 Forward(float yaw,float pitch=0){return Vector3Normalize(V(sinf(yaw)*cosf(pitch),-sinf(pitch),cosf(yaw)*cosf(pitch)));}
District Zone(float x,float z){
    if(z>610)return District::Waterfront;
    if(fabs(x)>650&&fabs(z)<430)return District::Highway;
    if(fabs(x)>620||fabs(z)>620)return District::Outskirts;
    if(fabs(x)<220&&fabs(z)<260)return District::Downtown;
    if(x>250&&z<300)return District::Commercial;
    if(x<-250&&z<260)return District::Industrial;
    if((x>260&&z>300)||(x<-250&&z>300))return District::Park;
    return District::Suburbs;
}

void AddMission(Game&g,const char*n,const char*t,Vector3 target,float radius){g.missions.push_back({n,t,target,radius});}

void BuildGame(Game&g){
    std::srand(1337);g.cars.clear();g.peds.clear();g.missions.clear();
    g.stations={{V(-720,0,-650)},{V(690,0,-570)},{V(-650,0,520)},{V(650,0,520)},{V(0,0,-40)}};
    const Color cars[]={{38,76,112,255},{155,43,38,255},{174,118,42,255},{42,108,78,255},{76,70,104,255},{178,178,170,255},{54,54,58,255},{120,120,126,255},{92,54,38,255}};
    for(int i=0;i<150;i++){
        Car c;bool h=i%2==0;float lane=(i/2%17-8)*54.0f;float q=Rnd(-830,830);
        c.p=h?V(q,1,lane):V(lane,1,q);c.yaw=h?(i%4<2?0:PI):(i%4<2?PI*.5f:-PI*.5f);c.speed=Rnd(7,16);c.body=i<12?Color{205,210,214,255}:cars[i%9];c.police=i<12;g.cars.push_back(c);
    }
    for(int i=0;i<420;i++){
        Ped p;p.p=V(Rnd(-830,830),0,Rnd(-830,830));p.goal=V(Clamp(p.p.x+Rnd(-80,80),-850.f,850.f),0,Clamp(p.p.z+Rnd(-80,80),-850.f,850.f));p.style=i%8;p.speed=Rnd(1.0f,1.8f);g.peds.push_back(p);
    }
    AddMission(g,"DOWNTOWN RELAY","Reach the communications tower in the financial core.",V(-115,3,-175),12);
    AddMission(g,"INDUSTRIAL RUN","Deliver the package to the freight depot.",V(-510,2,-80),14);
    AddMission(g,"WATERFRONT GETAWAY","Reach the marina before the heat closes in.",V(250,2,700),18);
    AddMission(g,"HEAT CHECK","Survive the police response for 60 seconds.",V(0,1,0),999);
}

void Road(float x,float z,float w,float d){
    DrawCube(V(x,-.02f,z),w,.18f,d,{29,32,35,255});
    DrawCube(V(x,.075f,z-d*.5f+.48f),w,.035f,.10f,{118,121,116,210});
}
void DrawRoads(const Game&g){
    const float spacing=90;
    for(float q=-810;q<=810;q+=spacing){Road(0,q,WORLD,20);Road(q,0,20,WORLD);}
    // Broken secondary roads create blocks without a sterile chessboard.
    for(float q=-765;q<=765;q+=180){Road(q*.32f,q,1500,12);Road(q,-q*.27f,12,1500);}
    // Major boulevards and divided highway.
    Road(0,430,WORLD,34);Road(0,-430,WORLD,34);Road(520,0,34,WORLD);Road(-520,0,34,WORLD);
    for(float s=-840;s<840;s+=24){
        for(float q=-840;q<=840;q+=90){DrawCube(V(s,.09f,q),9,.035f,.16f,{205,190,150,190});DrawCube(V(q,.09f,s),.16f,.035f,9,{205,190,150,190});}
    }
    // Highway shoulders and median.
    DrawCube(V(-760,.12f,0),32,.04f,WORLD,{42,44,44,255});DrawCube(V(760,.12f,0),32,.04f,WORLD,{42,44,44,255});
    DrawCube(V(-760,.14f,0),2,.06f,WORLD,{216,203,164,230});DrawCube(V(760,.14f,0),2,.06f,WORLD,{216,203,164,230});
    (void)g;
}

void DrawTree(Vector3 p,float s){
    DrawCylinder(p+V(0,1.7f*s,0),.25f*s,.34f*s,3.4f*s,8,{69,50,36,255});
    DrawSphere(p+V(0,4.0f*s,0),1.55f*s,{39,69,47,255});
    DrawSphere(p+V(-.8f*s,4.25f*s,.3f*s),1.0f*s,{49,82,52,255});
    DrawSphere(p+V(.75f*s,4.15f*s,-.25f*s),1.1f*s,{34,61,43,255});
}
void DrawStreetLamp(Vector3 p){DrawCylinder(p+V(0,2.9f,0),.08f,.11f,5.8f,8,{55,59,64,255});DrawCube(p+V(.38f,5.65f,0),.8f,.12f,.12f,{61,64,67,255});DrawSphere(p+V(.78f,5.62f,0),.22f,{225,210,170,255});}
void DrawBuilding(float x,float z,float w,float d,float h,int seed,District zone){
    Color base;
    switch(zone){case District::Downtown:base={62,68,76,255};break;case District::Commercial:base={79,72,66,255};break;case District::Industrial:base={75,73,67,255};break;case District::Suburbs:base={91,86,79,255};break;case District::Waterfront:base={69,79,83,255};break;default:base={68,73,70,255};}
    base.r=(unsigned char)Clamp(int(base.r)+(seed%13-6),20,120);base.g=(unsigned char)Clamp(int(base.g)+((seed*3)%13-6),20,120);base.b=(unsigned char)Clamp(int(base.b)+((seed*5)%15-7),20,130);
    DrawCube(V(x,h*.5f,z),w,h,d,base);
    if(zone==District::Downtown&&h>35){DrawCube(V(x,h+.5f,z),w*.72f,1,d*.72f,{39,43,49,255});DrawCube(V(x,h+2,z),w*.36f,3,d*.36f,{48,53,60,255});}
    if(zone==District::Industrial){DrawCube(V(x,h+.5f,z),w*.75f,1,d*.75f,{48,51,50,255});}
    if(zone==District::Suburbs){DrawCube(V(x,h+.35f,z),w*1.03f,.7f,d*1.03f,{58,54,50,255});}
    int rows=std::max(1,int(h/4.5f)),cols=std::max(1,int(w/3.8f));
    for(int r=0;r<rows;r++)for(int c=0;c<cols;c++){
        if((c*17+r*11+seed)%7==0)continue;float xx=x-w*.5f+1.8f+c*3.8f,yy=2.3f+r*4.45f;
        Color glass=((r+c+seed)%5==0)?Color{192,157,92,215}:Color{73,105,121,225};
        DrawCube(V(xx,yy,z-d*.501f),1.42f,1.65f,.07f,glass);
        if(zone==District::Downtown)DrawCube(V(xx,yy,z+d*.501f),1.42f,1.65f,.07f,glass);
    }
    DrawCubeWires(V(x,h*.5f,z),w,h,d,{22,25,28,100});
}

void DrawLandmarks(){
    // Financial tower.
    Vector3 a=V(-115,0,-175);DrawCube(a+V(0,55,0),46,110,42,{48,57,67,255});
    for(int i=0;i<12;i++){float y=10+i*8;DrawCube(a+V(0,y,-21.2f),35,3,1,{105,137,151,220});DrawCube(a+V(0,y,21.2f),35,3,1,{105,137,151,220});}
    DrawCylinder(a+V(0,111,0),2,1,34,16,{170,176,181,255});DrawSphere(a+V(0,130,0),2.5f,{205,70,58,255});
    // Industrial tanks and crane.
    for(int i=0;i<4;i++){Vector3 p=V(-560+i*48,0,-120);DrawCylinder(p+V(0,15,0),15,15,30,20,{93,91,83,255});DrawCylinder(p+V(0,31,0),2,2,8,12,{125,122,112,255});}
    DrawCube(V(-420,28,-95),6,56,6,{67,70,68,255});DrawCube(V(-380,52,-95),85,4,4,{67,70,68,255});
    // Waterfront marina pylons and pier.
    DrawCube(V(270,-.2f,700),360,.5f,90,{91,85,72,255});
    for(int i=0;i<9;i++)DrawCube(V(100+i*42,.3f,750),4,1,34,{63,56,49,255});
    // Stadium in the southern park.
    DrawCylinder(V(410,2,510),92,82,4,40,{64,68,69,255});DrawCylinder(V(410,4,510),70,64,6,40,{32,45,38,255});
    for(int i=0;i<16;i++){float a0=float(i)*PI/8;DrawCube(V(410+cosf(a0)*78,9,510+sinf(a0)*78),3,14,3,{105,107,102,255});}
}

void DrawDistrictDressing(const Game&g){
    const int step=90,range=8;int cx=int(floor(g.p.x/step)),cz=int(floor(g.p.z/step));
    for(int ix=cx-range;ix<=cx+range;ix++)for(int iz=cz-range;iz<=cz+range;iz++){
        float x=ix*step+45,z=iz*step+45;if(Dist(V(x,0,z),g.p)>820)continue;District d=Zone(x,z);
        int seed=ix*92821+iz*68917;
        if(d==District::Park){DrawCube(V(x,0,z),74,.16f,74,{43,69,48,255});for(int k=0;k<5;k++){float ox=std::fmod(float(seed*(k+3)),58.f)-29;float oz=std::fmod(float(seed*(k+5)),58.f)-29;DrawTree(V(x+ox,0,z+oz),.75f+float((k+ix+iz)&1)*.25f);}}
        else if(d==District::Waterfront){DrawCube(V(x,.03f,z),74,.10f,74,{73,82,75,255});for(int k=0;k<2;k++)DrawTree(V(x-24+k*48,0,z-24),1.0f);}
        else if(d==District::Highway){DrawCube(V(x,.02f,z),74,.12f,74,{48,55,48,255});}
        else {float h; if(d==District::Downtown)h=Rnd(35,100);else if(d==District::Commercial)h=Rnd(15,55);else if(d==District::Industrial)h=Rnd(9,28);else if(d==District::Suburbs)h=Rnd(6,18);else h=Rnd(8,25);float w=Rnd(28,62),dd=Rnd(28,62);float ox=std::fmod(float(seed*17),18.f)-9,oz=std::fmod(float(seed*31),18.f)-9;DrawBuilding(x+ox,z+oz,w,dd,h,seed,d);if(d==District::Suburbs){DrawTree(V(x+w*.48f,0,z+dd*.5f),.75f);}}
        if((ix+iz)%3==0&&d!=District::Highway)DrawStreetLamp(V(x+30,0,z+30));
    }
}

void DrawWorld(const Game&g){
    ClearBackground({105,124,142,255});DrawPlane(V(0,-.08f,0),{WORLD,WORLD},{51,65,54,255});DrawRoads(g);DrawDistrictDressing(g);DrawLandmarks();
    // Distant atmospheric skyline silhouettes keep the horizon from ending abruptly.
    for(int i=0;i<28;i++){float x=-850+i*62;float h=20+float((i*29)%55);DrawCube(V(x,h*.5f,-875),42,h,12,{39,47,55,255});}
}

void DrawCar(const Car&c){
    Vector3 p=c.p;float s=c.player?1.04f:1;Color trim={24,27,30,255},glass={57,82,96,245};
    DrawCube(p+V(0,.62f,0),4.1f*s,1.08f*s,7.5f*s,c.body);
    DrawCube(p+V(0,1.18f,.30f),3.0f*s,.88f*s,3.75f*s,trim);
    DrawCube(p+V(0,1.20f,-.55f),2.68f*s,.58f*s,1.45f*s,glass);DrawCube(p+V(0,1.20f,1.72f),2.68f*s,.58f*s,1.08f*s,glass);
    DrawCube(p+V(0,.42f,-3.7f),3.55f*s,.22f*s,.14f*s,{211,211,195,255});DrawCube(p+V(0,.42f,3.7f),3.55f*s,.22f*s,.14f*s,{190,53,44,255});
    for(int side=-1;side<=1;side+=2)for(int z=-1;z<=1;z+=2)DrawCylinder(p+V(side*2.08f*s,.48f,z*2.48f*s),.66f*s,.66f*s,.38f*s,16,{15,17,19,255});
    if(c.police){DrawCube(p+V(0,1.84f,0),1.45f,.18f,.48f,{204,207,205,255});DrawCube(p+V(-.40f,1.98f,0),.30f,.12f,.46f,{184,64,60,255});DrawCube(p+V(.40f,1.98f,0),.30f,.12f,.46f,{66,95,174,255});}
}
void DrawPed(const Ped&p){
    if(p.state==PedState::Dead)return;Color shirts[]={{58,86,111,255},{121,66,57,255},{75,105,73,255},{104,84,116,255},{145,112,65,255},{65,65,70,255},{97,77,65,255},{54,88,91,255}};Color pants[]={{36,39,43,255},{63,70,76,255},{45,52,48,255},{72,61,52,255}};Color skin[]={{184,145,112,255},{130,91,67,255},{206,164,126,255},{109,73,53,255}};
    Vector3 p0=p.p;float t=GetTime(),bob=sinf(t*5+p.p.x)*.025f,swing=sinf(t*7+p.p.z)*.16f;Color shirt=p.state==PedState::Fleeing?Color{76,96,124,255}:shirts[p.style%8];
    DrawCylinder(p0+V(0,.68f+bob,0),.27f,.34f,1.25f,10,pants[p.style%4]);DrawCube(p0+V(0,1.32f+bob,0),.62f,.72f,.36f,shirt);DrawSphere(p0+V(0,1.91f+bob,0),.29f,skin[p.style%4]);
    DrawCylinderEx(p0+V(-.34f,1.48f+bob,0),p0+V(-.34f+swing,.83f+bob,0),.09f,.075f,8,skin[p.style%4]);DrawCylinderEx(p0+V(.34f,1.48f+bob,0),p0+V(.34f-swing,.83f+bob,0),.09f,.075f,8,skin[p.style%4]);
    DrawCylinderEx(p0+V(-.16f,.73f+bob,0),p0+V(-.16f-swing,.08f,0),.11f,.09f,8,pants[p.style%4]);DrawCylinderEx(p0+V(.16f,.73f+bob,0),p0+V(.16f+swing,.08f,0),.11f,.09f,8,pants[p.style%4]);
}

bool ClearLOS(Vector3 a,Vector3 b){return Dist(a,b)<105.0f;}
int ClosestStation(const Game&g,Vector3 p){int best=0;float bd=1e9f;for(int i=0;i<(int)g.stations.size();i++){float d=Dist(g.stations[i].p,p);if(d<bd){bd=d;best=i;}}return best;}
void DispatchPolice(Game&g,Vector3 crime){int station=ClosestStation(g,crime),spawned=0;for(auto&c:g.cars)if(c.police&&!c.dispatch&&Dist(c.p,g.stations[station].p)<150){c.dispatch=true;c.speed=17;++spawned;if(spawned>=4)break;}if(spawned<4)for(auto&c:g.cars)if(c.police&&!c.dispatch){c.p=g.stations[station].p+V(Rnd(-15,15),1,Rnd(-15,15));c.dispatch=true;c.speed=17;if(++spawned>=4)break;}g.wanted=Clamp(g.wanted+1,1,5);}
void Witnesses(Game&g,Vector3 crime){bool seen=false;for(auto&p:g.peds)if(p.state!=PedState::Dead&&Dist(p.p,crime)<40&&ClearLOS(p.p+V(0,1,0),crime+V(0,1,0))){p.state=PedState::Fleeing;p.panic=7;Vector3 away=Vector3Normalize(Flat(p.p-crime));if(Vector3Length(away)<.1f)away=V(1,0,0);p.goal=p.p+away*55;seen=true;}if(seen)DispatchPolice(g,crime);}

void Traffic(Game&g,float dt){for(auto&c:g.cars)if(!c.player){Vector3 d=V(sinf(c.yaw),0,cosf(c.yaw));if(c.dispatch){Vector3 to=Flat(g.p-c.p);if(Vector3Length(to)>1){Vector3 n=Vector3Normalize(to);c.yaw=atan2f(n.x,n.z);c.speed=Clamp(c.speed+28*dt,13.f,28.f);d=V(sinf(c.yaw),0,cosf(c.yaw));}}c.p+=d*c.speed*dt;c.p.y=Ground(c.p.x,c.p.z)+1;if(c.p.x>880)c.p.x=-880;if(c.p.x<-880)c.p.x=880;if(c.p.z>880)c.p.z=-880;if(c.p.z<-880)c.p.z=880;if(!c.dispatch)c.speed=Clamp(c.speed+Rnd(-1.f,1.f)*dt,6.f,17.f);}}
void UpdatePeds(Game&g,float dt){for(auto&p:g.peds)if(p.state!=PedState::Dead){if(p.state==PedState::Fleeing){Vector3 d=Flat(p.goal-p.p);if(Vector3Length(d)>1){d=Vector3Normalize(d);p.p+=d*6.2f*dt;p.facing=atan2f(d.x,d.z);}p.panic-=dt;if(p.panic<=0||Dist(p.p,g.p)>120)p.state=PedState::Neutral;}else{Vector3 d=Flat(p.goal-p.p);if(Vector3Length(d)<2)p.goal=V(Clamp(p.p.x+Rnd(-90,90),-850.f,850.f),0,Clamp(p.p.z+Rnd(-90,90),-850.f,850.f));d=Flat(p.goal-p.p);if(Vector3Length(d)>.1f){d=Vector3Normalize(d);p.p+=d*p.speed*dt;p.facing=atan2f(d.x,d.z);}}p.p.y=Ground(p.p.x,p.p.z);}}
void EnterExitCar(Game&g){if(g.vehicle){g.p=g.vehicle->p+V(4,0,0);g.vehicle->player=false;g.vehicle=nullptr;g.inCar=false;return;}float best=6;Car*near=nullptr;for(auto&c:g.cars)if(!c.police){float d=Dist(g.p,c.p);if(d<best){best=d;near=&c;}}if(near){g.inCar=true;g.vehicle=near;near->player=true;g.p=near->p;}}
void AttackPed(Game&g,Ped&hit){if(hit.state==PedState::Dead)return;hit.hp-=34;Witnesses(g,hit.p);if(hit.hp<=0)hit.state=PedState::Dead;}
void Shoot(Game&g,Camera3D&cam){if(g.shotCooldown>0)return;g.shotCooldown=.16f;Ray ray=GetMouseRay(GetMousePosition(),cam);float best=1e9f;Ped*hit=nullptr;for(auto&p:g.peds)if(p.state!=PedState::Dead){RayCollision c=GetRayCollisionSphere(ray,p.p+V(0,1.25f,0),.55f);if(c.hit&&c.distance<best){best=c.distance;hit=&p;}}if(hit)AttackPed(g,*hit);}
void PoliceResponse(Game&g,float dt){for(auto&c:g.cars)if(c.police&&c.dispatch){float d=Dist(c.p,g.p);if(d<27&&ClearLOS(c.p+V(0,1,0),g.p+V(0,1,0))&&Rnd(0,1)<dt*.9f){if(g.armor>0)g.armor=std::max(0.f,g.armor-6.f);else g.hp-=6;g.damageFlash=.07f;}}}

void BuildCamera(const Game&g,Camera3D&cam){Vector3 target=g.inCar&&g.vehicle?g.vehicle->p+V(0,1.35f,0):g.p+V(0,1.58f,0);if(g.firstPerson){cam.position=target;cam.target=cam.position+Forward(g.yaw,g.pitch);cam.fovy=g.aim?58:72;}else{Vector3 f=Forward(g.yaw,g.pitch*.55f);float dist=g.inCar?12.5f:7.2f;cam.position=target-f*dist+V(0,2.3f,0);cam.target=target;cam.fovy=62;}cam.up={0,1,0};cam.projection=CAMERA_PERSPECTIVE;}

void Update(Game&g,float dt,Camera3D&cam){
    g.time+=dt;g.shotCooldown=std::max(0.f,g.shotCooldown-dt);g.damageFlash=std::max(0.f,g.damageFlash-dt);
    if(IsKeyPressed(KEY_ESCAPE)){g.paused=!g.paused;if(g.paused)EnableCursor();else DisableCursor();}if(g.paused)return;
    if(IsKeyPressed(KEY_V))g.firstPerson=!g.firstPerson;Vector2 mouse=GetMouseDelta();g.yaw+=mouse.x*.0027f;g.pitch=Clamp(g.pitch-mouse.y*.0021f,-.72f,.82f);g.aim=IsMouseButtonDown(MOUSE_BUTTON_RIGHT);if(IsKeyPressed(KEY_E))EnterExitCar(g);if(IsMouseButtonDown(MOUSE_BUTTON_LEFT))Shoot(g,cam);
    Traffic(g,dt);UpdatePeds(g,dt);
    if(g.inCar&&g.vehicle){Car&c=*g.vehicle;float throttle=(IsKeyDown(KEY_W)?1.f:0.f)-(IsKeyDown(KEY_S)?1.f:0.f),steer=(IsKeyDown(KEY_D)?1.f:0.f)-(IsKeyDown(KEY_A)?1.f:0.f);float maxSpeed=IsKeyDown(KEY_LEFT_SHIFT)?39:27;c.speed=Clamp(c.speed+throttle*30*dt,-12.f,maxSpeed);if(std::fabs(throttle)<.1f)c.speed*=pow(.045f,dt);c.yaw+=steer*dt*(1.6f+std::fabs(c.speed)*.045f)*(c.speed>=0?1:-1);c.p+=V(sinf(c.yaw),0,cosf(c.yaw))*c.speed*dt;c.p.x=Clamp(c.p.x,-870.f,870.f);c.p.z=Clamp(c.p.z,-870.f,870.f);c.p.y=Ground(c.p.x,c.p.z)+1;g.p=c.p;if(IsKeyPressed(KEY_SPACE))c.speed*=.80f;}else{Vector3 f=Forward(g.yaw),r=V(f.z,0,-f.x),move{};if(IsKeyDown(KEY_W))move+=f;if(IsKeyDown(KEY_S))move-=f;if(IsKeyDown(KEY_D))move+=r;if(IsKeyDown(KEY_A))move-=r;move.y=0;if(Vector3Length(move)>.01f)g.p+=Vector3Normalize(move)*(IsKeyDown(KEY_LEFT_SHIFT)?10.5f:6.f)*dt;if(IsKeyPressed(KEY_SPACE)&&g.p.y<=Ground(g.p.x,g.p.z)+1.05f)g.p.y+=3.4f;g.p.y=std::max(Ground(g.p.x,g.p.z)+1.f,g.p.y-14*dt);g.p.x=Clamp(g.p.x,-870.f,870.f);g.p.z=Clamp(g.p.z,-870.f,870.f);}
    PoliceResponse(g,dt);
    if(g.hp<=0){g.hp=100;g.armor=30;g.cash=std::max(0,g.cash-250);g.wanted=0;g.p=V(0,1,30);g.inCar=false;g.vehicle=nullptr;for(auto&c:g.cars)if(c.police)c.dispatch=false;}
    if(g.mission<(int)g.missions.size()){Mission&m=g.missions[g.mission];if(!m.active&&!m.done){m.active=true;if(g.mission==3){g.wanted=3;for(auto&c:g.cars)if(c.police)c.dispatch=true;}}if(m.active){m.timer+=dt;if(g.mission==3&&m.timer>=60){m.done=true;m.active=false;g.cash+=2500;++g.mission;g.wanted=0;for(auto&c:g.cars)if(c.police)c.dispatch=false;}else if(g.mission<3&&Dist(g.p,m.target)<m.radius){m.done=true;m.active=false;g.cash+=1250;++g.mission;}}}
}

void DrawCrosshair(const Game&g){int cx=GetScreenWidth()/2,cy=GetScreenHeight()/2;float gap=g.aim?5:9;DrawLine(cx-12,cy,cx-gap,cy,{230,235,235,220});DrawLine(cx+gap,cy,cx+12,cy,{230,235,235,220});DrawLine(cx,cy-12,cx,cy-gap,{230,235,235,220});DrawLine(cx,cy+gap,cx,cy+12,{230,235,235,220});}
void DrawUI(const Game&g){int w=GetScreenWidth(),h=GetScreenHeight();DrawRectangle(0,0,w,70,{8,11,15,225});DrawText("GROKTASTIC",28,17,27,{232,235,238,255});DrawText(g.firstPerson?"FIRST PERSON":"THIRD PERSON",255,24,15,{133,169,185,255});DrawText(TextFormat("$ %06d",g.cash),w-235,18,24,{124,216,160,255});
    DrawText("HP",28,h-95,15,{214,218,222,255});DrawRectangle(28,h-73,270,11,{34,38,43,255});DrawRectangle(28,h-73,int(270*g.hp/100),11,{195,64,60,255});DrawText("ARMOR",28,h-50,13,{161,170,178,255});DrawRectangle(88,h-51,210,9,{34,38,43,255});DrawRectangle(88,h-51,int(210*g.armor/30),9,{74,126,173,255});
    DrawText("WANTED",w-235,h-80,14,{210,214,219,255});for(int i=0;i<5;i++)DrawText(i<g.wanted?"*":"-",w-140+i*25,h-83,19,i<g.wanted?Color{220,183,76,255}:Color{93,98,105,255});
    if(g.mission<(int)g.missions.size()){const Mission&m=g.missions[g.mission];DrawRectangle(30,94,650,88,{8,11,15,210});DrawText(m.name.c_str(),50,110,21,{226,190,96,255});DrawText(m.text.c_str(),50,141,16,{220,224,228,255});if(g.mission==3)DrawText(TextFormat("%02d s",std::max(0,60-int(m.timer))),585,110,21,{216,91,80,255});}
    District d=Zone(g.p.x,g.p.z);const char*zn[] = {"DOWNTOWN","SUBURBS","INDUSTRIAL","COMMERCIAL","PARK","WATERFRONT","HIGHWAY","OUTSKIRTS"};DrawText(zn[int(d)],w-235,28,14,{171,184,190,255});
    if(g.wanted>0)DrawText("POLICE RESPONSE",w/2-95,88,17,{203,93,86,255});DrawText("WASD move/drive  E enter/exit  V camera  LMB fire  RMB aim  SHIFT sprint/boost  SPACE jump/brake  ESC pause",30,h-18,14,{153,160,168,255});DrawCrosshair(g);
}

int main(){SetConfigFlags(FLAG_MSAA_4X_HINT|FLAG_WINDOW_RESIZABLE|FLAG_VSYNC_HINT);InitWindow(SW,SH,"Groktastic | Urban Open World Vertical Slice");SetTargetFPS(120);DisableCursor();Game g;BuildGame(g);Camera3D cam{};
    while(!WindowShouldClose()){float dt=Clamp(GetFrameTime(),0.f,.033f);Update(g,dt,cam);BuildCamera(g,cam);BeginDrawing();BeginMode3D(cam);DrawWorld(g);for(const auto&c:g.cars)if(Dist(c.p,g.p)<850)DrawCar(c);for(const auto&p:g.peds)if(Dist(p.p,g.p)<520)DrawPed(p);EndMode3D();DrawUI(g);if(g.damageFlash>0)DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),{150,35,30,30});if(g.paused){DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),{0,0,0,150});DrawText("PAUSED",GetScreenWidth()/2-78,GetScreenHeight()/2-20,40,{235,238,240,255});}EndDrawing();}CloseWindow();return 0;}
