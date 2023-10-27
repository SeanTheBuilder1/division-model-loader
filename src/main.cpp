#include "model_loader.h"

#include <iostream>
#include <cstdio>

int main(int argc, char** argv){
    if(argc < 2){
        std::cerr << "ERROR: Usage main filename.model filename.dat";
        return 1;
    }
    Model* model_ptr = new Model();
    if(!loadModel(argv[1], model_ptr))
        return 1;
    if(argc < 3){
        saveModel(argv[2], model_ptr);
        printf("Saved %s at %s", argv[1], argv[2]);
    }
    else{
        std::string destination(argv[1]);
        size_t i = destination.find_last_of('.');
        if(i != std::string::npos){
            std::cout << destination;
            size_t diff = destination.size() - i;
            destination.resize(destination.size() - diff);
        }
        saveModel(destination + ".dat", model_ptr);
        printf("Saved %s at %s", argv[1], destination.c_str());
    }
    return 0;
}