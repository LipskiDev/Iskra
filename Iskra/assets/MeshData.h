#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace Iskra {
  struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
  };

  struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
  };
}