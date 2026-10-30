#pragma once

#include <experiments/Experiment.h>
#include <scene/Camera.h>
#include <scene/OrbitCameraController.h>

namespace Iskra {
  struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
  };

  class MeshViewingExperiment : public Experiment {
  public:
    void OnInit(nvrhi::IDevice* device, const nvrhi::FramebufferInfo& framebufferInfo) override;
    ~MeshViewingExperiment() = default;

    void OnUpdate(float deltaTime) override;
    void OnInput(float deltaTime, Input& input) override;
    void OnRender(FrameContext &frame) override;
    void OnUiRender(FrameContext &frame) override;
    void OnDestroy() override;
    void OnResize(int width, int height) override;

    std::string_view GetName() const override;

  private:
    Camera m_Camera;
    OrbitCameraController m_CameraController;

    nvrhi::GraphicsPipelineHandle m_Pipeline;
    nvrhi::BufferHandle m_VertexBuffer;
    nvrhi::BufferHandle m_IndexBuffer;

    nvrhi::BufferHandle m_ConstantBuffer;

    nvrhi::BindingLayoutHandle m_BindingLayout;
    nvrhi::BindingSetHandle m_BindingSet;

    struct ConstantBufferData {
      glm::mat4 model;
      glm::mat4 view;
      glm::mat4 projection;
    };
  };
}