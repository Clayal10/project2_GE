/*	Some Notes:
 - Replace floor with texture (DONE)
 - change shooting / get rid of burst?
 - Reorganize classes into something like base_class.h (DONE)
	- Could reorganize more by making base_class.cpp and putting classes in game.h or erase game.h
 - Create proper turret class.
 - Add a health or damage mechanism
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
#include<ctime>
#include "stb_image.h"
#include "scolor.hpp"
#include "base_class.h"

#define _USE_MATH_DEFINES
#define GRAVITY 0.015f
#define M_PI 3.14159265f

std::mutex grand_mutex;

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

void fire(bool burst = false){
	ice_balls.add_projectile(player_position, player_heading, player_elevation, 0.3f, 10000.0f, 1.0f, burst);
}

void mouse_click_callback(GLFWwindow* window, int button, int action, int mods){
	if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
		fire();//non burst
	if(button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
		fire(true);//burst
}

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
		//if(player_platform){ //this is for only jumping on a platform
			player_fall_speed = 0.65f;
			player_position.y += 1.0f;
			player_platform = 0;
		//}
	}
}


long is_empty(glm::vec3 position, float distance){
        for(gameobject* o : objects) {
                long collide_index = o->collision_index(position, 0.2f);
                if(collide_index != -1)
                        return false;
        }
        return true;

}

int shutdown_engine = 0;
/* Must be called at a consistent rate */
void player_movement(){
	while(!shutdown_engine){
//		grand_mutex.lock();
		auto start = std::chrono::system_clock::now();
		glm::vec3 step_to_point = player_position;
		if(player_key_status.forward){
			step_to_point += 0.6f * glm::vec3(sinf(player_heading), 0, cosf(player_heading));
		}
		if(player_key_status.backward){
			step_to_point += 0.4f * glm::vec3(-sinf(player_heading), 0, -cosf(player_heading));
		}
		if(player_key_status.left){
			step_to_point += 0.6f * glm::vec3(sinf(player_heading + M_PI/2), 0, cosf(player_heading + M_PI/2));
		}
		if(player_key_status.right){
			step_to_point += 0.4f * glm::vec3(-sinf(player_heading + M_PI/2), 0, -cosf(player_heading + M_PI/2));
		}
                for(gameobject* o : objects) {
                        long collide_index = o->collision_index(step_to_point, 0.2f);
                        if(collide_index != -1) {
                                if(is_empty(glm::vec3(player_position.x, step_to_point.y, step_to_point.z), 0.2f)) {
                                        step_to_point.x = player_position.x;
                                        break;
                                }
                                else if(is_empty(glm::vec3(step_to_point.x, step_to_point.y, player_position.z), 0.2f)) {
                                        step_to_point.z = player_position.z;
                                        break;
                                }
                                else {
                                        step_to_point = player_position;
                                        break;
                                }


                        }
                }
                player_position = step_to_point;

		if(player_platform){
			if(!player_platform->is_on_idx(player_position, player_platform_index))
				player_platform = 0;
		} else {
			float floor_height = 0;
			for(gameobject* o : objects) {
				long ppi = o->is_on(player_position);
				if(ppi != -1) {
					player_platform_index = ppi;
					player_platform = o;	
					floor_height = player_platform->locations[player_platform_index].y + (player_platform->size.y / 2);
					player_fall_speed = 0;
					player_position.y = floor_height + player_height; 
				}
			}
			if(player_position.y - player_height > floor_height) {
				player_position.y += player_fall_speed;
				player_fall_speed -= GRAVITY;
			} else {
				player_fall_speed = 0;
				player_position.y = floor_height + player_height; 
			}
		}
//		grand_mutex.unlock();
		auto end = std::chrono::system_clock::now();
		//		double difference = std::chrono::duration_cast<std::chrono::milliseconds>(start - end).count();
		//		printf("Time difference:  %lf\n", difference);
		std::this_thread::sleep_for(std::chrono::microseconds(1000) - (start - end));
	}
}

void object_movement(){
	while(!shutdown_engine){
		auto start = std::chrono::system_clock::now();
//		grand_mutex.lock();
		if(player_platform){
			glm::vec3 pltloc = player_platform->locations[player_platform_index];
			float floor_height = pltloc.y + (player_platform->size.y / 2);
			player_position.y = floor_height + player_height;
		}
//		grand_mutex.unlock();
		for(gameobject* o : objects)
			o->move();
		auto end = std::chrono::system_clock::now();
		//		double difference = std::chrono::duration_cast<std::chrono::milliseconds>(start - end).count();
		//		printf("Time difference:  %lf\n", difference);
		std::this_thread::sleep_for(std::chrono::microseconds(1000) - (start - end));
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
		ice_balls.data_mutex.lock();
		for(size_t proj_index = 0; proj_index < ice_balls.locations.size(); proj_index++){
			glm::vec3 l = ice_balls.locations[proj_index];
			for(auto o : objects){
				if(o->collision_check){
					long index = o->collision_index(l);
					if(index != -1) {
						o->hit_index(index);
						ice_balls.hit_index(proj_index);
					}
				}
			}
		}	
		ice_balls.data_mutex.unlock();
		auto end = std::chrono::system_clock::now();
		//		double difference = std::chrono::duration_cast<std::chrono::milliseconds>(start - end).count();
		//		printf("Time difference:  %lf\n", difference);
		std::this_thread::sleep_for(std::chrono::microseconds(1000) - (start - end));
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


target targets;
void bob(){
	static bool bob_happened = false;
	if(!bob_happened){
		bob_happened = true;
		targets.locations.push_back(glm::vec3(-10, 5, 10));
	}
};

int main(int argc, char** argv) {
	srand((unsigned int)time(0));

	general_buffer = (char*)malloc(GBLEN);
	glfwInit();
	GLFWwindow* window = glfwCreateWindow(width, height, "Simple OpenGL 4.0+ Demo", 0, 0);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwMakeContextCurrent(window);
	glewInit();

	unsigned supported_threads = std::thread::hardware_concurrency();
	printf("Supported threads:  %u\n", supported_threads);

	/* Set up callbacks */
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, pos_callback);
	glfwSetFramebufferSizeCallback(window, resize);
	glfwSetMouseButtonCallback(window, mouse_click_callback);

	/* Set starting point */
	player_position = glm::vec3(53, 10, 50);
	player_heading = M_PI;


	/* Level Loading (hardcoded at the moment) */
	tile_floor fl;
	objects.push_back(&ice_balls);
	objects.push_back(&fl);


	//targets.scale = 10.0f;

	objects.push_back(&brick_fragments);

	/*The wall
	* The current reconfigured engine is using a seperate object type for collision, this version still uses
	* loaded object.
	loaded_object wallblock("cube.obj", "brick.jpg", glm::vec3(2, 2, 2));
	objects.push_back(&wallblock);
	for(int x = 0; x < 20; x += 2)
		for(int y = -10; y < 10; y += 2)
			for(int z = 0; z < 4; z += 2)
				wallblock.locations.push_back(glm::vec3(x, y, z));
	*/

	/*texture cube*/
	loaded_object tex_cube("tex_cube.obj", "beans.jpg", glm::vec3(2, 2, 2));
	tex_cube.locations.push_back(glm::vec3(0, 0, 0));
	objects.push_back(&tex_cube);

	/*
	activation_area target_spawning;
	target_spawning.size = glm::vec3(10, 10, 10);
	target_spawning.add_area(glm::vec3(10, 0, 10), bob);
	objects.push_back(&target_spawning);
	*/

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

//		grand_mutex.lock();

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
//		grand_mutex.unlock();

		glfwSwapBuffers(window);
	}
	shutdown_engine = 1;
	player_movement_thread.join();
	// TODO:  join other threads
	glfwDestroyWindow(window);
	glfwTerminate();
	free(general_buffer);
}
