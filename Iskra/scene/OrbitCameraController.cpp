#include <scene/OrbitCameraController.h>

#include <iostream>

namespace Iskra {
  void OrbitCameraController::OnUpdate(float deltaTime)
  {
  }

  void OrbitCameraController::OnInput(float deltaTime, Input& input)
  {
    if (!m_Camera) {
      return;
    }

    const auto [mouseX, mouseY] = input.GetMousePosition();
    const glm::vec2 mousePosition(mouseX, mouseY);

    const bool dragging =
      input.IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);

    if (dragging && m_WasDragging) {
      const glm::vec2 delta = mousePosition - m_LastMousePosition;
      constexpr float sensitivity = 0.005f; // Radians per pixel.

      m_Yaw -= delta.x * sensitivity;
      m_Pitch += delta.y * sensitivity;

      // Avoid looking parallel to the camera's world-up vector.
      m_Pitch = glm::clamp(
        m_Pitch, -glm::radians(89.0f), glm::radians(89.0f));
    }

    m_LastMousePosition = mousePosition;
    m_WasDragging = dragging;

    constexpr float zoomSpeed = 3.0f;

    if (input.IsKeyPressed(GLFW_KEY_W)) {
      m_Distance -= zoomSpeed * deltaTime;
    }

    if (input.IsKeyPressed(GLFW_KEY_S)) {
      m_Distance += zoomSpeed * deltaTime;
    }

    m_Distance = glm::clamp(m_Distance, 0.5f, 50.0f);

    const glm::vec3 offset(
      std::sin(m_Yaw) * std::cos(m_Pitch),
      std::sin(m_Pitch),
      std::cos(m_Yaw) * std::cos(m_Pitch));

    const glm::vec3 position = m_Target + offset * m_Distance;

    m_Camera->SetPosition(position);
    m_Camera->SetOrientation(
      glm::normalize(m_Target - position));
  }

}