#include "lve_collision.hpp"

// libs
#include <glm/gtx/intersect.hpp>
#include <tiny_obj_loader.h>

// std
#include <stdexcept>
#include <iostream>

#ifndef ENGINE_DIR
#define ENGINE_DIR "../"
#endif

namespace lve{
  std::vector<Triangle> LveCollision::createCollisionFromFile(const std::string &filepath){
    std::vector<Triangle> triangles;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    std::string fullPath = ENGINE_DIR + filepath;
    std::string baseDir = fullPath.substr(0, fullPath.find_last_of('/') + 1);

    if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, fullPath.c_str(), baseDir.c_str(), true)){
      throw std::runtime_error("TinyObjLoader failed: " + warn + err);
    }

    for(const auto &shape : shapes){
      const auto &mesh = shape.mesh;
      for (size_t i = 0; i < mesh.indices.size(); i += 3){
        Triangle tri;

        for (int j = 0; j < 3; ++j){
          int idx = mesh.indices[i + j].vertex_index;
          glm::vec3 &vertex = (j == 0) ? tri.v0 : (j == 1) ? tri.v1
                                                           : tri.v2;

          vertex = {
              attrib.vertices[3 * idx + 0],
              attrib.vertices[3 * idx + 1],
              attrib.vertices[3 * idx + 2]};
        }

        triangles.push_back(tri);
      }
    }

    std::cout << "Loaded " << triangles.size() << " triangles from " << filepath << std::endl;
    return triangles;
  }
}