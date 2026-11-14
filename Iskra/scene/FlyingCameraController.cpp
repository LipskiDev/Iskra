#include <scene/FlyingCameraController.h>

#include <cmath>

namespace Iskra {
  void FlyingCameraController::SetCamera(Camera& camera)
  {
    m_Camera = &camera;
    m_WasDragging = false;
    const glm::vec3 direction = camera.GetOrientation();
    const float lengthSquared = glm::dot(direction, direction);
    if (lengthSquared > 0.0f) {
      const glm::vec3 forward = direction / std::sqrt(lengthSquared);
      m_Yaw = std::atan2(forward.x, -forward.z);
      m_Pitch = std::asin(glm::clamp(forward.y, -1.0f, 1.0f));
    }
    else {
      m_Yaw = 0.0f;
      m_Pitch = 0.0f;
    }
  }

  void FlyingCameraController::OnUpdate(float deltaTime)
  {
  }

  void FlyingCameraController::OnInput(float deltaTime, Input& input)
  {
    if (!m_Camera) {
      return;
    }

    const auto [mouseX, mouseY] = input.GetMousePosition();
    const glm::vec2 mousePosition(mouseX, mouseY);
    const bool dragging = input.IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
    const bool looking = dragging && m_WasDragging;

    if (looking) {
      const glm::vec2 delta = mousePosition - m_LastMousePosition;
      constexpr float sensitivity = 0.005f;
      m_Yaw += delta.x * sensitivity;
      m_Pitch -= delta.y * sensitivity;
      m_Yaw = std::remainder(m_Yaw, 6.28318530718f);
      m_Pitch = glm::clamp(
        m_Pitch, -glm::radians(89.0f), glm::radians(89.0f));
    }

    m_LastMousePosition = mousePosition;
    m_WasDragging = dragging;

    const glm::vec3 forward(
      std::sin(m_Yaw) * std::cos(m_Pitch),
      std::sin(m_Pitch),
      -std::cos(m_Yaw) * std::cos(m_Pitch));
    const glm::vec3 right(std::cos(m_Yaw), 0.0f, std::sin(m_Yaw));
    const glm::vec3 up(0.0f, 1.0f, 0.0f);

    glm::vec3 movement(0.0f);
    if (input.IsKeyPressed(GLFW_KEY_W)) movement += forward;
    if (input.IsKeyPressed(GLFW_KEY_S)) movement -= forward;
    if (input.IsKeyPressed(GLFW_KEY_D)) movement += right;
    if (input.IsKeyPressed(GLFW_KEY_A)) movement -= right;
    if (input.IsKeyPressed(GLFW_KEY_E)) movement += up;
    if (input.IsKeyPressed(GLFW_KEY_Q)) movement -= up;

    const float lengthSquared = glm::dot(movement, movement);
    if (lengthSquared > 0.0f && deltaTime > 0.0f) {
      constexpr float movementSpeed = 3.0f;
      const bool fast = input.IsKeyPressed(GLFW_KEY_LEFT_SHIFT)
        || input.IsKeyPressed(GLFW_KEY_RIGHT_SHIFT);
      const float speed = movementSpeed * (fast ? 4.0f : 1.0f);
      m_Camera->SetPosition(m_Camera->GetPosition()
        + movement / std::sqrt(lengthSquared) * speed * deltaTime);
    }

    if (looking) {
      m_Camera->SetOrientation(forward);
    }
  }
}
