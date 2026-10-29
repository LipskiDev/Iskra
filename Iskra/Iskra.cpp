#include "Iskra.h"

using namespace std;

namespace Iskra {
  void Application::Run()
  {
    while (!m_Window.ShouldClose()) {
      m_Window.PollEvents();

      m_Swapchain.Resize(m_Window.GetFramebufferSize());

      auto frame = m_Swapchain.BeginFrame();


      if (!frame) {
        glfwWaitEvents();
        continue;
      }

      
      m_Renderer.Render(*frame, {});

      m_Swapchain.Present(*frame);
    }
  }
}

int main()
{
  Iskra::Application app;
  app.Run();
  return 0;
}
