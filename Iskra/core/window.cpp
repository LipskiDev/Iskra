#include "window.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdexcept>

namespace Iskra {


  Window::Window(const std::string_view& title, unsigned int width, unsigned int height)
    : m_title(title), m_width(width), m_height(height), m_windowHandle(nullptr)
  {
    if (!glfwInit())
    {
      throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);

    if (!window) {
      throw std::runtime_error("Failed to create window");
    }

    m_Window = window;
  }

  Window::~Window()
  {
    if (m_Window) {
      glfwDestroyWindow(m_Window);
    }
    glfwTerminate();
  }

  void Window::PollEvents()
  {
    glfwPollEvents();
  }

  bool Window::ShouldClose() const
  {
    return glfwWindowShouldClose(m_Window);
  }

  Extent2D Window::GetFramebufferSize()
  {
    int width, height;
    glfwGetFramebufferSize(m_Window, &width, &height);
    return Extent2D(static_cast<unsigned int>(width), static_cast<unsigned int>(height));
  }

  GLFWwindow* Window::GetHandle() const
  {
    return m_Window;
  }

}