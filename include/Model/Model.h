#pragma once

#include <array>
#include <unordered_map>
#include <vector>
#include <Model/Mesh.h>
#include <Model/Animation.h>
#include <string>

class Model {
public:
    std::vector<Mesh> meshes;
    std::string source;
    std::string directory;

    std::unordered_map<std::string, uint32_t> bone_to_index_map;
    std::vector<Animation> registered_animations;
    glm::mat4 global_inverse_transform = glm::mat4(1.0f);
    Skeleton skeleton;
};

// Model getExampleModel();
