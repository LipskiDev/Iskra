#pragma once

#include <iostream>

#include <core/window.h>
#include <core/GraphicsContext.h>
#include <core/Swapchain.h>
#include <core/Input.h>
#include <scene/Scene.h>
#include <experiments/Experiment.h>
#include <experiments/MeshViewingExperiment.h>

namespace Iskra {
  class Application {

    Window m_Window;
    Input m_Input;
    GraphicsContext m_Graphics;
    Swapchain m_Swapchain;
    std::unique_ptr<Experiment> m_Experiment = nullptr;
  public:
    Application() : m_Window("Iskra", 1920, 1080), m_Input(m_Window.GetHandle()), m_Graphics(GraphicsConfig()), m_Swapchain(m_Graphics, m_Window)
    {
      m_Experiment = std::make_unique<MeshViewingExperiment>();
      m_Experiment->OnInit(m_Graphics.GetDevice(), m_Swapchain.GetFramebufferInfo());
    }

    void Run();
  };
}