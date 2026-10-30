#pragma once

#include <assets/ModelData.h>
#include <graphics/MeshDrawItem.h>
#include <vector>

namespace Iskra {
  // Appends instances to items. Clear it first when replacing a model's list.
  void CollectDrawItems(const ModelNode& node,
    const glm::mat4& parentTransform,
    std::vector<MeshDrawItem>& items);
}
