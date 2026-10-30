#pragma once

#include <experiments/Experiment.h>
#include <scene/Camera.h>
#include <scene/OrbitCameraController.h>
#include <assets/ModelData.h>
#include <graphics/GpuMesh.h>
#include <graphics/MeshRenderer.h>
#include <graphics/MeshDrawItem.h>
#include <graphics/RayTracingScene.h>

namespace Iskra {

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
    void LoadScene();
    void CreateBindings();
    void CreatePipeline(const nvrhi::FramebufferInfo& outputInfo);
    void RenderGeometry(const FrameContext& frame);

    Camera m_Camera;
    OrbitCameraController m_CameraController;

    ModelData m_Model;
    std::vector<GpuMesh> m_GpuMeshes;
    std::vector<MeshDrawItem> m_DrawItems;

    nvrhi::GraphicsPipelineHandle m_Pipeline;
    nvrhi::BindingLayoutHandle m_BindingLayout;
    nvrhi::BindingSetHandle m_BindingSet;
    nvrhi::BufferHandle m_ConstantBuffer;

    struct ConstantBufferData {
      glm::mat4 model;
      glm::mat4 view;
      glm::mat4 projection;
      glm::mat4 normalMatrix;
    };

    MeshRenderer m_MeshRenderer;
    RayTracingScene m_RayTracingScene;
  };
}
