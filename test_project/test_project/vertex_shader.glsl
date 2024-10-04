#version 460

in vec3 in_vertex;
in vec3 in_color;
uniform mat4 mvp;
uniform mat4 animation;
out vec4 fcolor;
out vec4 gl_Position;

void main(void) {
	vec4 tmp = animation * vec4(in_vertex, 1.0);
	tmp.x += 25 - 5 * gl_InstanceID;
	gl_Position = mvp * tmp;
 	fcolor = vec4(in_color, 1.0);
}
