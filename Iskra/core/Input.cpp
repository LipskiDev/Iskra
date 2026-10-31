#include <core/Input.h>

namespace Iskra {
  Input::Input(GLFWwindow* window)
    : m_Window(window)
  {
  }

  void Input::Initialize(GLFWwindow* window)
  {
    m_Window = window;
  }

  void Input::Shutdown()
  {
  }

  bool Input::IsKeyPressed(int key)
  {
    return !m_CaptureKeyboard && glfwGetKey(m_Window, key) == GLFW_PRESS;
  }

  bool Input::IsMouseButtonPressed(int button)
  {
    return !m_CaptureMouse && glfwGetMouseButton(m_Window, button) == GLFW_PRESS;
  }

  void Input::SetCapture(bool mouse, bool keyboard)
  {
    m_CaptureMouse = mouse;
    m_CaptureKeyboard = keyboard;
  }

  std::pair<float, float> Input::GetMousePosition()
  {
    double xpos, ypos;
    glfwGetCursorPos(m_Window, &xpos, &ypos);
    return std::pair<float, float>(static_cast<float>(xpos), static_cast<float>(ypos));
  }

}
