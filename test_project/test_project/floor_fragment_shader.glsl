#version 460

in vec2 f_texcoord;
//in vec4 fcolor;
out vec4 outcolor;
uniform sampler2D tex;

void main(void) {
  //outcolor = vec4(fcolor.xyz, 1);
	outcolor = texture(tex, f_texcoord);
}
