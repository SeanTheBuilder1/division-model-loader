#pragma once

#include <string>
#include <fstream>
#include <limits>
#include <iostream>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Model/Model.h"

bool loadModel(const std::string& source, Model* result_model);
void processNode(
    Model* model, aiNode* node, const aiScene* scene,
    const std::string& directory
);
Mesh processMesh(
    aiMesh* mesh, const aiScene* scene, const std::string& directory
);
std::vector<Texture> loadMaterialTextures(
    aiMaterial* mat, aiTextureType type, std::string typeName,
    std::vector<Texture>& textures_loaded
);
void saveModel(const std::string& destination, Model* src_model);
bool compareSavedModel(Model* original_model, Model* test_model);
void loadModelTester(const std::string& source, Model* result_model);
