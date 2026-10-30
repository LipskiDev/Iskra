#include <experiments/MeshViewingExperiment.h>

#include <assets/ModelLoader.h>
#include <graphics/MeshUploader.h>

namespace Iskra {
  void MeshViewingExperiment::OnInit(nvrhi::IDevice* device, const nvrhi::FramebufferInfo& framebufferInfo)
  {
    m_Device = device;
    m_Camera = Camera(0.1f, 100.0f, 45.0f, 16.0f / 9.0f);
    m_Camera.SetPosition(glm::vec3(0.0f, 0.0f, 5.0f));
    m_Camera.SetOrientation(glm::vec3(0.0, 0.0f, -1.0f));
    m_CameraController.SetCamera(m_Camera);

    m_Model = ModelLoader::Load("assets/models/Sponza.glb");

    auto upload = device->createCommandList();
    upload->open();

    for (const MeshData& mesh : m_Model.meshes) {
      m_GpuMeshes.push_back(UploadMesh(m_Device, upload.Get(), mesh));
    }

    upload->close();
    m_Device->executeCommandList(upload);

    m_MeshRenderer.Initialize(device, framebufferInfo);
  }

  void MeshViewingExperiment::OnUpdate(float deltaTime)
  {
    m_CameraController.OnUpdate(deltaTime);
  }

  void MeshViewingExperiment::OnInput(float deltaTime, Input& input)
  {
    m_CameraController.OnInput(deltaTime, input);
  }

  void MeshViewingExperiment::OnRender(FrameContext& frame)
  {
    frame.commandList->clearDepthStencilTexture(
      frame.depthBuffer,
      nvrhi::AllSubresources,
      true, 1.0f, false, 0);

    frame.commandList->clearTextureFloat(
      frame.backbuffer,
      nvrhi::AllSubresources,
      nvrhi::Color(0.15f, 0.12f, 0.20f, 1.0f));

    m_MeshRenderer.Render(frame, m_Model, m_GpuMeshes, m_Camera);
  }

  void MeshViewingExperiment::OnUiRender(FrameContext &frame)
  {
  }

  void MeshViewingExperiment::OnDestroy()
  {
  }

  void MeshViewingExperiment::OnResize(int width, int height)
  {
  }

  std::string_view MeshViewingExperiment::GetName() const
  {
    return std::string_view("Mesh Viewing Experiment");
  }



}
