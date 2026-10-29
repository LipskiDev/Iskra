#pragma once

#include <iostream>

#include <core/window.h>
#include <core/GraphicsContext.h>
#include <core/Swapchain.h>
#include <graphics/Renderer.h>
#include <scene/Scene.h>

namespace Iskra {
  class Application {

    Window m_Window;
    GraphicsContext m_Graphics;
    Swapchain m_Swapchain;
    Renderer m_Renderer;
  public:
    Application() : m_Window("Iskra", 1920, 1080), m_Graphics(GraphicsConfig()), m_Swapchain(m_Graphics, m_Window), m_Renderer(m_Graphics.GetDevice()) {}

    void Run();
  };
}