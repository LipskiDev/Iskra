#include "Renderer.h"

namespace Iskra {
  Renderer::Renderer(nvrhi::IDevice* device)
  {

  }

  void Renderer::Render(const FrameContext& frame, const Scene& scene)
  {
    frame.commandList->clearTextureFloat(
      frame.backbuffer,
      nvrhi::AllSubresources,
      nvrhi::Color(0.08f, 0.12f, 0.20f, 1.0f));
  }
}
