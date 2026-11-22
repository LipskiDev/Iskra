#pragma once

#include <string>
#include <cstdint>
#include <glm/glm.hpp>

namespace Iskra {
struct Material {
  glm::vec4 baseColorFactor{1.0f};
  glm::vec3 emissiveFactor{0.0f};
  float metallicFactor = 0.0f;
  float roughnessFactor = 1.0f;
  float transmissionFactor = 0.0f;
  float indexOfRefraction = 1.0f;
  // Indices into AssetRegistry's texture storage; -1 means no texture.
  int32_t baseColorTextureIndex = 0;
  int32_t metallicRoughnessTextureIndex = 0;
  int32_t normalTextureIndex = 0;
  int32_t occlusionTextureIndex = 0;
  int32_t emissiveTextureIndex = 0;
  float padding = 0.0f;
};
}  // namespace Iskra
