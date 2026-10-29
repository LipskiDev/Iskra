#pragma once

#include <nvrhi/nvrhi.h>

namespace Iskra {
  struct FrameContext;


  class TrianglePass {
  public:
    TrianglePass(nvrhi::IDevice* device, nvrhi::IFramebuffer* framebuffer);

    void Render(const FrameContext& frame);

  private:
    nvrhi::GraphicsPipelineHandle m_Pipeline;
  };
}