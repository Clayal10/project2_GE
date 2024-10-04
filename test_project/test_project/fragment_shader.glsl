#version 460

in vec4 fcolor;
out vec4 outcolor;

void main(void) {
  outcolor = vec4(fcolor.xyz, 1);
}
