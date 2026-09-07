#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform float time;
void main(){vec2 uv=fragTexCoord;vec3 c=texture(texture0,uv).rgb;float d=distance(uv,vec2(.5));float v=smoothstep(.82,.25,d);c*=mix(.82,1.06,v);c+=.012*sin(vec3(1.,1.7,2.4)*(time*.8+uv.y*40.));c=c/(c+vec3(.75));c=pow(c,vec3(.92));finalColor=vec4(c,1.);}
