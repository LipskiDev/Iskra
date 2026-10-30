#pragma once

#include <glm/glm.hpp>

namespace Iskra {
  class Camera {
  public:
    Camera() = default;
    Camera(float near, float far, float fov, float aspectRatio);

    void SetPosition(glm::vec3 position);
    void SetOrientation(glm::vec3 orientation);

    glm::vec3 GetPosition() const;
    glm::vec3 GetOrientation() const;

    glm::mat4 GetViewMatrix() const { return m_ViewMatrix; }
    glm::mat4 GetProjectionMatrix() const { return m_ProjectionMatrix; }
  private:
    void UpdateViewMatrix();

  private:
    glm::vec3 m_Position = {0.0f, 0.0f, 0.0f};
    glm::vec3 m_Orientation = {0.0f, 0.0f, 1.0f};
    
    glm::vec3 m_LockedAxis = {0.0f, 0.0f, 0.0f};
    glm::vec3 m_LockedAxisOrientation = {0.0f, 0.0f, 0.0f};

    glm::mat4 m_ViewMatrix;
    glm::mat4 m_ProjectionMatrix;

    float m_Near;
    float m_Far;
    float m_FOV;
    float m_AspectRatio;
  };
}