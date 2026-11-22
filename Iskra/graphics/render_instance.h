#pragma once

#include <assets/asset_handle.h>
#include <scene/components/Entity.h>

#include <glm/glm.hpp>

#include <cstdint>

namespace Iskra {
struct RenderInstance {
  EntityId entity;
  MeshHandle mesh;
  MaterialHandle material;
  glm::mat4 worldTransform{1.0f};
  uint32_t visibilityMask = 0xffffffff;
};
}  // namespace Iskra
