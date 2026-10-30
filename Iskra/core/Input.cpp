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
    return glfwGetKey(m_Window, key) == GLFW_PRESS;
  }

  bool Input::IsMouseButtonPressed(int button)
  {
    return glfwGetMouseButton(m_Window, button) == GLFW_PRESS;
  }

  std::pair<float, float> Input::GetMousePosition()
  {
    double xpos, ypos;
    glfwGetCursorPos(m_Window, &xpos, &ypos);
    return std::pair<float, float>(static_cast<float>(xpos), static_cast<float>(ypos));
  }

}