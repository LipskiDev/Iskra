#pragma once

#include <nvrhi/nvrhi.h>

namespace Iskra {
  struct GpuMesh {
    nvrhi::BufferHandle vertexBuffer;
    nvrhi::BufferHandle indexBuffer;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
  };

  struct GpuTexture {
    nvrhi::TextureHandle texture;
    nvrhi::SamplerHandle sampler;
  };

  struct GpuMaterial {
    nvrhi::BufferHandle materialDataBuffer;
    GpuTexture albedo;
    GpuTexture normal;
    GpuTexture roughness;
    GpuTexture metallic;
  };

  struct GpuModel {
    GpuMesh mesh;
    GpuMaterial material;
  };
}