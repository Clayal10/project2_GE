#define GLM_ENABLE_EXPERIMENTAL

#include<GL/glew.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include "scolor.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "game.h"
#include "tiny_obj_loader.h"


namespace std {
	template<> struct hash<vertex> {
		size_t operator()(vertex const& v) const {
			return hash<glm::vec3>()(v.pos) ^ (hash<glm::vec2>()(v.tex_coord) << 1);
		}
	};
}

unsigned int load_texture(const char* filename){
	int width, height, channels;
	unsigned char *image_data = stbi_load(filename, &width, &height, &channels, 3);
	if(image_data){
		printf("Loaded image, %d by %d\n", width, height);
		unsigned int tex;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		// If you want to set parameters, call glTexParameteri (or similar)
 		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image_data);
		free(image_data);
		return tex;
	} else {
		printf(RED("Image failed to load:  %s\n").c_str(), filename);
		return 0;
	}
}

int load_model(std::vector<vertex> &vertices, std::vector<uint32_t> &indices, const char *filename, float scale, bool swap_yz){
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;
	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filename)) {
		printf("Loading failed!  %s\n", err.c_str());
		return 1;
	}

	std::unordered_map<vertex, uint32_t> uniqueVertices = {};

	for (const auto& shape : shapes) {
		for (const auto& index : shape.mesh.indices) {
			vertex new_vertex = {};
			new_vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]

			};
			/* Transform vertices if we need to */
			new_vertex.pos *= scale;
			if (swap_yz) {
				float tmp = new_vertex.pos.y;
				new_vertex.pos.y = new_vertex.pos.z;
				new_vertex.pos.z = tmp;
			}

			new_vertex.tex_coord = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
			};
			if (uniqueVertices.count(new_vertex) == 0) {
				uniqueVertices[new_vertex] = (uint32_t)vertices.size();
				vertices.push_back(new_vertex);
			}
			indices.push_back(uniqueVertices[new_vertex]);
		}
	}
	return 0;
}
