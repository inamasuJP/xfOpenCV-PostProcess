precision lowp float;

uniform sampler2D texture;
varying vec2 vTextureCoord;

void main() {
    float minBright = 0.5;
    vec3 texel = (max(vec3(0.0), (texture2D(texture,vTextureCoord) - minBright).rgb)/(1.0 - minBright)) * 0.7;
    gl_FragColor = vec4(texel, 1.0);
}