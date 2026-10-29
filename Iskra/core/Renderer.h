#pragma once 

#include <nvrhi/nvrhi.h>
#include "Swapchain.h"
#include <scene/Scene.h>

namespace Iskra {

  class Renderer {
  public:
    explicit Renderer(nvrhi::IDevice* device);

    void Render(const FrameContext& frame, const Scene& scene);

  private:
    nvrhi::GraphicsPipelineHandle trianglePipeline;
  };
}