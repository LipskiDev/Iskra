#pragma once

#include <graphics/GpuData.h>

namespace Iskra {
class MeshRenderer {
 public:
  void Draw(nvrhi::ICommandList *commandList, const GpuMesh &mesh,
            nvrhi::GraphicsState state) const;
};
}  // namespace Iskra
