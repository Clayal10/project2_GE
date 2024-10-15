#ifndef BASE_CLASS_H
#define BASE_CLASS_H

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
#include<ctime>
#include "scolor.hpp"
#include "game.h"

#define _USE_MATH_DEFINES
#define GRAVITY 0.015f
#define M_PI 3.14159265f	

class gameobject;

float height = 1550;
float width = 2600;

// Will be used for a lot of stuff throughout the demo
// NOTE:  general_buffer is NOT thread safe.  Don't try to load shaders in parallel!
// NOTE on the NOTE:  You probably shouldn't do that anyway!
#define GBLEN (1024*32)
/* Global section */
char* general_buffer;
int framecount = 0;
int time_resolution = 10;

/* Player globals */
glm::vec3 player_position;
float player_heading;
float player_height = 2;
float player_elevation;
float player_fall_speed = 0;
gameobject* player_platform = 0;
size_t player_platform_index = 0;

std::vector<gameobject*> objects;



GLuint make_program(const char* v_file, const char* tcs_file, const char* tes_file, const char* g_file, const char* f_file);
GLuint make_shader(const char* filename, GLenum shaderType);

class gameobject {
	public:
		bool collision_check = false;
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
		virtual bool collision_with_index(glm::vec3 position, size_t index, float distance = 0) { return false; }//GO BACK TO THIS
		virtual void hit_index(long index) {}
};

class activation_area : public gameobject {
	public:
		std::vector<void (*)()> callbacks;
		activation_area() {
			collision_check = false;
		}
		void add_area(glm::vec3 location, void (*callback_function)()){
			locations.push_back(location);
			callbacks.push_back(callback_function);
		}
		long collision_index(glm::vec3 position, float distance = 0){
			for(long i = 0; i < locations.size(); i++){
				glm::vec3 l = locations[i]; // This'll get optimized out
				// TODO:  Collision Bounds
				if(	size.x/2.0f + distance > abs(l.x-position.x) && 
						size.y/2.0f + distance > abs(l.y-position.y) && 
						size.z/2.0f + distance > abs(l.z-position.z)){
					callbacks[i]();
					return i;
				}
			}
			return -1;
		}
};


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
		float scale = 1.0f;
		bool swap_yz = false;
		loaded_object(const char* of, const char* tf, glm::vec3 s) : objectfile(of), texturefile(tf) {
			size = s;
			collision_check = true;
		}

		int init() override {
			// Initialization part
			std::vector<vertex> vertices;
			std::vector<uint32_t> indices; // Consider unified terminology

			load_model(vertices, indices, objectfile, scale, swap_yz);

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
		}

		bool collision_with_index(glm::vec3 position, size_t index, float distance = 0){
                        glm::vec3 l = locations[index]; // This'll get optimized out
                        if(     size.x/2.0f + distance > abs(l.x-position.x) &&
                                size.y/2.0f + distance > abs(l.y-position.y) &&
                                size.z/2.0f + distance > abs(l.z-position.z)){
                                return true;
                        }
                        return false;

                }

};

float randvel(float speed) {
	long min = -100;
	long max = 100;
	return speed * (min + rand() % (max + 1 - min));
}
/* Projectiles have:
 * 	speed
 * 	direction
 * 	lifespan
 */
class projectile : public loaded_object {
public:
	std::vector<glm::vec3> directions;
	std::vector<float> lifetimes;
	std::vector<bool> bursting;
	std::mutex data_mutex;
	projectile() : loaded_object("projectile.obj", "projectile.jpg", glm::vec3(0.1, 0.1, 0.1)) {
		collision_check = false;
	}
	void create_burst(float quantity, glm::vec3 origin, float speed){
		for(size_t i = 0; i < quantity; i++){
			locations.push_back(origin);
			lifetimes.push_back(10000.0f);
			// One note:  This does create a cube of projectiles
			directions.push_back(glm::vec3(randvel(speed), randvel(speed), randvel(speed)));
			bursting.push_back(false);
		}
	}
	void move() {
		data_mutex.lock();
		for(int i = 0; i < locations.size(); i++){
			if(bursting[i])
				directions[i].y -= 0.0002;
			locations[i] += directions[i];
			lifetimes[i] -= time_resolution; // TODO:  Manage time resolutions better
			if(lifetimes[i] <= 0.0f) {
				if(bursting[i])
					create_burst(200, locations[i], 0.003);
				remove_projectile(i);
			}
		}
		data_mutex.unlock();
	}
	void remove_projectile(size_t index){
		locations.erase(locations.begin() + index);
		directions.erase(directions.begin() + index);
		lifetimes.erase(lifetimes.begin() + index);
		bursting.erase(bursting.begin() + index);
	}
	
	void add_projectile(glm::vec3 location, glm::vec3 direction, float lifetime, bool burst = false){
		data_mutex.lock();
		locations.push_back(location);
		directions.push_back(direction);
		lifetimes.push_back(lifetime);
		bursting.push_back(burst);
		data_mutex.unlock();
	}
	void add_projectile(glm::vec3 location, float heading, float elevation, float speed, float lifetime, float offset = 0.0f, bool burst = false){
		glm::vec3 direction;
		direction.x = cosf(elevation) * sinf(heading);
		direction.y = sinf(elevation);
		direction.z = cosf(elevation) * cosf(heading);
		location += offset * direction;
		if(!burst)
			speed *= 2;
		direction *= speed;
		add_projectile(location, direction, lifetime, burst);
	}
	void hit_index(size_t idx){
		directions[idx] = glm::vec3(0, 0, 0);
// 		directions[idx] = -directions[idx];
	}
};

projectile ice_balls;

class fragment : public loaded_object {
public:
	std::vector<float> life_counts;
	std::vector<glm::vec3> trajectories;
	fragment() : loaded_object("projectile.obj", "brick.jpg", glm::vec3(1.0f, 1.0f, 1.0f)){
		collision_check = false;
	}
	
	void create_burst(float quantity, glm::vec3 origin, float speed){
		for(size_t i = 0; i < quantity; i++){
			locations.push_back(origin);
			life_counts.push_back(1000.0f);
			// One note:  This does create a cube of projectiles
			trajectories.push_back(glm::vec3(randvel(speed), randvel(speed), randvel(speed)));
		}
	}

	void move() {
		for(size_t i = 0; i < locations.size(); i++){
			life_counts[i] -= 0.1f;
			locations[i] += trajectories[i];
			// Is it on the ground?
			// Import player fall code to make this more elaborate and probably buggy
			if(locations[i].y <= -9.0){
				trajectories[i].y = fabs(trajectories[i].y);

				if(fabs(trajectories[i].x) < 0.02)
					trajectories[i].x = 0.0f;
				else 
					trajectories[i].x *= 0.95f;

				if(trajectories[i].y < 0.2)
					trajectories[i].y = 0.0f;
				else
					trajectories[i].y *= 0.8f;

				if(fabs(trajectories[i].z) < 0.02)
					trajectories[i].z = 0.0f;
				else
					trajectories[i].z *= 0.95f;

			} else { 
				trajectories[i].y -= 0.1;
			}
		}
	}
		void draw(glm::mat4 vp) override {
			glUseProgram(program);
			std::vector<glm::mat4> models;
			models.reserve(locations.size());
			for(size_t i = 0; i < locations.size(); i++){
				glm::mat4 new_model = glm::mat4(1.0f);
				new_model = translate(new_model, locations[i]);
				if(fabs(trajectories[i].x) > 0.0f || fabs(trajectories[i].z) > 0.0f)
					new_model = rotate(new_model, life_counts[i], glm::vec3(-trajectories[i].z, 0, trajectories[i].x));
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
	
};

fragment brick_fragments;

class target : public loaded_object {
public:
	target() : loaded_object("monkey.obj", "brick.jpg", glm::vec3(15.0f, 10.0f, 15.0f)) {
		collision_check = true;
	}
	void hit_index(long index){
		// Make fragments
		brick_fragments.create_burst(100, locations[index], 0.01f);
		locations.erase(locations.begin() + index);
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
				if(locations[0].y > 100)
					up = false;
			} else {
				locations[0].y -= .1;
				if(locations[0].y <= 0)
					up = true;
			}
		}
		void draw(glm::mat4 vp){
			loaded_object::draw(vp);
		}
};

#endif
