#include "Iskra.h"

#include <backends/imgui_impl_glfw.h>
#include <stdexcept>

using namespace std;

namespace Iskra {
  Application::~Application()
  {
    m_Graphics.GetDevice()->waitForIdle();
    m_ImGuiRenderer.Shutdown();
    if (m_ImGuiInitialized) {
      ImGui_ImplGlfw_Shutdown();
    }
    if (ImGui::GetCurrentContext()) {
      ImGui::DestroyContext();
    }
  }

  void Application::Run()
  {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    if (!ImGui_ImplGlfw_InitForOther(m_Window.GetHandle(), true)) {
      throw std::runtime_error("Failed to initialize ImGui GLFW backend");
    }
    m_ImGuiInitialized = true;
    m_ImGuiRenderer.Initialize(m_Graphics.GetDevice(), m_Swapchain.GetFramebufferInfo());

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

      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();
      m_Experiment->OnUiRender(*frame);

      const ImGuiIO& io = ImGui::GetIO();
      m_Input.SetCapture(io.WantCaptureMouse, io.WantCaptureKeyboard);
      m_Experiment->OnInput(deltaTime, m_Input);
      m_Experiment->OnUpdate(deltaTime);
      m_Experiment->OnRender(*frame);

      ImGui::Render();
      m_ImGuiRenderer.Render(frame->commandList, frame->framebuffer, ImGui::GetDrawData());

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
