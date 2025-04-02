#pragma once

#include <array>
#include <unordered_map>
#include <vector>
#include <Model/Mesh.h>
#include <Model/Animation.h>
#include <string>

struct ModelFileHeader {
    uint32_t version;
    char vacant[60] = {};
};

struct EmbeddedTexture {
    uint32_t width;
    uint32_t height;
    std::vector<char> texture_buffer;
};

class Model {
public:
    std::vector<Mesh> meshes;
    std::string source;
    std::string directory;

    std::unordered_map<std::string, BoneIndex> bone_to_index_map;
    std::unordered_map<std::string, EmbeddedTexture> embedded_texture_map;
    std::vector<Animation> registered_animations;
    glm::mat4 global_inverse_transform = glm::mat4(1.0f);
    Skeleton skeleton;
};

void saveModel(const std::string& destination, Model* src_model);
void loadModelTester(const std::string& source, Model* result_model);

// Model getExampleModel();
