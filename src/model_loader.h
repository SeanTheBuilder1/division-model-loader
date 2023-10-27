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
void processNode(Model* model, aiNode *node, const aiScene *scene);
Mesh processMesh(aiMesh *mesh, const aiScene *scene);
std::vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName, std::vector<Texture>& textures_loaded);
void saveModel(const std::string& destination, Model* src_model);
