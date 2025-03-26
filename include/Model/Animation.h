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
};

struct Skeleton {
    // Actual in production
    std::vector<BoneNode> bone_graph;
    std::vector<BoneData> bone_data;

    // Like Intermediate Representation
    std::vector<BoneIntermediate> bone_intermediate_representation;
};

struct BoneIndex {
    uint32_t data_index = -1;
    uint32_t graph_index = -1;
};

bool operator==(const BoneIndex& bone, const BoneIndex& other);

struct Vec3Keyframe {
    glm::vec3 vector;
    double keyframe_tick;
};

struct QuatKeyframe {
    glm::quat quat;
    double keyframe_tick;
};

struct AnimationChannel {
    BoneIndex bone_index;
    std::vector<Vec3Keyframe> position;
    std::vector<QuatKeyframe> rotation;
    std::vector<Vec3Keyframe> scale;
};

// struct AnimationChannelIntermediate {
//     std::vector<Vec3Keyframe> position;
//     std::vector<QuatKeyframe> rotation;
//     std::vector<Vec3Keyframe> scale;
// };

struct Animation {
    std::string name;
    double tick_rate_hz;
    double duration_ticks;
    std::vector<AnimationChannel> channels;
};

struct AnimationIntermediate {
    uint32_t tickrate;
    float duration_seconds;
};

void importAnimation();
