#include "model_loader.h"
#include <filesystem>
#include <span>
#include <cinttypes>

inline glm::mat4 aiMat4ToMat4(const aiMatrix4x4& matrix) {
    glm::mat4 result;
    result[0][0] = matrix.a1;
    result[1][0] = matrix.a2;
    result[2][0] = matrix.a3;
    result[3][0] = matrix.a4;
    result[0][1] = matrix.b1;
    result[1][1] = matrix.b2;
    result[2][1] = matrix.b3;
    result[3][1] = matrix.b4;
    result[0][2] = matrix.c1;
    result[1][2] = matrix.c2;
    result[2][2] = matrix.c3;
    result[3][2] = matrix.c4;
    result[0][3] = matrix.d1;
    result[1][3] = matrix.d2;
    result[2][3] = matrix.d3;
    result[3][3] = matrix.d4;
    return result;
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
            return ret;
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
            return ret;
    }
    return real_path;
}

std::vector<Texture> loadMaterialTextures(
    aiMaterial* mat, aiTextureType type, std::string typeName,
    std::vector<Texture>& textures_loaded, const std::string& directory
) {
    std::vector<Texture> textures;
    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
        aiString str;
        mat->GetTexture(type, i, &str);
        // check if texture was loaded before and if so, continue to next
        // iteration: skip loading a new texture
        bool skip = false;
        std::string real_path = getRelativePath(directory, str.C_Str());
        for (unsigned int j = 0; j < textures_loaded.size(); j++) {
            if (textures_loaded[j].path == real_path) {
                textures.push_back(textures_loaded[j]);
                skip =
                    true; // a texture with the same filepath has already been
                          // loaded, continue to next one. (optimization)
                break;
            }
        }
        if (!skip) { // if texture hasn't been loaded already, load it
            Texture texture;
            texture.type = typeName;
            texture.path = real_path;
            textures.push_back(texture);
            textures_loaded.push_back(texture
            ); // store it as texture loaded for entire model, to ensure we
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
            model.skeleton.bone_data.emplace_back(BoneData{
                .bone_offset_transform =
                    aiMat4ToMat4(mesh->mBones[i]->mOffsetMatrix)
            });
            bone_size = model.bone_to_index_map.size();
        }
        else {
            bone_id = model.bone_to_index_map[bone_name];
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
                if (vertices[j.mVertexId].weights[i] == 0.0f) {
                    vertices[j.mVertexId].weights[i] = j.mWeight;
                    vertices[j.mVertexId].bone_ids[i] = bone_id;
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
        material, aiTextureType_DIFFUSE, "texture_diffuse", textures_loaded,
        directory
    );
    textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    // 2. specular maps
    std::vector<Texture> specularMaps = loadMaterialTextures(
        material, aiTextureType_SPECULAR, "texture_specular", textures_loaded,
        directory
    );
    textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    // 3. normal maps
    std::vector<Texture> normalMaps = loadMaterialTextures(
        material, aiTextureType_HEIGHT, "texture_normal", textures_loaded,
        directory
    );
    textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
    // 4. height maps
    std::vector<Texture> heightMaps = loadMaterialTextures(
        material, aiTextureType_SHININESS, "texture_reflection",
        textures_loaded, directory
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
    std::unordered_map<std::string, uint32_t> hierarchy_cache;
    const aiAnimation* animation = scene->mAnimations[0];
    std::string node_name = node->mName.C_Str();
    glm::mat4 node_transform = aiMat4ToMat4(node->mTransformation);
    int current_index = parent_index;
    glm::mat4 global_transform = parent_transform * node_transform;
    if (model->bone_to_index_map.contains(node_name)) {
        uint32_t bone_data_index = model->bone_to_index_map[node_name];
        model->skeleton.bone_graph.emplace_back(BoneNode{
            .parent_index = static_cast<uint32_t>(parent_index),
            .bone_data_index = bone_data_index,
            .bone_node_transform = global_transform
        });
        current_index = model->skeleton.bone_graph.size() - 1;
        hierarchy_cache.emplace(node_name, current_index);
        if (parent_index != -1) {
            model->skeleton.bone_graph[parent_index]
                .children_index.emplace_back(current_index);
        }
        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            processAnimationNode(
                model, node->mChildren[i], scene, current_index
            );
        }
    }
    else {

        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            processAnimationNode(
                model, node->mChildren[i], scene, parent_index, global_transform
            );
        }
    }
}

void saveModel(const std::string& destination, Model* src_model) {
    std::fstream file(destination, std::ios::out | std::ios::binary);
    int size = src_model->meshes.size() * 3;
    int* mesh_sizes = new int[size];
    file.write((char*)&size, sizeof(int));
    for (int i = 0; i < size; i = i + 3) {
        mesh_sizes[i] = src_model->meshes[i / 3].vertices.size();
        mesh_sizes[i + 1] = src_model->meshes[i / 3].indices.size();
        mesh_sizes[i + 2] = src_model->meshes[i / 3].textures.size();
    }
    file.write((char*)&mesh_sizes[0], sizeof(int) * size);
    for (int i = 0; i < size; i = i + 3) {
        file.write(
            (char*)&src_model->meshes[i / 3].vertices[0],
            sizeof(Vertex) * mesh_sizes[i]
        );
        file.write(
            (char*)&src_model->meshes[i / 3].indices[0],
            sizeof(unsigned int) * mesh_sizes[i + 1]
        );
        for (int j = 0; j < mesh_sizes[i + 2]; ++j) {
            Texture& texture = src_model->meshes[i / 3].textures[j];
            file.write((char*)&texture.id, sizeof(int));
            file << '\n' << texture.type << '\n';
            file << '/' << texture.path << '\n';
        }
    }
    for (auto [bone_name, bone_index] : src_model->bone_to_index_map) {
        file.write(reinterpret_cast<char*>(&bone_index), sizeof(uint32_t));
        file << bone_name << '\n';
    }
    uint32_t map_delimiter = -1;
    file.write(reinterpret_cast<char*>(&map_delimiter), sizeof(uint32_t));
    file.write(
        reinterpret_cast<char*>(&src_model->global_inverse_transform),
        sizeof(glm::mat4)
    );
    uint32_t skeleton_vector_size = src_model->skeleton.bone_graph.size();
    if (src_model->skeleton.bone_data.size() != skeleton_vector_size) {
        // Should be error but fuck it we ball
        skeleton_vector_size = std::min(
            skeleton_vector_size,
            static_cast<uint32_t>(src_model->skeleton.bone_data.size())
        );
    }
    file.write(
        reinterpret_cast<char*>(&skeleton_vector_size), sizeof(uint32_t)
    );
    for (uint32_t i = 0; i < skeleton_vector_size; ++i) {
        file.write(
            reinterpret_cast<char*>(&src_model->skeleton.bone_graph[i]),
            sizeof(BoneNode) - offsetof(BoneNode, children_index)
        );
        file.write(
            reinterpret_cast<char*>(
                src_model->skeleton.bone_graph[i].children_index.data()
            ),
            sizeof(uint32_t)
                * src_model->skeleton.bone_graph[i].children_index.size()
        );
        file.write(reinterpret_cast<char*>(&map_delimiter), sizeof(uint32_t));
    }
    file.write(
        reinterpret_cast<char*>(src_model->skeleton.bone_graph.data()),
        sizeof(BoneNode) * skeleton_vector_size
    );
    file.write(
        reinterpret_cast<char*>(src_model->skeleton.bone_data.data()),
        sizeof(BoneData) * skeleton_vector_size
    );
    delete[] mesh_sizes;
    file.close();
}

bool loadModel(const std::string& source, Model* result_model) {
    Assimp::Importer import;
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
    result_model->directory = directory;
    result_model->directory = result_model->directory.substr(
        0, result_model->directory.find_last_of('/')
    );
    processNode(result_model, scene->mRootNode, scene, result_model->directory);
    result_model->skeleton.bone_data.resize(
        result_model->bone_to_index_map.size()
    );
    result_model->global_inverse_transform =
        glm::inverse(aiMat4ToMat4(scene->mRootNode->mTransformation));
    processAnimationNode(result_model, scene->mRootNode, scene);
    return true;
}

void loadModelTester(const std::string& source, Model* result_model) {
    std::ifstream file(source, std::ios::binary);
    int size{0};
    file.read((char*)&size, sizeof(int));
    if (!size) {
        result_model->directory = ' ';
        return;
    }
    std::unique_ptr<int[]> mesh_sizes(new int[size]);
    file.read((char*)mesh_sizes.get(), (sizeof(int) * size));
    result_model->meshes.reserve(size / 3);
    for (int i = 0; i < size; i = i + 3) {
        result_model->meshes.emplace_back(Mesh());
        Mesh& mesh = result_model->meshes.back();

        mesh.vertices.resize(mesh_sizes[i]);
        file.read((char*)mesh.vertices.data(), sizeof(Vertex) * mesh_sizes[i]);
        mesh.indices.resize(mesh_sizes[i + 1]);
        file.read(
            (char*)mesh.indices.data(), sizeof(unsigned int) * mesh_sizes[i + 1]
        );

        mesh.textures.reserve(mesh_sizes[i + 2]);
        for (int j = 0; j < mesh_sizes[i + 2]; ++j) {
            mesh.textures.emplace_back(Texture());
            Texture& texture = mesh.textures.back();
            // Texture texture;
            file.read((char*)&texture.id, sizeof(int));
            file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            file >> texture.type;
            file >> texture.path;
            file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            // mesh.textures.emplace_back(std::move(texture));
        }
        // model->meshes.emplace_back(std::move(mesh));
    }
    while (true) {
        std::string key;
        uint32_t value;
        file.read(reinterpret_cast<char*>(&value), sizeof(uint32_t));
        if (value == -1) {
            break;
        }
        file >> key;
        file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        result_model->bone_to_index_map.emplace(std::move(key), value);
    }
    file.read(
        reinterpret_cast<char*>(&result_model->global_inverse_transform),
        sizeof(glm::mat4)
    );
    uint32_t skeleton_vector_size = 0;
    file.read(reinterpret_cast<char*>(&skeleton_vector_size), sizeof(uint32_t));
    result_model->skeleton.bone_graph.resize(skeleton_vector_size);
    for (uint32_t i = 0; i < skeleton_vector_size; ++i) {
        file.read(
            reinterpret_cast<char*>(&result_model->skeleton.bone_graph[i]),
            sizeof(BoneNode) - offsetof(BoneNode, children_index)
        );
        while (true) {
            uint32_t child_index = -1;
            file.read(reinterpret_cast<char*>(&child_index), sizeof(uint32_t));
            if (child_index == -1) {
                break;
            }
            result_model->skeleton.bone_graph[i].children_index.emplace_back(
                child_index
            );
        }
    }
    file.read(
        reinterpret_cast<char*>(result_model->skeleton.bone_graph.data()),
        sizeof(BoneNode) * skeleton_vector_size
    );
    result_model->skeleton.bone_data.resize(skeleton_vector_size);
    file.read(
        reinterpret_cast<char*>(result_model->skeleton.bone_data.data()),
        sizeof(BoneData) * skeleton_vector_size
    );
    result_model->source = source;
    result_model->directory = source.substr(0, source.find_last_of('/'));
    file.close();
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
                value
            );
            success = false;
        }
        uint32_t test_value = test_model->bone_to_index_map[key];
        if (value != test_value) {
            printf(
                "Bone \"%s\": %" PRIu32 " != %" PRIu32 "\n", key.c_str(), value,
                test_value
            );
            success = false;
        }
    }
    return success;
}
