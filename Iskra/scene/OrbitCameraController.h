#pragma once

#include <scene/Camera.h>
#include <core/Input.h>
#include <memory>

namespace Iskra {
  class OrbitCameraController {

  public:
    OrbitCameraController() = default;
    void OnUpdate(float deltaTime);
    void OnInput(float deltaTime, Input& input);
    void SetCamera(Camera& camera) { m_Camera = &camera; }

  private:
    Camera* m_Camera = nullptr;

    glm::vec3 m_Target{ 0.0f };
    float m_Distance = 5.0f;
    float m_Yaw = 0.0f;
    float m_Pitch = 0.0f;

    glm::vec2 m_LastMousePosition{ 0.0f };
    bool m_WasDragging = false;
  };
}