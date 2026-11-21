#pragma once

#include <scene/Camera.h>
#include <core/Input.h>

namespace Iskra {
class FlyingCameraController {
 public:
  FlyingCameraController() = default;
  void OnUpdate(float deltaTime);
  void OnInput(float deltaTime, Input &input);
  void SetCamera(Camera &camera);

 private:
  Camera *m_Camera = nullptr;

  float m_Yaw = 0.0f;
  float m_Pitch = 0.0f;

  glm::vec2 m_LastMousePosition{0.0f};
  bool m_WasDragging = false;
};
}  // namespace Iskra
