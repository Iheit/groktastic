#include <raylib.h>
#include <raymath.h>
#include <cmath>

// Visual interception layer. The existing game remains the gameplay authority;
// this layer gives its primitive-based world a higher-detail presentation pass.
static void HQDrawCube(Vector3 p,float w,float h,float d,Color c){
    ::DrawCube(p,w,h,d,c);
    const float volume=w*h*d;
    if(h>0.55f && w>8.0f && d>8.0f && volume>700.0f){
        Color edge={18,21,24,90};
        ::DrawCubeWires(p,w+0.035f,h+0.035f,d+0.035f,edge);
        float sx=w*.5f, sz=d*.5f;
        Color trim={150,154,158,55};
        ::DrawCube({p.x-sx+.08f,p.y,p.z-sz+.08f},.12f,h*.94f,.12f,trim);
        ::DrawCube({p.x+sx-.08f,p.y,p.z+sz-.08f},.12f,h*.94f,.12f,trim);
    }
    if(w>3.4f && w<4.8f && h>.75f && h<1.45f && d>6.0f && d<8.6f){
        Color sill={12,15,18,145};
        ::DrawCube({p.x,p.y-.42f,p.z},w*.96f,.055f,d*.92f,sill);
        ::DrawCube({p.x,p.y+.49f,p.z-d*.44f},w*.72f,.035f,.035f,{225,225,210,180});
    }
}

static void HQDrawCylinder(Vector3 p,float r1,float r2,float h,int slices,Color c){
    ::DrawCylinder(p,r1,r2,h,slices,c);
    if(r1>2.5f && h>8.0f){
        Color ring={175,177,171,75};
        ::DrawCylinderWires(p,r1+.04f,r2+.04f,h+.04f,slices,ring);
        ::DrawCylinder({p.x,p.y+h*.36f,p.z},r1*1.002f,r2*1.002f,.09f,slices,ring);
        ::DrawCylinder({p.x,p.y-h*.36f,p.z},r1*1.002f,r2*1.002f,.09f,slices,ring);
    }
}

static void HQDrawSphere(Vector3 p,float r,Color c){
    ::DrawSphere(p,r,c);
    if(r>.45f){
        Color rim={220,228,216,28};
        ::DrawSphere({p.x-r*.18f,p.y+r*.18f,p.z-r*.18f},r*.72f,rim);
    }
}

static void HQDrawPlane(Vector3 p,Vector2 size,Color c){
    ::DrawPlane(p,size,c);
    if(size.x>1000.0f && size.y>1000.0f){
        Color grid={86,94,82,20};
        for(float x=-1700.0f;x<=1700.0f;x+=180.0f)
            ::DrawLine3D({x,.012f,-1700.0f},{x,.012f,1700.0f},grid);
        for(float z=-1700.0f;z<=1700.0f;z+=180.0f)
            ::DrawLine3D({-1700.0f,.013f,z},{1700.0f,.013f,z},grid);
    }
}

#define DrawCube HQDrawCube
#define DrawCylinder HQDrawCylinder
#define DrawSphere HQDrawSphere
#define DrawPlane HQDrawPlane
#define main groktastic_game_main
#include "main.cpp"
#undef main
#undef DrawCube
#undef DrawCylinder
#undef DrawSphere
#undef DrawPlane

int main(){ return groktastic_game_main(); }
