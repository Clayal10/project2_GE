
#ifndef GAME_H
#define GAME_H

#include<vector>

struct vertex {
	glm::vec3 pos;
	glm::vec2 tex_coord;

	bool operator==(const vertex& other) const {
		return pos == other.pos && tex_coord == other.tex_coord;
	}
}; // __attribute__((packed)); // TODO:  Alignment

unsigned int load_texture(const char* filename);
int load_model(std::vector<vertex> &verticies, std::vector<uint32_t> &indices, const char* filename);

#endif
