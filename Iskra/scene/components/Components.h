#pragma once

#include <assets/asset_handle.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Iskra {
struct TransformComponent {
  glm::vec3 translation{0.0f, 0.0f, 0.0f};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f, 1.0f, 1.0f};

  glm::mat4 localTransform{1.0f};
  glm::mat4 worldTransform{1.0f};
  bool dirty = true;
};

struct CameraComponent {
  float fov = glm::radians(60.0f);
  float nearPlane = 0.1f;
  float farPlane = 1000.0f;

  bool isPrimary = true;
};

struct MeshComponent {
  MeshHandle mesh;
  MaterialHandle material;
  uint32_t visibilityMask = 0xFFFFFFFF;
  bool castsShadows = true;
};

struct ModelComponent {
  ModelHandle model;
  uint32_t visibilityMask = 0xFFFFFFFF;
  bool castsShadows = true;
};
}  // namespace Iskra
