cmake_minimum_required(VERSION "3.0")
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "-O3 -std=c++20")
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -pthread")
set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_COMPILER "g++")

set(EXECUTABLE_OUTPUT_PATH ./bin/)

project("main")
aux_source_directory(${CMAKE_SOURCE_DIR}/src/ SRC_LIST)
aux_source_directory(${CMAKE_SOURCE_DIR}/include/Model/ MODEL_SRC)

add_executable(
    main
    ${CMAKE_SOURCE_DIR}/src/main.cpp
    ${SRC_LIST}
    ${MODEL_SRC}
)

add_subdirectory(${CMAKE_SOURCE_DIR}/include/ASSIMP)

target_include_directories(
    main PUBLIC ${CMAKE_SOURCE_DIR}/include/ ${CMAKE_SOURCE_DIR}/include/ASSIMP/include/
)

target_link_libraries(
    main
    assimp
)

