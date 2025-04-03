#include <Model/Model.h>
#include <filesystem>
#include <fstream>
#include <iostream>

void loadAnimationTester(std::ifstream& file, Model* result_model) {
    uint32_t size;
    file.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
    result_model->registered_animations.resize(size);
    for (auto& animation : result_model->registered_animations) {

        std::getline(file, animation.name);
        file.read(
            reinterpret_cast<char*>(&animation.tick_rate_hz), sizeof(double)
        );
        file.read(
            reinterpret_cast<char*>(&animation.duration_ticks), sizeof(double)
        );
        file.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
        animation.channels.resize(size);
        for (auto& channel : animation.channels) {
            std::string bone_to_find;
            std::getline(file, bone_to_find);
            BoneIndex bone_index;
            if (!result_model->bone_to_index_map.contains(bone_to_find)) {
                std::cerr << "ERROR: Bone \"" << bone_to_find
                          << "\" not found in bone map\n";
                std::exit(-1);
            }
            channel.bone_index = result_model->bone_to_index_map[bone_to_find];
            file.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
            channel.position.resize(size);
            file.read(
                reinterpret_cast<char*>(channel.position.data()),
                sizeof(Vec3Keyframe) * size
            );
            file.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
            channel.rotation.resize(size);
            file.read(
                reinterpret_cast<char*>(channel.rotation.data()),
                sizeof(QuatKeyframe) * size
            );
            file.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
            channel.scale.resize(size);
            file.read(
                reinterpret_cast<char*>(channel.scale.data()),
                sizeof(Vec3Keyframe) * size
            );
        }
    }
}

void saveAnimation(std::fstream& file, Model* src_model) {
    uint32_t size = src_model->registered_animations.size();
    file.write(reinterpret_cast<char*>(&size), sizeof(uint32_t));
    for (auto& animation : src_model->registered_animations) {

        file << animation.name << '\n';
        file.write(
            reinterpret_cast<char*>(&animation.tick_rate_hz), sizeof(double)
        );
        file.write(
            reinterpret_cast<char*>(&animation.duration_ticks), sizeof(double)
        );
        size = animation.channels.size();
        file.write(reinterpret_cast<char*>(&size), sizeof(uint32_t));
        for (auto& channel : animation.channels) {
            BoneIndex bone_index_to_find = channel.bone_index;
            std::string bone_name;
            for (auto [name, id] : src_model->bone_to_index_map) {
                if (id == bone_index_to_find) {
                    bone_name = name;
                }
            }
            if (bone_name.empty()) {
                std::cerr << "ERROR: Bone id{" << bone_index_to_find.data_index
                          << ", " << bone_index_to_find.graph_index
                          << "} not found in bone map\n";
                std::exit(-1);
            }
            file << bone_name << '\n';
            size = channel.position.size();
            file.write(reinterpret_cast<char*>(&size), sizeof(uint32_t));
            file.write(
                reinterpret_cast<char*>(channel.position.data()),
                sizeof(Vec3Keyframe) * size
            );
            size = channel.rotation.size();
            file.write(reinterpret_cast<char*>(&size), sizeof(uint32_t));
            file.write(
                reinterpret_cast<char*>(channel.rotation.data()),
                sizeof(QuatKeyframe) * size
            );
            size = channel.scale.size();
            file.write(reinterpret_cast<char*>(&size), sizeof(uint32_t));
            file.write(
                reinterpret_cast<char*>(channel.scale.data()),
                sizeof(Vec3Keyframe) * size
            );
        }
    }
}

void loadEmbeddedTextureMap(std::ifstream& file, Model* result_model) {
    while (true) {
        if (!file.good() || file.eof()) {
            std::cerr << "ERROR: Reading embedded texture map failed\n";
            break;
        }
        std::string key;
        EmbeddedTexture tex;
        file.read(reinterpret_cast<char*>(&tex.width), sizeof(uint32_t));
        file.read(reinterpret_cast<char*>(&tex.height), sizeof(uint32_t));
        uint32_t size = UINT32_MAX;
        file.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
        if (tex.height == UINT32_MAX && tex.width == UINT32_MAX
            && size == UINT32_MAX) {
            break;
        }
        std::getline(file, key);
        tex.texture_buffer.resize(size);
        file.read(tex.texture_buffer.data(), sizeof(char) * size);
        result_model->embedded_texture_map.emplace(
            std::move(key), std::move(tex)
        );
    }
}

void saveEmbeddedTextureMap(std::fstream& file, Model* src_model) {
    for (auto [path, tex] : src_model->embedded_texture_map) {
        file.write(reinterpret_cast<char*>(&tex.width), sizeof(uint32_t));
        file.write(reinterpret_cast<char*>(&tex.height), sizeof(uint32_t));
        uint32_t size = tex.texture_buffer.size();
        file.write(reinterpret_cast<char*>(&size), sizeof(uint32_t));
        file << '/' << path << '\n';
        file.write(tex.texture_buffer.data(), sizeof(char) * size);
        // printf("LOL: %s\n%u,%u,%u", path.c_str(), tex.width, tex.height,
        // size);
    }
    uint32_t map_delimiter = UINT32_MAX;
    file.write(reinterpret_cast<char*>(&map_delimiter), sizeof(uint32_t));
    file.write(reinterpret_cast<char*>(&map_delimiter), sizeof(uint32_t));
    file.write(reinterpret_cast<char*>(&map_delimiter), sizeof(uint32_t));
}

void loadModelTester(const std::string& source, Model* result_model) {
    std::ifstream file(source, std::ios::binary);
    result_model->source = source;
    std::filesystem::path directory(
        result_model->source, std::filesystem::path::format::generic_format
    );
    result_model->directory = directory.generic_string();
    result_model->directory = result_model->directory.substr(
        0, result_model->directory.find_last_of('/')
    );
    ModelFileHeader header;
    // Version 1: Initial Version, only meshes
    // Version 2: Add 64 byte header, add skeleton support
    file.read(reinterpret_cast<char*>(&header), sizeof(ModelFileHeader));
    int size{0};
    file.read((char*)&size, sizeof(int));
    if (!size) {
        result_model->directory = ' ';
        return;
    }
    std::unique_ptr<int[]> mesh_sizes = std::make_unique<int[]>(size);
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
            std::getline(file, texture.type);
            std::getline(file, texture.path);
        }
    }
    while (true) {
        if (!file.good() || file.eof()) {
            std::cerr << "ERROR: Reading skeleton map failed\n";
            break;
        }
        std::string key;
        BoneIndex value;
        file.read(reinterpret_cast<char*>(&value), sizeof(BoneIndex));
        if (value.data_index == UINT32_MAX && value.graph_index == UINT32_MAX) {
            break;
        }
        std::getline(file, key);
        result_model->bone_to_index_map.emplace(std::move(key), value);
    }
    if (result_model->bone_to_index_map.empty()) {
        file.close();
        return;
    }
    if (!file.good() || file.eof()) {
        file.close();
        return;
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
            offsetof(BoneNode, children_index)
        );
        while (true) {
            uint32_t child_index = UINT32_MAX;
            file.read(reinterpret_cast<char*>(&child_index), sizeof(uint32_t));
            if (child_index == UINT32_MAX) {
                break;
            }
            result_model->skeleton.bone_graph[i].children_index.emplace_back(
                child_index
            );
        }
    }
    file.read(reinterpret_cast<char*>(&skeleton_vector_size), sizeof(uint32_t));
    result_model->skeleton.bone_data.resize(skeleton_vector_size);
    file.read(
        reinterpret_cast<char*>(result_model->skeleton.bone_data.data()),
        sizeof(BoneData) * skeleton_vector_size
    );
    loadAnimationTester(file, result_model);
    if (header.version >= 3) {
        loadEmbeddedTextureMap(file, result_model);
        if (!file.good() || file.eof()) {
            file.close();
            return;
        }
    }
    file.close();
}

void saveModel(const std::string& destination, Model* src_model) {
    std::fstream file(destination, std::ios::out | std::ios::binary);
    ModelFileHeader header;
    // Version 1: Initial Version, only meshes
    // Version 2: Add 64 byte header, add skeleton support
    header.version = 3;
    file.write(reinterpret_cast<char*>(&header), sizeof(ModelFileHeader));
    int size = src_model->meshes.size() * 3;
    std::unique_ptr<int[]> mesh_sizes = std::make_unique<int[]>(size);
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
        file.write(reinterpret_cast<char*>(&bone_index), sizeof(BoneIndex));
        file << bone_name << '\n';
    }
    uint32_t map_delimiter = UINT32_MAX;
    BoneIndex bone_index_map_delimiter = {
        .data_index = UINT32_MAX, .graph_index = UINT32_MAX
    };
    file.write(
        reinterpret_cast<char*>(&bone_index_map_delimiter), sizeof(BoneIndex)
    );
    if (src_model->bone_to_index_map.empty()) {
        file.close();
        return;
    }
    file.write(
        reinterpret_cast<char*>(&src_model->global_inverse_transform),
        sizeof(glm::mat4)
    );
    uint32_t skeleton_vector_size = src_model->skeleton.bone_graph.size();
    file.write(
        reinterpret_cast<char*>(&skeleton_vector_size), sizeof(uint32_t)
    );
    for (uint32_t i = 0; i < skeleton_vector_size; ++i) {
        file.write(
            reinterpret_cast<char*>(&src_model->skeleton.bone_graph[i]),
            offsetof(BoneNode, children_index)
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
    skeleton_vector_size = src_model->skeleton.bone_data.size();
    file.write(
        reinterpret_cast<char*>(&skeleton_vector_size), sizeof(uint32_t)
    );
    file.write(
        reinterpret_cast<char*>(src_model->skeleton.bone_data.data()),
        sizeof(BoneData) * skeleton_vector_size
    );
    saveAnimation(file, src_model);
    saveEmbeddedTextureMap(file, src_model);

    file.close();
}
