#version 460

layout(packed, binding=0) buffer model_list {
	mat4 models[];
};
in vec3 in_vertex;
in vec2 in_texcoord;
uniform mat4 vp;
out vec2 frag_texcoord;
out vec4 gl_Position;

void main(void) {	
	gl_Position = vp * models[gl_InstanceID] * vec4(in_vertex, 1.0);
	frag_texcoord = in_texcoord;
}
