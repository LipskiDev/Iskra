#pragma once

#include <GLFW/glfw3.h>

#include <tuple>

namespace Iskra {
  class Input {
  public:
    Input(GLFWwindow* window);
    void Initialize(GLFWwindow* window);
    void Shutdown();
    void SetCapture(bool mouse, bool keyboard);
    bool IsKeyPressed(int key);
    bool IsMouseButtonPressed(int button);
    std::pair<float, float> GetMousePosition();

  private:
    GLFWwindow* m_Window;
    bool m_CaptureMouse = false;
    bool m_CaptureKeyboard = false;
  };
}
