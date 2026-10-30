#pragma once

#include <assets/MeshData.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>
#include <string>

namespace Iskra {
  struct MeshInstanceData {
    uint32_t meshIndex;
    int32_t materialIndex = -1;
  };

  struct ModelNode {
    std::string name;
    glm::mat4 localTransform{ 1.0f };

    std::vector<MeshInstanceData> meshInstances;
    std::vector<ModelNode> children;
  };

  struct ModelData {
    std::vector<MeshData> meshes;
    ModelNode root;
  };
}