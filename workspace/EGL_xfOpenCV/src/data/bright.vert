attribute vec4 position;
attribute vec2 textureCoord;
varying vec2 vTextureCoord;

void main() {
    gl_Position = position;
    vTextureCoord = textureCoord;
}