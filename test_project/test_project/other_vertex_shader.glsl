#version 460

in vec3 in_vertex;
in vec3 in_color;
uniform mat4 mvp;
out vec4 fcolor;
out vec4 gl_Position;

void main(void) {
	vec4 tmp = vec4(in_vertex.x, in_vertex.y + 6, in_vertex.z, 1.0);
	gl_Position = mvp * tmp;
 	fcolor = vec4(in_color, 1.0);
}
