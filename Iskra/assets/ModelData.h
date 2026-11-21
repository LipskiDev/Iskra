#pragma once

#include <assets/asset_handle.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>
#include <string>

namespace Iskra {
struct MeshInstanceData {
  MeshHandle mesh;
  MaterialHandle material;
};

struct ModelNode {
  std::string name;
  glm::mat4 localTransform{1.0f};

  std::vector<MeshInstanceData> meshInstances;
  std::vector<ModelNode> children;
};

struct ModelData {
  ModelNode root;
};
}  // namespace Iskra
