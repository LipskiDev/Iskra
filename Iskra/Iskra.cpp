#include "Iskra.h"

using namespace std;

namespace Iskra {
  void Application::Run()
  {
    double previousTime = glfwGetTime();

    while (!m_Window.ShouldClose()) {
      const double currentTime = glfwGetTime();
      const float deltaTime = static_cast<float>(currentTime - previousTime);
      previousTime = currentTime;

      m_Window.PollEvents();

      m_Swapchain.Resize(m_Window.GetFramebufferSize());

      auto frame = m_Swapchain.BeginFrame();

      if (!frame) {
        glfwWaitEvents();
        continue;
      }

      m_Experiment->OnUpdate(deltaTime);
      m_Experiment->OnRender(*frame);
      m_Experiment->OnInput(deltaTime, m_Input);
      m_Experiment->OnUiRender(*frame);

      m_Swapchain.Present(*frame);
      float time = glfwGetTime();
    }
  }
}

int main()
{
  Iskra::Application app;
  app.Run();
  return 0;
}
