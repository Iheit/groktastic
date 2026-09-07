#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform float time;

void main() {
    vec2 uv = fragTexCoord;
    vec3 c = texture(texture0, uv).rgb;
    float d = distance(uv, vec2(0.5));
    float vignette = smoothstep(0.82, 0.25, d);
    c *= mix(0.82, 1.06, vignette);
    c += 0.012 * sin(vec3(1.0, 1.7, 2.4) * (time * 0.8 + uv.y * 40.0));
    c = c / (c + vec3(0.75));
    c = pow(c, vec3(0.92));
    finalColor = vec4(c, 1.0);
}
