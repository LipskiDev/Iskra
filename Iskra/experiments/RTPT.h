#pragma once

#include <experiments/Experiment.h>

namespace Iskra {
  class RTPTExperiment : public Experiment {
  public:
    void OnInit(nvrhi::IDevice* device, const nvrhi::FramebufferInfo& framebufferInfo) override;
    ~RTPTExperiment() = default;

    void OnUpdate(float deltaTime) override;
    void OnInput(float deltaTime, Input& input) override;
    void OnRender(FrameContext& frame) override;
    void OnUiRender(FrameContext& frame) override;
    void OnDestroy() override;
    void OnResize(int width, int height) override;

    std::string_view GetName() const override;

  private:
    void CreateOutput(uint32_t width, uint32_t height, nvrhi::Format format);

    struct UniformBuffer
    {
      int frame;
      int seed;
    };

    nvrhi::rt::AccelStructHandle m_BLAS;
    nvrhi::rt::AccelStructHandle m_TLAS;
    nvrhi::rt::PipelineHandle m_Pipeline;
    nvrhi::rt::ShaderTableHandle m_ShaderTable;
    nvrhi::TextureHandle m_Output;
    nvrhi::TextureHandle m_AccumulationTexture;
    nvrhi::BindingSetHandle m_RayTracingBindingSet;
    nvrhi::BufferHandle m_AabbsBuffer;
    nvrhi::BufferHandle m_SpheresBuffer;
    nvrhi::BufferHandle m_MaterialsBuffer;
    nvrhi::BufferHandle m_UniformBuffer;
    nvrhi::BindingLayoutHandle m_RayTracingBindingLayout;

    int m_Seed = 0;
    int m_Frame = 0;
  };
}
