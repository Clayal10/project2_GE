/*
   Simplified OpenGL 4.5 demo
 * Seth Long, Fall 2020
 * This is a *very* short demo which displays a triangle
 * Many of these functions can fail, and return error values
 * I doubt it can be done much shorter without leaving vertices in only main memory or some such
 */

#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<GL/glew.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include<vector>
#include<thread>
#include<chrono>
#include<mutex>
#include "scolor.hpp"
#include "game.h"

#define _USE_MATH_DEFINES
#define GRAVITY 0.015f
#define M_PI 3.14159265

std::mutex grand_mutex;
class gameobject;

float height = 800;
float width = 1280;

// Will be used for a lot of stuff throughout the demo
// NOTE:  general_buffer is NOT thread safe.  Don't try to load shaders in parallel!
// NOTE on the NOTE:  You probably shouldn't do that anyway!
#define GBLEN (1024*32)
/* Global section */
char* general_buffer;
int framecount = 0;

/* Player globals */
glm::vec3 player_position;
float player_heading;
float player_height = 6;
float player_elevation;
float player_fall_speed = 0;
gameobject *player_platform = 0;
size_t player_platform_index = 0;

GLuint make_program(const char* v_file, const char* tcs_file, const char* tes_file, const char* g_file, const char* f_file);
GLuint make_shader(const char* filename, GLenum shaderType);

class gameobject {
	public:
		std::vector<glm::vec3> locations;
		glm::vec3 size; // What about non-square objects?
		virtual int init() { return 0; }
		virtual void deinit() {};
		virtual void draw(glm::mat4) {}
		virtual void move() {}
		virtual void animate() {}
		virtual bool is_on_idx(glm::vec3 position, size_t index) {return false;}
		virtual long is_on(glm::vec3 position) {return -1;}
		virtual long collision_index(glm::vec3 position, float distance = 0) {
			return -1;
		}
		virtual glm::vec3 collision_normal(glm::vec3 move_to, glm::vec3 old_position, long index, float distance = 0) {
			return glm::vec3(0, 0, 0);
		}
};

std::vector<gameobject*> objects;

class tile_floor : public gameobject {
	public:
		unsigned int mvp_uniform, anim_uniform, v_attrib, c_attrib, program, vbuf, cbuf, ebuf;
		int init() override {
			// Initialization part
			float vertices[] = {
				1.0, -10,  1.0,
				1.0, -10,  -1.0,
				-1.0, -10,  -1.0,
				-1.0, -10,  1.0,
			};
			glGenBuffers(1, &vbuf);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, vbuf);
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

			GLushort floor_elements[] = {
				0, 1, 2, 2, 3, 0,

			};
			glGenBuffers(1, &ebuf);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebuf);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(floor_elements), floor_elements, GL_STATIC_DRAW);

			program = make_program("floor_vertex_shader.glsl",0, 0, 0, "floor_fragment_shader.glsl");
			if (!program)
				return 1;

			v_attrib = glGetAttribLocation(program, "in_vertex");
			c_attrib = glGetAttribLocation(program, "in_color");
			mvp_uniform = glGetUniformLocation(program, "mvp");
			return 0;
		}
		void draw(glm::mat4 vp) override {
			glUseProgram(program);

			glEnableVertexAttribArray(v_attrib);
			glBindBuffer(GL_ARRAY_BUFFER, vbuf);
			glVertexAttribPointer(v_attrib, 3, GL_FLOAT, GL_FALSE, 0, 0);

			int size;
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebuf);
			glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);

			glUniformMatrix4fv(mvp_uniform, 1, 0, glm::value_ptr(vp));

			glDrawElementsInstanced(GL_TRIANGLES, size / sizeof(uint16_t), GL_UNSIGNED_SHORT, 0, 10000);
		}
};

class loaded_object : public gameobject {
	public:
		unsigned int mvp_uniform, anim_uniform, v_attrib, t_attrib, program, vbuf, cbuf, ebuf, tex, models_buffer;
		const char *objectfile, *texturefile;
		loaded_object(const char* of, const char* tf, glm::vec3 s) : objectfile(of), texturefile(tf) {
			size = s;
		}

		int init() override {
			// Initialization part
			std::vector<vertex> vertices;
			std::vector<uint32_t> indices; // Consider unified terminology

			load_model(vertices, indices, objectfile);

			glGenBuffers(1, &vbuf);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, vbuf);
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);
			// TODO:  Remember to explain the layout later

			glGenBuffers(1, &ebuf);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebuf);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * indices.size(), indices.data(), GL_STATIC_DRAW);

			glGenBuffers(1, &models_buffer);

			tex = load_texture(texturefile);

			program = make_program("loaded_object_vertex_shader.glsl",0, 0, 0, "loaded_object_fragment_shader.glsl");
			if (!program)
				return 1;

			v_attrib = glGetAttribLocation(program, "in_vertex");
			t_attrib = glGetAttribLocation(program, "in_texcoord");
			mvp_uniform = glGetUniformLocation(program, "vp");
			return 0;
		}

		void draw(glm::mat4 vp) override {
			glUseProgram(program);
			std::vector<glm::mat4> models;
			models.reserve(locations.size());
			for(glm::vec3 l : locations){
				glm::mat4 new_model = glm::mat4(1.0f);
				new_model = translate(new_model, l);
				models.push_back(new_model);
			}
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, models_buffer);
			glBufferData(GL_SHADER_STORAGE_BUFFER, models.size() * sizeof(glm::mat4), models.data(), GL_STATIC_DRAW);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, models_buffer);

			glEnableVertexAttribArray(v_attrib);
			glBindBuffer(GL_ARRAY_BUFFER, vbuf);
			glVertexAttribPointer(v_attrib, 3, GL_FLOAT, GL_FALSE, 20, 0);

			glEnableVertexAttribArray(t_attrib);
			glVertexAttribPointer(t_attrib, 2, GL_FLOAT, GL_FALSE, 20, (const void*)12);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, tex);

			int size;
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebuf);
			glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);

			glUniformMatrix4fv(mvp_uniform, 1, 0, glm::value_ptr(vp));

			glDrawElementsInstanced(GL_TRIANGLES, size / sizeof(GLuint), GL_UNSIGNED_INT, 0, locations.size());
		}
		bool is_on_idx(glm::vec3 position, size_t index){
			return (0.0f < (position.y - locations[index].y) && 
					1.0f > (player_position.y - player_height) - (locations[index].y + size.y/2) &&
					size.x/2 > fabs(position.x - locations[index].x) && 
					size.z/2 > fabs(position.z - locations[index].z));

		}
		long is_on(glm::vec3 position) override {
			for(long i = 0; i < locations.size(); i++)
				if(is_on_idx(position, i))
					return i;
			return -1;
		}
		long collision_index(glm::vec3 position, float distance = 0){
			for(long i = 0; i < locations.size(); i++){
				glm::vec3 l = locations[i]; // This'll get optimized out
				// TODO:  Collision Bounds
				if(	size.x/2.0f + distance > abs(l.x-position.x) && 
						size.y/2.0f + distance > abs(l.y-position.y) && 
						size.z/2.0f + distance > abs(l.z-position.z)){
					return i;	
				}
			}
			return -1;
		}

		/* Leaving y for later, so we finish today */

		glm::vec3 collision_normal(glm::vec3 move_to, glm::vec3 old_position, long index, float distance = 0){
			glm::vec3 l = locations[index]; // This'll get optimized out
			if(	old_position.z > l.z + (size.z/2 + distance) &&
					old_position.x >= l.x - (size.x/2 + distance) &&
					old_position.x <= l.x + (size.x/2 + distance)){
				return glm::vec3(0, 0, 1);
			}
			if(	old_position.z < l.z - (size.z/2 + distance) &&
					old_position.x >= l.x - (size.x/2 + distance) &&
					old_position.x <= l.x + (size.x/2 + distance)){
				return glm::vec3(0, 0, -1);
			}
			if(	old_position.x < l.x - (size.x/2 + distance) &&
					old_position.z >= l.z - (size.z/2 + distance) &&
					old_position.z <= l.z + (size.z/2 + distance)){
				return glm::vec3(1, 0, 0);
			}
			if(	old_position.x > l.x + (size.x/2 + distance) &&
					old_position.z >= l.z - (size.z/2 + distance) &&
					old_position.z <= l.z + (size.z/2 + distance)){
				return glm::vec3(-1, 0, 0);
			}
			puts("Ended collision normal without returning");
			return glm::vec3(0, 0, 0);
		}
};

class elevator : public loaded_object {
	public:
		bool up = true;

		elevator(const char* of, const char* tf, glm::vec3 s) : loaded_object(of, tf, s) {}
		void move(){
			// Just one elevator for now
			if(up) {
				locations[0].y += .1;
				if(locations[0].y > 150)
					up = false;
			} else {
				locations[0].y -= .1;
				if(locations[0].y <= 80)
					up = true;
			}
		}
		void draw(glm::mat4 vp){
			loaded_object::draw(vp);
		}
};

class slider : public loaded_object {
public:
	bool left = true;//if not left, then right
	//double movement = 0;
	double speed = .3f;

	slider(const char* of, const char* tf, glm::vec3 s) : loaded_object(of, tf, s) {}
	void move() {
		// Just one elevator for now
		if (left) {
			locations[0].z += speed;
			//movement += speed;
			if (locations[0].z > 75)
				left = false;
		}
		else {
			locations[0].z -= speed;
			//movement -= speed;
			if (locations[0].z <= 25)
				left = true;
		}
	}
	void draw(glm::mat4 vp) {
		loaded_object::draw(vp);
	}
};

class to_paradise : public loaded_object {
public:

	double speed = .6f;
	bool to = true;

	to_paradise(const char* of, const char* tf, glm::vec3 s) : loaded_object(of, tf, s) {}
	void move() {
		if (to) {
			locations[0].x += speed;
			if (locations[0].x >= 1000) {
				to = false;
			}
		}
		else {
			locations[0].x -= speed;
			if (locations[0].x <= 235) {
				to = true;
			}
		}


	}
	void draw(glm::mat4 vp) {
		loaded_object::draw(vp);
	}
};

GLuint make_shader(const char* filename, GLenum shaderType) {
	FILE* fd = fopen(filename, "r");
	if (fd == 0) {
		printf("File not found:  %s\n", filename);
		return 0;
	}
	size_t readlen = fread(general_buffer, 1, GBLEN, fd);
	fclose(fd);
	if (readlen == GBLEN) {
		printf(RED("Buffer Length of %d bytes Inadequate for File %s\n").c_str(), GBLEN, filename);
		return 0;
	}
	if (readlen == 0) {
		puts(RED("File read problem, read 0 bytes").c_str());
		return 0;
	}
	general_buffer[readlen] = 0;
	printf(DGREEN("Read shader in file %s (%d bytes)\n").c_str(), filename, readlen);
	puts(general_buffer);
	unsigned int s_reference = glCreateShader(shaderType);
	glShaderSource(s_reference, 1, (const char**)&general_buffer, 0);
	glCompileShader(s_reference);
	glGetShaderInfoLog(s_reference, GBLEN, NULL, general_buffer);
	puts(general_buffer);
	GLint compile_ok;
	glGetShaderiv(s_reference, GL_COMPILE_STATUS, &compile_ok);
	if (compile_ok) {
		puts(GREEN("Compile Success").c_str());
		return s_reference;
	}
	puts(RED("Compile Failed\n").c_str());
	return 0;
}

GLuint make_program(const char* v_file, const char* tcs_file, const char* tes_file, const char* g_file, const char* f_file) {
	unsigned int vs_reference = make_shader(v_file, GL_VERTEX_SHADER);
	unsigned int tcs_reference = 0, tes_reference = 0;
	if (tcs_file)
		if (!(tcs_reference = make_shader(tcs_file, GL_TESS_CONTROL_SHADER)))
			return 0;
	if (tes_file)
		if (!(tes_reference = make_shader(tes_file, GL_TESS_EVALUATION_SHADER)))
			return 0;
	unsigned int gs_reference = 0;
	if (g_file)
		gs_reference = make_shader(g_file, GL_GEOMETRY_SHADER);
	unsigned int fs_reference = make_shader(f_file, GL_FRAGMENT_SHADER);
	if (!(vs_reference && fs_reference))
		return 0;
	if (g_file && !gs_reference)
		return 0;


	unsigned int program = glCreateProgram();
	glAttachShader(program, vs_reference);
	if (g_file)
		glAttachShader(program, gs_reference);
	if (tcs_file)
		glAttachShader(program, tcs_reference);
	if (tes_file)
		glAttachShader(program, tes_reference);
	glAttachShader(program, fs_reference);
	glLinkProgram(program);
	GLint link_ok;
	glGetProgramiv(program, GL_LINK_STATUS, &link_ok);
	if (!link_ok) {
		glGetProgramInfoLog(program, GBLEN, NULL, general_buffer);
		puts(general_buffer);
		puts(RED("Link Failed").c_str());
		return 0;
	}

	return program;
}

struct key_status {
	int forward, backward, left, right;
};
struct key_status player_key_status;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods){
	if(GLFW_KEY_W == key && 1 == action){
		player_key_status.forward = 1;
	}	
	else if(GLFW_KEY_W == key && 0 == action){
		player_key_status.forward = 0;
	}	
	if(GLFW_KEY_S == key && 1 == action){
		player_key_status.backward = 1;
	}	
	else if(GLFW_KEY_S == key && 0 == action){
		player_key_status.backward = 0;
	}
	if(GLFW_KEY_A == key)
		player_key_status.left = action;
	if(GLFW_KEY_D == key)
		player_key_status.right = action;
	if(GLFW_KEY_SPACE == key && 1 == action){
		if(player_platform){//only jump on platforms. Keep?
			player_fall_speed = 0.65f;
			player_position.y += 1;
			/*
			while (player_fall_speed < GRAVITY) {
				player_fall_speed += .001f;
			}
			*/
			player_platform = 0;
		}
	}
}

int shutdown_engine = 0;
/* Must be called at a consistent rate */
void player_movement(){
	while(!shutdown_engine){
		grand_mutex.lock();
		auto start = std::chrono::system_clock::now();
		glm::vec3 step_to_point = player_position;
		float speed;
		if(player_key_status.forward){
			if (!player_platform) {
				speed = 0.2f;
			}
			else {
				speed = 0.6f;
			}
			step_to_point += speed * glm::vec3(sinf(player_heading), 0, cosf(player_heading));
		}
		if(player_key_status.backward){
			if (!player_platform) {
				speed = 0.2f;
			}
			else {
				speed = 0.6f;
			}
			step_to_point += speed * glm::vec3(-sinf(player_heading), 0, -cosf(player_heading));
		}
		if(player_key_status.left){
			if (!player_platform) {
				speed = 0.2f;
			}
			else {
				speed = 0.6f;
			}
			step_to_point += speed * glm::vec3(sinf(player_heading + M_PI/2), 0, cosf(player_heading + M_PI/2));
		}
		if(player_key_status.right){
			if (!player_platform) {
				speed = 0.2f;
			}
			else {
				speed = 0.6f;
			}
			step_to_point += speed * glm::vec3(-sinf(player_heading + M_PI/2), 0, -cosf(player_heading + M_PI/2));
		}
		// TODO:  What about x and z?
		/*
		 * if we're touching a wall:
		 * 	Note what directions our movement is blocked in
		 * 	Big question:  Can we have walls that don't align with an axis?
		 * 	At first:  NO
		 * 	Later:  Yes
		 */
		glm::vec3 block_direction(0, 0, 0);
		for(gameobject* o : objects) {
			long collide_index = o->collision_index(player_position, 4.0f);
			if(collide_index != -1) {
				block_direction = o->collision_normal(step_to_point, player_position, collide_index, 4.0f);
			}
		}
		if(block_direction.x <= 0.0f && player_position.x < step_to_point.x)
			player_position.x = step_to_point.x;
		else if(block_direction.x >= 0.0f && player_position.x > step_to_point.x)
			player_position.x = step_to_point.x;
		if(block_direction.z <= 0.0f && player_position.z < step_to_point.z)
			player_position.z = step_to_point.z;
		else if(block_direction.z >= 0.0f && player_position.z > step_to_point.z)
			player_position.z = step_to_point.z;
		
/*
		if(block_direction.x != 0.0f || block_direction.z != 0.0f){
			xblock = 0.0f;
			zblock = 0.0f;
		}
*/
		if(player_platform){
			if(!player_platform->is_on_idx(player_position, player_platform_index))
				player_platform = 0;
			else {
				/*
				   glm::vec3 pltloc = player_platform->locations[player_platform_index];
				   float floor_height = pltloc.y + (player_platform->size.y / 2);
				   printf("On object, offset = %f\n", player_position.y - (floor_height + player_height));
				   player_position.y = floor_height + player_height;
				 */

			}
		} else {
			float floor_height = -10;
			for(gameobject* o : objects) {
				long ppi = o->is_on(player_position);
				if(ppi != -1) {
					player_platform_index = ppi;
					player_platform = o;	
					floor_height = (player_platform->locations[player_platform_index].y) + (player_platform->size.y / 2);
					player_fall_speed = 0;
					player_position.y = floor_height;
				}
			}
			//gravity is always a factor here
			if(player_position.y - player_height > floor_height) {
				player_position.y += player_fall_speed;
				player_fall_speed -= GRAVITY;
			} else {
				player_fall_speed = 0;
				player_position.y = floor_height + player_height; 
			}
		}
		grand_mutex.unlock();
		auto end = std::chrono::system_clock::now();
		//		double difference = std::chrono::duration_cast<std::chrono::milliseconds>(start - end).count();
		//		printf("Time difference:  %lf\n", difference);
		std::this_thread::sleep_for(std::chrono::microseconds(1000) - (start - end));
	}
}

void object_movement(){
	while(!shutdown_engine){
		auto start = std::chrono::system_clock::now();
		grand_mutex.lock();
		if(player_platform){
			glm::vec3 pltloc = player_platform->locations[player_platform_index];
			float floor_height = pltloc.y + (player_platform->size.y / 2);
			player_position.y = floor_height + player_height;
		}
		grand_mutex.unlock();
		for(gameobject* o : objects)
			o->move();
		auto end = std::chrono::system_clock::now();
		//		double difference = std::chrono::duration_cast<std::chrono::milliseconds>(start - end).count();
		//		printf("Time difference:  %lf\n", difference);
		std::this_thread::sleep_for(std::chrono::microseconds(10000) - (start - end));
	}
}

void animation(){
	while(!shutdown_engine){
		auto start = std::chrono::system_clock::now();
		for(gameobject* o : objects)
			o->animate();
		auto end = std::chrono::system_clock::now();
		//		double difference = std::chrono::duration_cast<std::chrono::milliseconds>(start - end).count();
		//		printf("Time difference:  %lf\n", difference);
		std::this_thread::sleep_for(std::chrono::microseconds(10000) - (start - end));
	}
}

void collision_detection(){
	while(!shutdown_engine){
		auto start = std::chrono::system_clock::now();
		// TODO:  Do this  (is it n^2?)
		auto end = std::chrono::system_clock::now();
		//		double difference = std::chrono::duration_cast<std::chrono::milliseconds>(start - end).count();
		//		printf("Time difference:  %lf\n", difference);
		std::this_thread::sleep_for(std::chrono::microseconds(10000) - (start - end));
	}
}
void pos_callback(GLFWwindow* window, double xpos, double ypos){
	double center_x = width/2;
	double diff_x = xpos - center_x;
	double center_y = height/2;
	double diff_y = ypos - center_y;
	glfwSetCursorPos(window, center_x, center_y);
	player_heading -= diff_x / 1000.0; // Is this too fast or slow?
	player_elevation -= diff_y / 1000.0;
}

void resize(GLFWwindow*, int new_width, int new_height){
	width = new_width;
	height = new_height;
	printf("Window resized, now %f by %f\n", width, height);
	glViewport(0, 0, width, height);
}

int main(int argc, char** argv) {
	general_buffer = (char*)malloc(GBLEN);
	glfwInit();
	GLFWwindow* window = glfwCreateWindow(width, height, "Simple OpenGL 4.0+ Demo", 0, 0);
	//resize(window, 1000, 800);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwMakeContextCurrent(window);
	glewInit();

	unsigned supported_threads = std::thread::hardware_concurrency();
	printf("Supported threads:  %u\n", supported_threads);

	/* Set up callbacks */
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, pos_callback);
	glfwSetFramebufferSizeCallback(window, resize);

	/* Set starting point */
	player_position = glm::vec3(50, 10, 50);
	player_heading = M_PI/2.0f;


	/* Level Loading (hardcoded at the moment) */

	tile_floor fl;
	objects.push_back(&fl);

	//starting pad. eyes are at 0 (won't let it go lower) object collision is -10
	loaded_object start("start_platform.obj", "brick.jpg", glm::vec3(30, 1, 30));//need to change the .obj
	start.locations.push_back(glm::vec3(50, -10, 50));
	objects.push_back(&start);
	
	//just a line of platforms
	loaded_object basic_pad("platform.obj", "brick.jpg", glm::vec3(8, 2, 8));
	int x, y, z;
	for (int i = 0; i < 100; i += 10) {
		
		x = 75 + (i-(i/5));
		y = i;
		z = 50;

		basic_pad.locations.push_back(glm::vec3(x, y, z));
	}//the last platform is at (147, 90, 50)
	objects.push_back(&basic_pad);

	//sideways moving objects
	slider slider_left("sliding_platform.obj", "ice.jpg", glm::vec3(16, 2, 16));
	slider_left.locations.push_back(glm::vec3(159, 90, 50));
	objects.push_back(&slider_left);
	
	slider slider_right("sliding_platform.obj", "ice.jpg", glm::vec3(16, 2, 16));
	slider_right.locations.push_back(glm::vec3(175, 90, 50));
	slider_right.left = false;
	objects.push_back(&slider_right);
	
	//just a walkway before elevator
	loaded_object static_platforms("platform.obj", "brick.jpg", glm::vec3(8, 2, 8));
	for (int x = 187; x <= 227; x += 8) {
		static_platforms.locations.push_back(glm::vec3(x, 90, 50));
	}
	for (int z = 58; z <= 98; z += 8) {
		static_platforms.locations.push_back(glm::vec3(235, 150, z));//go off to the right
	}
	objects.push_back(&static_platforms);

	//elevator takes you up to (227, 150, 50)
	elevator e("platform.obj", "brick.jpg", glm::vec3(8, 2, 8));
	e.locations.push_back(glm::vec3(235, 80, 50));
	objects.push_back(&e);

	//perhaps a guide to take you to paradise
	to_paradise tp("platform.obj", "beans.jpg", glm::vec3(8, 2, 8));
	tp.locations.push_back(glm::vec3(235, 150, 98));
	objects.push_back(&tp);
	
	//paradise
	loaded_object paradise("paradise.obj", "paradise.jpg", glm::vec3(20, 20, 20));
	paradise.locations.push_back(glm::vec3(1000, 140, 100));
	objects.push_back(&paradise);

	/* Initialize game objects */
	for(gameobject* o : objects){
		if(o->init()){
			puts(RED("Compile Failed, giving up!").c_str());
			return 1;
		}
	}

	/* Start Other Threads */
	std::thread player_movement_thread(player_movement);
	std::thread object_movement_thread(object_movement);
	std::thread animation_thread(animation);
	std::thread collision_detection_thread(collision_detection);

	glEnable(GL_DEPTH_TEST);
	while (!glfwWindowShouldClose(window)) {
		framecount++;
		glfwPollEvents();
		glClearColor(0, 0, 0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		glClear(GL_DEPTH_BUFFER_BIT);

		grand_mutex.lock();

		glm::vec3 axis_y(0, 1, 0);
		/* Where are we?  A:  player_position
		 * What are we looking at?
		 */
		glm::vec3 look_at_point = player_position;
		look_at_point.x += cosf(player_elevation) * sinf(player_heading);
		look_at_point.y += sinf(player_elevation);
		look_at_point.z += cosf(player_elevation) * cosf(player_heading);
		glm::mat4 view = glm::lookAt(player_position, look_at_point, glm::vec3(0, 1, 0));
		glm::mat4 projection = glm::perspective(45.0f, width / height, 0.1f, 10000.0f);
		glm::mat4 vp = projection * view;

		for(gameobject* o : objects)
			o->draw(vp);
		grand_mutex.unlock();

		glfwSwapBuffers(window);
	}
	shutdown_engine = 1;
	player_movement_thread.join();
	// TODO:  join other threads
	glfwDestroyWindow(window);
	glfwTerminate();
	free(general_buffer);
}
