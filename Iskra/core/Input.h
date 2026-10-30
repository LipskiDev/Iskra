#pragma once

#include <GLFW/glfw3.h>

#include <tuple>

namespace Iskra {
  class Input {
  public:
    Input(GLFWwindow* window);
    void Initialize(GLFWwindow* window);
    void Shutdown();
    bool IsKeyPressed(int key);
    bool IsMouseButtonPressed(int button);
    std::pair<float, float> GetMousePosition();

  private:
    GLFWwindow* m_Window;
  };
}