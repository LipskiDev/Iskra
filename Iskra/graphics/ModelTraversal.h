#pragma once

#include <assets/ModelData.h>
#include <graphics/MeshDrawItem.h>
#include <vector>

namespace Iskra {
void CollectDrawItems(const ModelNode &node, const glm::mat4 &parentTransform,
                      std::vector<MeshDrawItem> &items);
}
