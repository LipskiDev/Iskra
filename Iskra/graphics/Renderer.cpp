#include <graphics/Renderer.h>

#include <time.h>

namespace Iskra {
  Renderer::Renderer(nvrhi::IDevice* device) : m_Device(device)
  {
  }

  void Renderer::Render(const FrameContext& frame, const Scene& scene)
  {
    if (!m_TrianglePass) {
      m_TrianglePass = std::make_unique<TrianglePass>(m_Device, frame.framebuffer);
    }

    frame.commandList->clearTextureFloat(
      frame.backbuffer,
      nvrhi::AllSubresources,
      nvrhi::Color(0.15, 0.72f, 0.20f, 1.0f));
 
    m_TrianglePass->Render(frame);
  }

}
