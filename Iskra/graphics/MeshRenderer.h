#pragma once

#include <nvrhi/nvrhi.h>

#include <core/Swapchain.h>
#include <assets/ModelData.h>
#include <graphics/GpuMesh.h>
#include <scene/Camera.h>

namespace Iskra {
  class MeshRenderer {
  public:
    void Initialize(nvrhi::IDevice* device, const nvrhi::FramebufferInfo& outputInfo);

    void Render(
      const FrameContext& frame,
      const ModelData& model,
      const std::vector<GpuMesh>& gpuMeshes,
      const Camera& camera,
      const glm::mat4& placement = glm::mat4(1.0f));

  private:
    nvrhi::IDevice* m_Device = nullptr;
    nvrhi::GraphicsPipelineHandle m_Pipeline;
    nvrhi::BindingLayoutHandle m_BindingLayout;
    nvrhi::BindingSetHandle m_BindingSet;

    struct ConstantBufferData {
      glm::mat4 model;
      glm::mat4 view;
      glm::mat4 projection;
    };

    nvrhi::BufferHandle m_ConstantBuffer;

    void DrawNode(
      const FrameContext& frame,
      const ModelNode& node,
      const glm::mat4& parentTransform,
      const std::vector<GpuMesh>& gpuMeshes,
      const Camera& camera);
  };

}
