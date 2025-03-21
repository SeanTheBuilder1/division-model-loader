#pragma once

#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>
#include <string>
#include <vector>
#include <cstdint>

struct BoneNode {
    uint32_t parent_index = -1;
    uint32_t bone_data_index = -1;
    glm::mat4 bone_node_transform = glm::mat4(1.0f);
    std::vector<uint32_t> children_index;
};

struct BoneIntermediate {
    std::vector<uint32_t> children_index;
    uint32_t parent_index = -1;
    glm::mat4 bone_offset = glm::mat4(1.0f);
    std::string bone_name;
};

struct BoneData {
    glm::mat4 bone_offset_transform = glm::mat4(1.0f);
    glm::mat4 final_transform = glm::mat4(1.0f);
};

struct Skeleton {
    // Actual in production
    std::vector<BoneNode> bone_graph;
    std::vector<BoneData> bone_data;

    // Like Intermediate Representation
    std::vector<BoneIntermediate> bone_intermediate_representation;
};

struct AnimationKeyframeData {
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;
};

struct AnimationKeyframe {
    std::vector<AnimationKeyframe> bone_keyfame;
};

struct Animation {
    uint32_t tick_rate;
    float duration_seconds;
    std::vector<AnimationKeyframe> keyframes;
};

struct AnimationIntermediate {
    uint32_t tickrate;
    float duration_seconds;
};

void importAnimation();
