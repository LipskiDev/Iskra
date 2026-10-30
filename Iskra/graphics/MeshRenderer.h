#pragma once

#include <graphics/GpuMesh.h>

namespace Iskra {
  class MeshRenderer {
  public:
    void Draw(nvrhi::ICommandList* commandList,
      const GpuMesh& mesh,
      nvrhi::GraphicsState state) const;
  };
}
