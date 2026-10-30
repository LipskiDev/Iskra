#pragma once

#include <experiments/Experiment.h>
#include <scene/Camera.h>
#include <scene/OrbitCameraController.h>
#include <assets/ModelData.h>
#include <graphics/GpuMesh.h>
#include <graphics/MeshRenderer.h>

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
    Camera m_Camera;
    OrbitCameraController m_CameraController;

    ModelData m_Model;
    std::vector<GpuMesh> m_GpuMeshes;

    MeshRenderer m_MeshRenderer;
  };
}
