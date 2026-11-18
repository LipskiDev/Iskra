#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace Iskra {
  struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
	glm::vec3 tangent;
	glm::vec3 bitangent;
  };

  struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
  };
}