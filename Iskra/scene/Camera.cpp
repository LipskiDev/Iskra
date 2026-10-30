#include "Camera.h"

#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

namespace Iskra {
  Camera::Camera(float near, float far, float fov, float aspectRatio)
    : m_Near(near), m_Far(far), m_FOV(fov), m_AspectRatio(aspectRatio)
  {
    UpdateViewMatrix();

    m_ProjectionMatrix = glm::perspectiveRH_ZO(glm::radians(m_FOV), m_AspectRatio, m_Near, m_Far);
  }

  void Camera::SetPosition(glm::vec3 position)
  {
    m_Position = position;
    UpdateViewMatrix();
  }

  void Camera::SetOrientation(glm::vec3 orientation)
  {
    m_Orientation = orientation;
    UpdateViewMatrix();
  }

  glm::vec3 Camera::GetPosition() const
  {
    return m_Position;
  }

  glm::vec3 Camera::GetOrientation() const
  {
    return m_Orientation;
  }

  void Camera::UpdateViewMatrix()
  {
    m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_Orientation, glm::vec3(0.0f, 1.0f, 0.0f));
  }

}