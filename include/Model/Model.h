#pragma once

#include <array>
#include <vector>
#include <Model/Mesh.h>
#include <string>




class Model {
public:
    std::vector<Mesh> meshes;
    std::string source;
    std::string directory;

};

// Model getExampleModel();
