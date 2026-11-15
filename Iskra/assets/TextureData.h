#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <nvrhi/nvrhi.h>

namespace Iskra {
  struct Texture {
    uint32_t width = 0;
    uint32_t height = 0;

    nvrhi::Format format = nvrhi::Format::RGBA8_UNORM;
    std::vector<uint8_t> data;

    nvrhi::TextureHandle gpuTexture;
    uint32_t gpuTextureIndex = 0;

    std::string name; // debugging
  };
}
