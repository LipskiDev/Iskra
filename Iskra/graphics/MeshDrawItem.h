#pragma once

#include <assets/asset_handle.h>
#include <glm/glm.hpp>

namespace Iskra {
struct MeshDrawItem {
  MeshHandle mesh;
  MaterialHandle material;
  glm::mat4 worldTransform{1.0f};
};
}  // namespace Iskra
