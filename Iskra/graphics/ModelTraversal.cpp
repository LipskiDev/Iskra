#include <graphics/ModelTraversal.h>

namespace Iskra {
void CollectDrawItems(const ModelNode &node, const glm::mat4 &parentTransform,
                      std::vector<MeshDrawItem> &items) {
  const glm::mat4 world = parentTransform * node.localTransform;

  for (const MeshInstanceData &instance : node.meshInstances) {
    items.push_back({instance.mesh, instance.material, world});
  }

  for (const ModelNode &child : node.children) {
    CollectDrawItems(child, world, items);
  }
}

void CollectRenderInstances(const ModelNode &node,
                            const glm::mat4 &parentTransform, EntityId entity,
                            uint32_t visibilityMask,
                            std::vector<RenderInstance> &instances) {
  const glm::mat4 world = parentTransform * node.localTransform;

  for (const MeshInstanceData &instance : node.meshInstances) {
    instances.push_back({.entity = entity,
                         .mesh = instance.mesh,
                         .material = instance.material,
                         .worldTransform = world,
                         .visibilityMask = visibilityMask});
  }

  for (const ModelNode &child : node.children) {
    CollectRenderInstances(child, world, entity, visibilityMask, instances);
  }
}
}  // namespace Iskra
