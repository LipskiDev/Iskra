#pragma once

#include <nvrhi/nvrhi.h>

namespace Iskra {
  struct GpuMesh {
    nvrhi::BufferHandle vertexBuffer;
    nvrhi::BufferHandle indexBuffer;
    uint32_t indexCount = 0;
  };
}