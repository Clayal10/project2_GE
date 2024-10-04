#version 460

in vec2 frag_texcoord;
out vec4 outcolor;
uniform sampler2D tex;

void main(void) {
  outcolor = texture(tex, frag_texcoord);
}
