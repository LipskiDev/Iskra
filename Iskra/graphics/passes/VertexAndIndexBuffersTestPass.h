#pragma once

#include <nvrhi/nvrhi.h>

namespace Iskra {
  struct FrameContext;


  class VertexAndIndexBuffersTestPass {
  public:
    VertexAndIndexBuffersTestPass(nvrhi::IDevice* device, nvrhi::IFramebuffer* framebuffer);

    void Render(const FrameContext& frame);

  private:
    nvrhi::GraphicsPipelineHandle m_Pipeline;
    nvrhi::BufferHandle m_VertexBuffer;
    nvrhi::BufferHandle m_IndexBuffer;
  };
}