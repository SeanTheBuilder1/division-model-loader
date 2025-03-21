#pragma once

#include <vector>
#include <string>
#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>
#include <memory>
#include <string>

#define MAX_BONE_INFLUENCE 4
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 tex_coord;
    glm::vec3 tangent;
    glm::vec3 bitangent;
    //
    int bone_ids[MAX_BONE_INFLUENCE] = {};
    float weights[MAX_BONE_INFLUENCE] = {};
};

struct Texture {
    unsigned int id{0};
    std::string type;
    std::string path;
};

class Mesh {
public:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;

    unsigned int VAO{0};
    unsigned int VBO{0};
    unsigned int EBO{0};
};
