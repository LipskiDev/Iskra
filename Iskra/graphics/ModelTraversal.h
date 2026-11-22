#pragma once

#include <assets/ModelData.h>
#include <graphics/MeshDrawItem.h>
#include <graphics/render_instance.h>
#include <vector>

namespace Iskra {
void CollectDrawItems(const ModelNode &node, const glm::mat4 &parentTransform,
                      std::vector<MeshDrawItem> &items);
void CollectRenderInstances(const ModelNode &node,
                            const glm::mat4 &parentTransform, EntityId entity,
                            uint32_t visibilityMask,
                            std::vector<RenderInstance> &instances);
}  // namespace Iskra
