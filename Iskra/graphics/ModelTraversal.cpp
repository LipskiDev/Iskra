#include <graphics/ModelTraversal.h>

namespace Iskra {
  void CollectDrawItems(const ModelNode& node,
    const glm::mat4& parentTransform,
    std::vector<MeshDrawItem>& items)
  {
    const glm::mat4 world = parentTransform * node.localTransform;

    for (const MeshInstanceData& instance : node.meshInstances) {
      items.push_back({ instance.meshIndex, instance.materialIndex, world });
    }

    for (const ModelNode& child : node.children) {
      CollectDrawItems(child, world, items);
    }
  }
}
