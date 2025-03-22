cmake_minimum_required(VERSION "3.0")
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -pthread")
set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_COMPILER "g++")

set(EXECUTABLE_OUTPUT_PATH ./bin/)

project("main")
aux_source_directory(${CMAKE_SOURCE_DIR}/src/ SRC_LIST)
aux_source_directory(${CMAKE_SOURCE_DIR}/include/Model/ MODEL_SRC)

if(CMAKE_HOST_WIN32)    
    set(CMAKE_C_COMPILER_TARGET x86_64-w64-windows-gnu)
    set(CMAKE_CXX_COMPILER_TARGET x86_64-w64-windows-gnu)
endif(CMAKE_HOST_WIN32)

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

