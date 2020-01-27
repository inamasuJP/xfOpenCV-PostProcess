precision mediump float;

uniform sampler2D originalTexture;
uniform sampler2D bloomTexture;
uniform float toneScale;

varying vec2 vTextureCoord;

void main() {
    vec4 texel = vec4(0.0);
    texel  = texture2D(originalTexture, vTextureCoord) * toneScale;
    texel += texture2D(bloomTexture, vTextureCoord);
    gl_FragColor = texel;
}