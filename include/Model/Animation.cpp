#include <Model/Animation.h>

bool operator==(const BoneIndex& bone, const BoneIndex& other) {
    return (bone.data_index == other.data_index)
           && (bone.graph_index == other.graph_index);
}
