#pragma once 

#include <nvrhi/nvrhi.h>
#include <core/Swapchain.h>
#include <scene/Scene.h>

#include <memory>

#include <graphics/passes/TrianglePass.h>

namespace Iskra {

  class Renderer {
  public:
    explicit Renderer(nvrhi::IDevice* device);

    void Render(const FrameContext& frame, const Scene& scene);

  private:
    nvrhi::GraphicsPipelineHandle trianglePipeline;

    nvrhi::IDevice* m_Device;
    std::unique_ptr<TrianglePass> m_TrianglePass;
  };
}