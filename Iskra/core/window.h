#pragma once

#include <string>

#include <GLFW/glfw3.h>

namespace Iskra {

  struct Extent2D
  {
    unsigned int width;
    unsigned int height;
  };

  class Window
  {
  public:
    Window(const std::string_view& title, unsigned int width, unsigned int height);
    ~Window();

    void PollEvents();
    bool ShouldClose() const;
    Extent2D GetFramebufferSize();

    GLFWwindow* GetHandle() const;

  private:
    GLFWwindow* m_Window = nullptr;
    std::string m_title;
    unsigned int m_width;
    unsigned int m_height;
    void* m_windowHandle;
  };

}