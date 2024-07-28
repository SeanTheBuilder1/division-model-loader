#include "model_loader.h"

std::vector<Texture> loadMaterialTextures(
    aiMaterial* mat, aiTextureType type, std::string typeName,
    std::vector<Texture>& textures_loaded
) {
    std::vector<Texture> textures;
    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
        aiString str;
        mat->GetTexture(type, i, &str);
        // check if texture was loaded before and if so, continue to next
        // iteration: skip loading a new texture
        bool skip = false;
        for (unsigned int j = 0; j < textures_loaded.size(); j++) {
            if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0) {
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
            texture.path = str.C_Str();
            textures.push_back(texture);
            textures_loaded.push_back(texture
            ); // store it as texture loaded for entire model, to ensure we
               // won't unnecessary load duplicate textures.
        }
    }
    return textures;
}

Mesh processMesh(aiMesh* mesh, const aiScene* scene) {
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
        material, aiTextureType_DIFFUSE, "texture_diffuse", textures_loaded
    );
    textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    // 2. specular maps
    std::vector<Texture> specularMaps = loadMaterialTextures(
        material, aiTextureType_SPECULAR, "texture_specular", textures_loaded
    );
    textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    // 3. normal maps
    std::vector<Texture> normalMaps = loadMaterialTextures(
        material, aiTextureType_HEIGHT, "texture_normal", textures_loaded
    );
    textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
    // 4. height maps
    std::vector<Texture> heightMaps = loadMaterialTextures(
        material, aiTextureType_SHININESS, "texture_reflection", textures_loaded
    );
    textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());

    // return a mesh object created from the extracted mesh data
    return Mesh(vertices, indices, textures);
}

void processNode(Model* model, aiNode* node, const aiScene* scene) {
    // process all the node's meshes (if any)
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        model->meshes.push_back(processMesh(mesh, scene));
    }
    // then do the same for each of its children
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(model, node->mChildren[i], scene);
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

    processNode(result_model, scene->mRootNode, scene);
    return true;
}
