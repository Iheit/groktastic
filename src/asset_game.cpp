#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;
constexpr float WORLD = 3600.0f;
constexpr float HALF = WORLD * 0.5f;
constexpr float PI = 3.14159265359f;
constexpr float STREAM_RADIUS = 900.0f;
constexpr int MAX_MODELS = 72;

template<class T> T Clamp(T v,T a,T b){ return std::max(a,std::min(v,b)); }
float Dist2(Vector3 a,Vector3 b){ const float x=a.x-b.x,z=a.z-b.z; return x*x+z*z; }
Vector3 Forward(float yaw,float pitch=0){ return Vector3Normalize({sinf(yaw)*cosf(pitch),-sinf(pitch),cosf(yaw)*cosf(pitch)}); }

struct Asset { Model model{}; float scale=1.0f; float baseY=0.0f; unsigned long long lastUsed=0; bool loaded=false; };

class AssetLibrary {
public:
    std::vector<std::string> buildings, landmarks, parks, vehicles, props, vegetation;
    std::unordered_map<std::string,Asset> loaded;
    std::vector<std::string> pending;
    unsigned long long frame=0;

    static std::string lower(std::string s){ for(char& c:s)c=(char)std::tolower((unsigned char)c); return s; }
    void scan(){
        const char* roots[]={"assets 1","assets 2","assets 3","assets"};
        std::unordered_map<std::string,bool> seen;
        for(const char* root:roots){
            if(!fs::exists(root)) continue;
            std::error_code ec;
            for(auto it=fs::recursive_directory_iterator(root,fs::directory_options::skip_permission_denied,ec);it!=fs::recursive_directory_iterator();it.increment(ec)){
                if(ec||!it->is_regular_file()) continue;
                if(lower(it->path().extension().string())!=".glb") continue;
                std::string p=it->path().generic_string();
                std::string key=lower(p);
                if(seen[key]) continue; seen[key]=true;
                if(key.find("landmark")!=std::string::npos) landmarks.push_back(p);
                else if(key.find("park")!=std::string::npos) parks.push_back(p);
                else if(key.find("vehicle")!=std::string::npos || key.find("car")!=std::string::npos) vehicles.push_back(p);
                else if(key.find("veget")!=std::string::npos || key.find("tree")!=std::string::npos || key.find("plant")!=std::string::npos) vegetation.push_back(p);
                else if(key.find("prop")!=std::string::npos || key.find("street")!=std::string::npos || key.find("lamp")!=std::string::npos) props.push_back(p);
                else if(key.find("building")!=std::string::npos) buildings.push_back(p);
            }
        }
        auto sort=[](std::vector<std::string>& v){std::sort(v.begin(),v.end());};
        sort(buildings);sort(landmarks);sort(parks);sort(vehicles);sort(props);sort(vegetation);
    }
    const std::vector<std::string>& pool(const std::string& kind) const {
        if(kind=="landmark") return landmarks; if(kind=="park") return parks; if(kind=="vehicle") return vehicles;
        if(kind=="prop") return props; if(kind=="vegetation") return vegetation; return buildings;
    }
    std::string pick(const std::string& kind,unsigned seed) const { const auto& p=pool(kind); return p.empty()?std::string():p[seed%p.size()]; }
    void request(const std::string& p){ if(p.empty()||loaded.count(p))return; if(std::find(pending.begin(),pending.end(),p)==pending.end())pending.push_back(p); }
    void processOne(){
        ++frame;
        if(!pending.empty()){
            std::string p=pending.front(); pending.erase(pending.begin());
            if(!fs::exists(p)) return;
            Model m=LoadModel(p.c_str());
            if(m.meshCount<=0) return;
            BoundingBox b=GetModelBoundingBox(m);
            float h=std::max(0.1f,b.max.y-b.min.y);
            Asset a; a.model=m; a.scale=1.0f; a.baseY=-b.min.y; a.loaded=true; a.lastUsed=frame;
            loaded[p]=a;
        }
        if(loaded.size()<=MAX_MODELS) return;
        std::string victim; unsigned long long oldest=~0ULL;
        for(auto& [p,a]:loaded) if(a.lastUsed<oldest){oldest=a.lastUsed;victim=p;}
        if(!victim.empty()){UnloadModel(loaded[victim].model);loaded.erase(victim);}
    }
    Model* get(const std::string& p){ auto it=loaded.find(p); if(it==loaded.end())return nullptr; it->second.lastUsed=frame; return &it->second.model; }
    void draw(const std::string& p,Vector3 pos,float targetHeight,float yaw=0,float tintScale=1.0f){
        auto it=loaded.find(p); if(it==loaded.end()) {request(p);return;}
        Asset& a=it->second; a.lastUsed=frame;
        BoundingBox b=GetModelBoundingBox(a.model); float h=std::max(0.1f,b.max.y-b.min.y);
        float s=(targetHeight>0?targetHeight/h:1.0f)*tintScale;
        Vector3 origin={pos.x,pos.y+a.baseY*s,pos.z};
        DrawModelEx(a.model,origin,{0,1,0},yaw*180.0f/PI,{s,s,s},WHITE);
    }
    void unload(){for(auto& [p,a]:loaded)UnloadModel(a.model);loaded.clear();pending.clear();}
};

struct Car { Vector3 p{}; float yaw=0,speed=0; bool police=false; std::string asset; };
struct Mission { const char* name; Vector3 target; float radius; bool done=false; };
struct Game {
    Vector3 p{0,1,80}; float yaw=0,pitch=0.16f,health=100,armor=50; int cash=500,wanted=0; bool inCar=false; Car* car=nullptr; float fireTimer=0; float wantedTimer=0; std::vector<Car> cars; std::vector<Mission> missions; int mission=0;
};

float Ground(float x,float z){return 0.12f*sinf(x*.004f)*cosf(z*.003f);}
int Hash(int x,int z){ unsigned n=(unsigned)(x*73856093)^(unsigned)(z*19349663); n^=n>>13; return (int)(n&0x7fffffff); }
std::string District(int x,int z){
    if(z>1050) return "water"; if(std::abs(x)>1250) return "outskirts";
    if(std::abs(x)<360&&std::abs(z)<450) return "downtown";
    if(x>450&&z<650) return "commercial"; if(x<-450&&z<650) return "industrial";
    if(std::abs(x)>500&&z>500) return "park"; return "residential";
}

void DrawRoads(){
    DrawPlane({0,-.08f,0},{WORLD,WORLD},{43,57,46,255});
    for(float q=-1710;q<=1710;q+=90){
        DrawCube({0,0,q},WORLD,.16f,18,{31,34,37,255}); DrawCube({q,.001f,0},18,.16f,WORLD,{31,34,37,255});
        for(float s=-1740;s<1740;s+=42){DrawCube({s,.09f,q},16,.025f,.10f,{201,190,156,190});DrawCube({q,.09f,s},.10f,.025f,16,{201,190,156,190});}
    }
    DrawCube({0,.02f,520},WORLD,.20f,34,{25,28,31,255}); DrawCube({0,.02f,-520},WORLD,.20f,34,{25,28,31,255});
}

void DrawFallbackPlayer(const Game& g){ if(g.inCar)return; DrawCylinder({g.p.x,.9f,g.p.z},.32f,.38f,1.7f,10,{38,43,51,255}); DrawSphere({g.p.x,1.95f,g.p.z},.31f,{196,151,112,255}); }
void DrawCarFallback(const Car& c){
    DrawCube(c.p+Vector3{0,.65f,0},4.0f,1.05f,7.0f,c.police?Color{210,214,218,255}:Color{80,95,115,255});
    DrawCube(c.p+Vector3{0,1.2f,.25f},2.8f,.75f,3.5f,{31,37,44,255});
    DrawCube(c.p+Vector3{0,1.22f,-.5f},2.45f,.48f,1.25f,{67,91,105,255});
}

void BuildTraffic(Game& g,AssetLibrary& a){
    std::mt19937 rng(991); std::uniform_real_distribution<float> d(-1600,1600);
    for(int i=0;i<70;i++){Car c; bool h=i%2==0; float lane=(i%18-9)*90.0f; c.p=h?Vector3{d(rng),.25f,lane}:Vector3{lane,.25f,d(rng)}; c.yaw=h?(i%4<2?0:PI):(i%4<2?PI*.5f:-PI*.5f); c.speed=5+(i%8); c.police=i<6; c.asset=a.pick("vehicle",i*17+3); g.cars.push_back(c);}
}

void UpdateCars(Game& g,float dt){
    for(auto& c:g.cars){
        if(&c==g.car)continue;
        c.p.x+=sinf(c.yaw)*c.speed*dt; c.p.z+=cosf(c.yaw)*c.speed*dt;
        if(c.p.x>1780)c.p.x=-1780;if(c.p.x<-1780)c.p.x=1780;if(c.p.z>1780)c.p.z=-1780;if(c.p.z<-1780)c.p.z=1780;
    }
    if(g.inCar&&g.car){
        Car& c=*g.car; float accel=(IsKeyDown(KEY_W)?24.0f:0)-(IsKeyDown(KEY_S)?18.0f:0); c.speed+=accel*dt; c.speed*=powf(.985f,dt*60); c.speed=Clamp(c.speed,-12.0f,32.0f); float steer=(IsKeyDown(KEY_D)?1:0)-(IsKeyDown(KEY_A)?1:0); c.yaw+=steer*1.9f*dt*(0.25f+fabs(c.speed)/20.0f); c.p.x+=sinf(c.yaw)*c.speed*dt;c.p.z+=cosf(c.yaw)*c.speed*dt;c.p.x=Clamp(c.p.x,-1770.f,1770.f);c.p.z=Clamp(c.p.z,-1770.f,1770.f);g.p=c.p+Vector3{0,1.1f,-sinf(c.yaw)*4.5f};
    }
}

void UpdatePlayer(Game& g,float dt){
    if(g.inCar)return;
    float sp=IsKeyDown(KEY_LEFT_SHIFT)?8.0f:4.5f; Vector3 f=Forward(g.yaw); Vector3 r=Vector3Normalize(Vector3CrossProduct(f,{0,1,0})); Vector3 v{};
    if(IsKeyDown(KEY_W))v+=f;if(IsKeyDown(KEY_S))v-=f;if(IsKeyDown(KEY_A))v-=r;if(IsKeyDown(KEY_D))v+=r;
    if(Vector3Length(v)>.1f){v=Vector3Normalize(v)*sp;g.p+=v*dt;g.yaw=atan2f(v.x,v.z);} g.p.x=Clamp(g.p.x,-1770.f,1770.f);g.p.z=Clamp(g.p.z,-1770.f,1770.f);g.p.y=Ground(g.p.x,g.p.z)+1.0f;
}

void HandleEnterCar(Game& g){
    if(!IsKeyPressed(KEY_E))return;
    if(g.inCar){g.inCar=false;g.p+=Vector3{3,0,0};return;}
    float best=64;Car* chosen=nullptr;for(auto& c:g.cars){float d=Dist2(g.p,c.p);if(d<best){best=d;chosen=&c;}}
    if(chosen){g.inCar=true;g.car=chosen;chosen->speed=0;}
}

void Shoot(Game& g){
    if(g.fireTimer>0||!IsMouseButtonDown(MOUSE_BUTTON_LEFT))return; g.fireTimer=.16f; if(g.wanted<5)g.wanted++; g.wantedTimer=18;
    Vector3 origin=g.inCar?g.car->p+Vector3{0,1.5f,0}:g.p+Vector3{0,.8f,0}; Vector3 dir=Forward(g.yaw,g.pitch);
    float best=75; Car* hit=nullptr; for(auto& c:g.cars){Vector3 to=c.p-origin;float along=Vector3DotProduct(to,dir);if(along<0)continue;Vector3 miss=to-dir*along;float d=Vector3Length(miss);if(d<3.0f&&along<best){best=along;hit=&c;}}
    if(hit){hit->speed+=12;hit->police=false;}
}

void Police(Game& g,float dt){
    if(g.wanted<=0)return; g.wantedTimer-=dt;if(g.wantedTimer<=0){g.wanted--;g.wantedTimer=10;}
    for(auto& c:g.cars)if(c.police){Vector3 target=g.p;Vector3 to=target-c.p;float len=Vector3Length({to.x,0,to.z});if(len<750){c.yaw=atan2f(to.x,to.z);c.speed=14+g.wanted*2;if(len<10){g.health-=dt*5;if(g.health<0)g.health=100;}}}
}

void Missions(Game& g,float dt){
    if(g.missions.empty())return; Mission& m=g.missions[g.mission]; if(m.done){g.mission=(g.mission+1)%g.missions.size();return;} if(Dist2(g.p,m.target)<m.radius*m.radius){m.done=true;g.cash+=750;g.wanted=std::min(5,g.wanted+1);}
    (void)dt;
}

void DrawCity(Game& g,AssetLibrary& a){
    int cx=(int)floor(g.p.x/90.0f),cz=(int)floor(g.p.z/90.0f);
    for(int ix=cx-11;ix<=cx+11;ix++)for(int iz=cz-11;iz<=cz+11;iz++){
        float x=ix*90+45,z=iz*90+45; if((x-g.p.x)*(x-g.p.x)+(z-g.p.z)*(z-g.p.z)>STREAM_RADIUS*STREAM_RADIUS)continue;
        int seed=Hash(ix,iz); std::string d=District((int)x,(int)z);
        if(d=="park"||d=="water"){
            std::string p=a.pick("park",seed); if(!p.empty())a.draw(p,{x,Ground(x,z),z},12+(seed%7),float(seed%360));
        }else{
            std::string kind=(d=="downtown"||d=="commercial"||d=="industrial"||d=="residential")?"building":"building";
            std::string p=a.pick(kind,seed*13+17); float h=d=="downtown"?45+(seed%55):d=="commercial"?20+(seed%35):d=="industrial"?10+(seed%18):7+(seed%13);
            if(!p.empty())a.draw(p,{x+float(seed%19-9),Ground(x,z),z+float((seed/19)%19-9)},h,float(seed%360));
        }
        if((seed%5)==0){std::string p=a.pick("prop",seed+41);if(!p.empty())a.draw(p,{x+28,Ground(x+28,z+28),z+28},4.0f,float(seed%360));}
        if((seed%11)==0){std::string p=a.pick("vegetation",seed+91);if(!p.empty())a.draw(p,{x-25,Ground(x-25,z-25),z-25},5.0f,float(seed%360));}
    }
    for(size_t i=0;i<a.landmarks.size();i++){
        float ang=float(i)*PI*2.0f/float(std::max<size_t>(1,a.landmarks.size()));float r=420.0f+float((i*73)%420);Vector3 p={cosf(ang)*r,Ground(cosf(ang)*r,sinf(ang)*r),sinf(ang)*r};
        if(Dist2(g.p,p)<STREAM_RADIUS*STREAM_RADIUS)a.draw(a.landmarks[i],p,55+(i%4)*18,float(i*37));
    }
}

void DrawHUD(Game& g,AssetLibrary& a){
    DrawRectangle(24,24,360,104,{8,11,15,220});DrawText("GROKTASTIC",42,38,28,{232,235,239,255});DrawText(TextFormat("$ %d",g.cash),42,75,20,{215,198,140,255});DrawText(TextFormat("HP %d   ARMOR %d",(int)g.health,(int)g.armor),165,75,18,WHITE);
    for(int i=0;i<5;i++)DrawRectangle(300+i*14,104,10,10,i<g.wanted?Color{215,70,55,255}:Color{55,59,65,255});
    if(!g.missions.empty()){Mission&m=g.missions[g.mission];DrawRectangle(24,SH-118,470,86,{8,11,15,220});DrawText(m.name,42,SH-102,20,{238,221,170,255});DrawText(m.done?"Complete":"Reach the marked location",42,SH-76,17,WHITE);}
    DrawText(g.inCar?"WASD drive   E exit   LMB fire":"WASD move   SHIFT sprint   E enter car   LMB fire",24,SH-24,16,{205,208,212,230});
    DrawText(TextFormat("STREAM %d/%d  QUEUE %d",(int)a.loaded.size(),MAX_MODELS,(int)a.pending.size()),SW-250,28,14,{190,196,202,220});
}

int main(){
    SetConfigFlags(FLAG_MSAA_4X_HINT|FLAG_VSYNC_HINT|FLAG_WINDOW_RESIZABLE);InitWindow(1600,900,"Groktastic");SetTargetFPS(60);DisableCursor();
    AssetLibrary assets; assets.scan(); Game g; BuildTraffic(g,assets); g.missions={{"DOWNTOWN RELAY",{-180,1,-210},16},{"INDUSTRIAL RUN",{-650,1,-100},18},{"WATERFRONT GETAWAY",{260,1,1420},22}};
    Camera3D cam{};cam.position={0,5,0};cam.target={0,2,10};cam.up={0,1,0};cam.fovy=65;cam.projection=CAMERA_PERSPECTIVE;
    while(!WindowShouldClose()){
        float dt=std::min(GetFrameTime(),.033f); assets.processOne(); g.fireTimer=std::max(0.f,g.fireTimer-dt);
        if(IsKeyPressed(KEY_ESCAPE))EnableCursor(); if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))DisableCursor();
        Vector2 md=GetMouseDelta();g.yaw+=md.x*.0028f;g.pitch=Clamp(g.pitch-md.y*.0022f,-1.0f,.75f);
        HandleEnterCar(g);UpdatePlayer(g,dt);UpdateCars(g,dt);Shoot(g);Police(g,dt);Missions(g,dt);
        Vector3 focus=g.inCar&&g.car?g.car->p+Vector3{0,1.3f,0}:g.p;Vector3 back=Forward(g.yaw,0)*-9.5f+Vector3{0,4.5f,0};cam.position=focus+back;cam.target=focus+Forward(g.yaw,g.pitch)*12.0f; if(g.inCar)cam.position=focus+Forward(g.yaw,0)*-11+Vector3{0,4,0};
        BeginDrawing();ClearBackground({104,124,145,255});BeginMode3D(cam);DrawRoads();DrawCity(g,assets);DrawFallbackPlayer(g);
        for(auto& c:g.cars){if(Dist2(g.p,c.p)>STREAM_RADIUS*STREAM_RADIUS)continue;if(!c.asset.empty()&&assets.get(c.asset))assets.draw(c.asset,c.p,2.0f,c.yaw);else DrawCarFallback(c);}
        EndMode3D();DrawHUD(g,assets);if(g.wanted>0)DrawText("POLICE ALERT",SW/2-80,35,18,{230,75,60,255});EndDrawing();
    }
    assets.unload();CloseWindow();return 0;
}
