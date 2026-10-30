#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace Iskra {
  struct MeshDrawItem {
    uint32_t meshIndex;
    int32_t materialIndex = -1;
    glm::mat4 worldTransform{1.0f};
  };
}
