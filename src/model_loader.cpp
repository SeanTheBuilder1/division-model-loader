#include "model_loader.h"
#include <filesystem>
#include <span>
#include <cinttypes>

inline glm::mat4 aiMat4ToMat4(const aiMatrix4x4& matrix) {
    return glm::transpose(glm::make_mat4(&matrix.a1));
}

std::string
getRelativePath(const std::string& directory, const std::string& path) {
    std::error_code ec;
    std::filesystem::path directory_path(
        directory + '/', std::filesystem::path::format::generic_format
    );
    std::filesystem::path real_path(
        directory_path.string() + path,
        std::filesystem::path::format::generic_format
    );
    std::filesystem::path absolute_real_path =
        std::filesystem::absolute(real_path, ec);
    if (!ec && std::filesystem::exists(absolute_real_path)
        && (real_path == absolute_real_path)) {
        auto ret =
            std::filesystem::relative(absolute_real_path, directory_path, ec);
        if (!ec)
            return ret.string();
    }

    real_path = std::filesystem::path(
        path, std::filesystem::path::format::generic_format
    );
    absolute_real_path = std::filesystem::absolute(real_path, ec);
    if (!ec && std::filesystem::exists(absolute_real_path)
        && (real_path == absolute_real_path)) {
        auto ret =
            std::filesystem::relative(absolute_real_path, directory_path, ec);
        if (!ec)
            return ret.string();
    }
    return real_path.string();
}

std::vector<Texture> loadMaterialTextures(
    aiMaterial* mat, const aiScene* scene, aiTextureType type,
    std::string typeName, std::vector<Texture>& textures_loaded,
    const std::string& directory, Model& model
) {
    std::vector<Texture> textures;
    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
        aiString str;
        mat->GetTexture(type, i, &str);
        const aiTexture* embedded_tex_ptr =
            scene->GetEmbeddedTexture(str.C_Str());
        std::string real_path;
        if (embedded_tex_ptr) {
            if (!model.embedded_texture_map.contains(str.C_Str())) {
                auto& embedded_tex = model.embedded_texture_map[str.C_Str()];
                embedded_tex.height = embedded_tex_ptr->mHeight;
                embedded_tex.width = embedded_tex_ptr->mWidth;
                if (embedded_tex.height == 0) {
                    size_t tex_size = embedded_tex.width;
                    embedded_tex.texture_buffer.resize(tex_size);
                    memcpy(
                        embedded_tex.texture_buffer.data(),
                        embedded_tex_ptr->pcData, tex_size
                    );
                }
                else {
                    size_t tex_size = embedded_tex.width * embedded_tex.height
                                      * sizeof(aiTexel);
                    embedded_tex.texture_buffer.resize(tex_size);
                    memcpy(
                        embedded_tex.texture_buffer.data(),
                        embedded_tex_ptr->pcData, tex_size
                    );
                }
            }
            real_path = str.C_Str();
        }
        else {
            real_path = getRelativePath(directory, str.C_Str());
        }

        // check if texture was loaded before and if so, continue to next
        // iteration: skip loading a new texture
        bool skip = false;
        for (unsigned int j = 0; j < textures_loaded.size(); j++) {
            if (textures_loaded[j].path == real_path) {
                textures.push_back(textures_loaded[j]);
                skip = true;
                // a texture with the same filepath has already been
                // loaded, continue to next one. (optimization)
                break;
            }
        }
        if (!skip) { // if texture hasn't been loaded already, load it
            Texture texture;
            texture.type = typeName;
            texture.path = real_path;
            textures.push_back(texture);
            textures_loaded.push_back(texture);
            // store it as texture loaded for entire model, to ensure we
            // won't unnecessary load duplicate textures.
        }
    }
    return textures;
}

Mesh processMesh(
    aiMesh* mesh, const aiScene* scene, const std::string& directory,
    Model& model
) {
    // data to fill
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;
    std::vector<Texture> textures_loaded;

    // walk through each of the mesh's vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
        glm::vec3 vector; // we declare a placeholder vector since assimp uses
                          // its own vector class that doesn't directly convert
                          // to glm's vec3 class so we transfer the data to this
                          // placeholder glm::vec3 first.
        // positions
        vector.x = mesh->mVertices[i].x;
        vector.y = mesh->mVertices[i].y;
        vector.z = mesh->mVertices[i].z;
        vertex.position = vector;
        // normals
        if (mesh->HasNormals()) {
            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.normal = vector;
        }
        // texture coordinates
        if (mesh->mTextureCoords[0]) // does the mesh contain texture
                                     // coordinates?
        {
            glm::vec2 vec;
            // a vertex can contain up to 8 different texture coordinates. We
            // thus make the assumption that we won't use models where a vertex
            // can have multiple texture coordinates so we always take the first
            // set (0).
            vec.x = mesh->mTextureCoords[0][i].x;
            vec.y = mesh->mTextureCoords[0][i].y;
            vertex.tex_coord = vec;
            // // tangent
            // vector.x = mesh->mTangents[i].x;
            // vector.y = mesh->mTangents[i].y;
            // vector.z = mesh->mTangents[i].z;
            // vertex.tangent = vector;
            // // bitangent
            // vector.x = mesh->mBitangents[i].x;
            // vector.y = mesh->mBitangents[i].y;
            // vector.z = mesh->mBitangents[i].z;
            // vertex.bitangent = vector;
        }
        else {
            vertex.tex_coord = glm::vec2(0.0f, 0.0f);
        }
        vertices.push_back(vertex);
    }
    size_t bone_size = model.bone_to_index_map.size();
    for (int i = 0; i < mesh->mNumBones; ++i) {
        uint32_t bone_id;
        std::string bone_name = mesh->mBones[i]->mName.C_Str();
        if (!model.bone_to_index_map.contains(bone_name)) {
            model.bone_to_index_map.emplace(bone_name, bone_size);
            bone_id = bone_size;
            model.skeleton.bone_data.emplace_back(
                BoneData{
                    .bone_offset_transform =
                        aiMat4ToMat4(mesh->mBones[i]->mOffsetMatrix)
                }
            );
            bone_size = model.bone_to_index_map.size();
        }
        else {
            bone_id = model.bone_to_index_map[bone_name].data_index;
        }
        auto weight_span =
            std::span{mesh->mBones[i]->mWeights, mesh->mBones[i]->mNumWeights};
        for (auto& j : weight_span) {
            if (j.mVertexId >= vertices.size()) {
                std::cerr << "WARN: Bone Vertex ID "
                                 + std::to_string(j.mVertexId)
                                 + " is out of bounds of vertex array(size: "
                                 + std::to_string(vertices.size())
                                 + "), not adding vertex\n";
                continue;
            }
            for (int k = 0; k < MAX_BONE_INFLUENCE; ++k) {
                if (vertices[j.mVertexId].weights[k] == 0.0f) {
                    vertices[j.mVertexId].weights[k] = j.mWeight;
                    vertices[j.mVertexId].bone_ids[k] = bone_id;
                    break;
                }
                if (k == MAX_BONE_INFLUENCE - 1) {
                    std::cerr
                        << "WARN: Max bones reached for vertex, not adding\n";
                }
            }
        }
    }
    // now wak through each of the mesh's faces (a face is a mesh its triangle)
    // and retrieve the corresponding vertex indices.
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        // retrieve all indices of the face and store them in the indices vector
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }

    // process materials
    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
    // we assume a convention for sampler names in the shaders. Each diffuse
    // texture should be named as 'texture_diffuseN' where N is a sequential
    // number ranging from 1 to MAX_SAMPLER_NUMBER. Same applies to other
    // texture as the following list summarizes: diffuse: texture_diffuseN
    // specular: texture_specularN
    // normal: texture_normalN

    // 1. diffuse maps
    std::vector<Texture> diffuseMaps = loadMaterialTextures(
        material, scene, aiTextureType_DIFFUSE, "texture_diffuse",
        textures_loaded, directory, model
    );
    textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    // 2. specular maps
    std::vector<Texture> specularMaps = loadMaterialTextures(
        material, scene, aiTextureType_SPECULAR, "texture_specular",
        textures_loaded, directory, model
    );
    textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    // 3. normal maps
    std::vector<Texture> normalMaps = loadMaterialTextures(
        material, scene, aiTextureType_HEIGHT, "texture_normal",
        textures_loaded, directory, model
    );
    textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
    // 4. height maps
    std::vector<Texture> heightMaps = loadMaterialTextures(
        material, scene, aiTextureType_SHININESS, "texture_reflection",
        textures_loaded, directory, model
    );
    textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());

    // return a mesh object created from the extracted mesh data
    return Mesh(vertices, indices, textures);
}

void processNode(
    Model* model, aiNode* node, const aiScene* scene,
    const std::string& directory
) {
    // process all the node's meshes (if any)
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        model->meshes.push_back(processMesh(mesh, scene, directory, *model));
    }
    // then do the same for each of its children
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(model, node->mChildren[i], scene, directory);
    }
}

void processAnimationNode(
    Model* model, aiNode* node, const aiScene* scene,
    const uint32_t& parent_index = -1,
    const glm::mat4& parent_transform = glm::mat4(1.0f)
) {
    std::string node_name = node->mName.C_Str();
    const glm::mat4 node_transform = aiMat4ToMat4(node->mTransformation);
    uint32_t current_index = parent_index;
    glm::mat4 global_transform = parent_transform * node_transform;
    if (model->bone_to_index_map.contains(node_name)) {
        uint32_t bone_data_index =
            model->bone_to_index_map[node_name].data_index;
        model->skeleton.bone_graph.emplace_back(
            BoneNode{
                .parent_index = parent_index,
                .bone_data_index = bone_data_index,
                .bone_node_transform = node_transform,
                .children_index = {}
            }
        );
        current_index = model->skeleton.bone_graph.size() - 1;
        if (parent_index != -1) {
            model->skeleton.bone_graph[parent_index]
                .children_index.emplace_back(current_index);
        }
    }
    else {
        model->skeleton.bone_graph.emplace_back(
            BoneNode{
                .parent_index = parent_index,
                .bone_data_index = static_cast<uint32_t>(-1),
                .bone_node_transform = node_transform,
                .children_index = {}
            }
        );
        current_index = model->skeleton.bone_graph.size() - 1;
        model->bone_to_index_map.emplace(
            node_name,
            BoneIndex{
                .data_index = static_cast<uint32_t>(-1),
                .graph_index = current_index
            }
        );
        if (parent_index != -1) {
            model->skeleton.bone_graph[parent_index]
                .children_index.emplace_back(current_index);
        }
    }
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processAnimationNode(
            model, node->mChildren[i], scene, current_index, global_transform
        );
    }
}

Vec3Keyframe aiVectorKeyToKeyframe(const aiVectorKey& key) {
    return {
        .vector = {key.mValue.x, key.mValue.y, key.mValue.z},
        .keyframe_tick = key.mTime
    };
}

QuatKeyframe aiQuatKeyToKeyframe(const aiQuatKey& key) {
    return {
        .quat = {key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z},
        .keyframe_tick = key.mTime
    };
}

void processAnimations(Model* model, const aiScene* scene) {
    std::span<aiAnimation*> animations_span(
        scene->mAnimations, scene->mNumAnimations
    );
    for (auto& animation : animations_span) {
        Animation& current_animation =
            model->registered_animations.emplace_back();
        current_animation.name = animation->mName.C_Str();
        current_animation.duration_ticks = animation->mDuration;
        current_animation.tick_rate_hz = animation->mTicksPerSecond;
        std::span<aiNodeAnim*> channel_span(
            animation->mChannels, animation->mNumChannels
        );
        for (auto& channel : channel_span) {
            std::string bone_to_find = channel->mNodeName.C_Str();
            if (!model->bone_to_index_map.contains(bone_to_find)) {
                std::cerr << "WARN: Bone \"" << bone_to_find
                          << "\" not found in bone map, ignoring channel\n";
                continue;
            }
            AnimationChannel& current_channel =
                current_animation.channels.emplace_back();
            current_channel.bone_index = model->bone_to_index_map[bone_to_find];
            std::span<aiVectorKey> pos_key_span(
                channel->mPositionKeys, channel->mNumPositionKeys
            );
            for (auto& position : pos_key_span) {
                current_channel.position.emplace_back(
                    aiVectorKeyToKeyframe(position)
                );
            }
            std::span<aiQuatKey> rotation_key_span(
                channel->mRotationKeys, channel->mNumRotationKeys
            );
            for (auto& rotation : rotation_key_span) {
                current_channel.rotation.emplace_back(
                    aiQuatKeyToKeyframe(rotation)
                );
            }
            std::span<aiVectorKey> scaling_key_span(
                channel->mScalingKeys, channel->mNumScalingKeys
            );
            for (auto& scaling : scaling_key_span) {
                current_channel.scale.emplace_back(
                    aiVectorKeyToKeyframe(scaling)
                );
            }
        }
    }
}

bool loadModel(const std::string& source, Model* result_model) {
    Assimp::Importer import;
    import.SetPropertyBool(AI_CONFIG_IMPORT_REMOVE_EMPTY_BONES, false);
    const aiScene* scene =
        import.ReadFile(source, aiProcess_Triangulate | aiProcess_FlipUVs);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE
        || !scene->mRootNode) {
        std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << std::endl;
        return false;
    }
    result_model->directory = source.substr(0, source.find_last_of('/'));
    std::filesystem::path directory(
        result_model->directory, std::filesystem::path::format::generic_format
    );
    std::error_code ec; // ignore error
    directory = std::filesystem::absolute(directory, ec);
    result_model->directory = directory.string();
    result_model->directory = result_model->directory.substr(
        0, result_model->directory.find_last_of('/')
    );
    processNode(result_model, scene->mRootNode, scene, result_model->directory);
    result_model->global_inverse_transform =
        glm::inverse(aiMat4ToMat4(scene->mRootNode->mTransformation));
    processAnimationNode(result_model, scene->mRootNode, scene);
    processAnimations(result_model, scene);
    return true;
}

bool compareSavedModel(Model* original_model, Model* test_model) {
    if (original_model->meshes.size() != test_model->meshes.size()) {
        printf(
            "Model %s does not match mesh size from original\n%zu meshes vs. "
            "%zu "
            "meshes\n",
            original_model->directory.c_str(), original_model->meshes.size(),
            test_model->meshes.size()
        );
        return false;
    }
    bool success = true;
    for (int i = 0; i < original_model->meshes.size(); ++i) {
        if (original_model->meshes[i].vertices.size()
            != test_model->meshes[i].vertices.size()) [[unlikely]] {
            printf(
                "Model %s does not match vertices size from original\n%zu "
                "vertices "
                "vs. %zu vertices\n",
                original_model->directory.c_str(),
                original_model->meshes[i].vertices.size(),
                test_model->meshes[i].vertices.size()
            );
            return false;
        }
        for (int j = 0; j < original_model->meshes[i].vertices.size(); ++j) {
            std::vector<Vertex> vertices1 = original_model->meshes[i].vertices;
            std::vector<Vertex> vertices2 = test_model->meshes[i].vertices;
            if (vertices1[j].position != vertices2[j].position) [[unlikely]] {
                success = false;
                printf(
                    "position %f, %f, %f != %f, %f, %f\n",
                    vertices1[j].position.x, vertices1[j].position.y,
                    vertices1[j].position.z, vertices2[j].position.x,
                    vertices2[j].position.y, vertices2[j].position.z
                );
            }
            if (vertices1[j].normal != vertices2[j].normal) [[unlikely]] {
                success = false;
                printf(
                    "normal: %f, %f, %f != %f, %f, %f\n", vertices1[j].normal.x,
                    vertices1[j].normal.y, vertices1[j].normal.z,
                    vertices2[j].normal.x, vertices2[j].normal.y,
                    vertices2[j].normal.z
                );
            }

            if (vertices1[j].tex_coord != vertices2[j].tex_coord) [[unlikely]] {
                success = false;
                printf(
                    "tex_coord: %f, %f != %f, %f\n", vertices1[j].tex_coord.x,
                    vertices1[j].tex_coord.y, vertices2[j].tex_coord.x,
                    vertices2[j].tex_coord.y
                );
            }

            if (vertices1[j].tangent != vertices2[j].tangent) [[unlikely]] {
                success = false;
                printf(
                    "tangent: %f, %f, %f != %f, %f, %f\n",
                    vertices1[j].tangent.x, vertices1[j].tangent.y,
                    vertices1[j].tangent.z, vertices2[j].tangent.x,
                    vertices2[j].tangent.y, vertices2[j].tangent.z
                );
            }
            if (vertices1[j].bitangent != vertices2[j].bitangent) [[unlikely]] {
                success = false;
                printf(
                    "bitangent: %f, %f, %f != %f, %f, %f\n",
                    vertices1[j].bitangent.x, vertices1[j].bitangent.y,
                    vertices1[j].bitangent.z, vertices2[j].bitangent.x,
                    vertices2[j].bitangent.y, vertices2[j].bitangent.z
                );
            }
        }
        if (original_model->meshes[i].indices.size()
            != test_model->meshes[i].indices.size()) [[unlikely]] {
            printf(
                "Model %s does not match indices size from original\n%zu "
                "indices "
                "vs. %zu indices\n",
                original_model->directory.c_str(),
                original_model->meshes[i].indices.size(),
                test_model->meshes[i].indices.size()
            );
            return false;
        }
        for (int j = 0; j < original_model->meshes[i].indices.size(); ++j) {
            if (original_model->meshes[i].indices[j]
                != test_model->meshes[i].indices[j]) {
                printf(
                    "indices: %d != %d", original_model->meshes[i].indices[j],
                    test_model->meshes[i].indices[j]
                );
                success = false;
            }
        }
    }
    if (original_model->bone_to_index_map.size()
        != test_model->bone_to_index_map.size()) {
        printf(
            "Model %s does not match bone map size from original\n%zu bones "
            "vs. "
            "%zu "
            "bones\n",
            original_model->directory.c_str(),
            original_model->bone_to_index_map.size(),
            test_model->bone_to_index_map.size()
        );
        return false;
    }
    for (auto [key, value] : original_model->bone_to_index_map) {
        if (!test_model->bone_to_index_map.contains(key)) {
            printf(
                "Model %s does not have value for key %s in bone map\nValue of "
                "%s in original model is %" PRIu32 "\n",
                original_model->directory.c_str(), key.c_str(), key.c_str(),
                value.data_index
            );
            success = false;
        }
        uint32_t test_value = test_model->bone_to_index_map[key].data_index;
        if (value.data_index != test_value) {
            printf(
                "Bone \"%s\": %" PRIu32 " != %" PRIu32 "\n", key.c_str(),
                value.data_index, test_value
            );
            success = false;
        }
    }
    if (test_model->skeleton.bone_graph.size()
        != original_model->skeleton.bone_graph.size()) {

        printf(
            "Model %s does not match bone graph size from original\n%zu bone "
            "nodes vs. "
            "%zu "
            "bone nodes\n",
            original_model->directory.c_str(),
            original_model->skeleton.bone_graph.size(),
            test_model->skeleton.bone_graph.size()
        );
        return false;
    }
    if (test_model->skeleton.bone_data.size()
        != original_model->skeleton.bone_data.size()) {

        printf(
            "Model %s does not match bone data size from original\n%zu bone "
            "data vs. "
            "%zu "
            "bone data\n",
            original_model->directory.c_str(),
            original_model->skeleton.bone_data.size(),
            test_model->skeleton.bone_data.size()
        );
        return false;
    }
    for (int i = 0; i < original_model->skeleton.bone_data.size(); ++i) {
        BoneData& original_bone_data = original_model->skeleton.bone_data[i];
        BoneData& test_bone_data = test_model->skeleton.bone_data[i];
        if (memcmp(&original_bone_data, &test_bone_data, sizeof(BoneData))
            != 0) {
            printf(
                "Model %s bone data index %d does not match each other\n",
                original_model->directory.c_str(), i
            );
            success = false;
        }
    }
    for (int i = 0; i < original_model->skeleton.bone_graph.size(); ++i) {
        BoneNode& original_bone_node = original_model->skeleton.bone_graph[i];
        BoneNode& test_bone_node = test_model->skeleton.bone_graph[i];
        if (memcmp(
                &original_bone_node, &test_bone_node,
                offsetof(BoneNode, children_index)
            )
            != 0) {
            printf(
                "model %s bone graph index %d does not match each other\n",
                original_model->directory.c_str(), i
            );
            success = false;
        }
        if (original_bone_node.children_index.size()
            != test_bone_node.children_index.size()) {

            printf(
                "Model %s children list in bone graph mismatch for index %d, "
                "size from original\n%zu "
                "children "
                "size vs. "
                "%zu "
                "children size\n",
                original_model->directory.c_str(), i,
                original_bone_node.children_index.size(),
                test_bone_node.children_index.size()
            );
            return false;
        }
        if (memcmp(
                original_bone_node.children_index.data(),
                test_bone_node.children_index.data(),
                sizeof(uint32_t) * original_bone_node.children_index.size()
            )
            != 0) {
            printf(
                "Model %s bone graph index %d children list does not match "
                "each other\n",
                original_model->directory.c_str(), i
            );
            success = false;
        }
    }
    for (auto [key, value] : original_model->embedded_texture_map) {
        if (!test_model->embedded_texture_map.contains("/" + key)) {
            printf(
                "Model %s does not have value for key %s in embedded texture "
                "map\nValue of "
                "%s in original model is width: %" PRIu32 " height: %" PRIu32
                "\n",
                original_model->directory.c_str(), key.c_str(), key.c_str(),
                value.width, value.height
            );
            success = false;
        }
        EmbeddedTexture& test_value =
            test_model->embedded_texture_map["/" + key];
        if (value.width != test_value.width) {
            printf(
                "Texture width for \"%s\": %" PRIu32 " != %" PRIu32 "\n",
                key.c_str(), value.width, test_value.width
            );
            success = false;
        }
        if (value.height != test_value.height) {
            printf(
                "Texture height for \"%s\": %" PRIu32 " != %" PRIu32 "\n",
                key.c_str(), value.height, test_value.height
            );
            success = false;
        }
        if (value.texture_buffer.size() != test_value.texture_buffer.size()) {
            uint32_t value_size = value.texture_buffer.size();
            uint32_t test_value_size = value.texture_buffer.size();
            printf(
                "Texture buffer size for \"%s\": %" PRIu32 " != %" PRIu32 "\n",
                key.c_str(), value_size, test_value_size
            );
            success = false;
        }
        else {
            uint32_t value_size = value.texture_buffer.size();
            if (memcmp(
                    value.texture_buffer.data(),
                    test_value.texture_buffer.data(), sizeof(char) * value_size
                )
                != 0) {
                printf(
                    "Texture buffer not equal somehow for \"%s\"\n", key.c_str()
                );
                success = false;
            }
        }
    }
    return success;
}
