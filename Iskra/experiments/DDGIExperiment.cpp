#include <experiments/DDGIExperiment.h>

#include <assets/ModelLoader.h>
#include <graphics/MeshUploader.h>
#include <graphics/ModelTraversal.h>
#include <imgui.h>

#include <cstddef>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

  std::vector<char> ReadBinary(const char* path)
  {
    std::ifstream file(path, std::ios::binary);

    if (!file) {
      throw std::runtime_error(
        std::string("Cannot open shader: ") + path);
    }

    std::vector<char> bytes{
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    };

    if (bytes.empty()) {
      throw std::runtime_error(
        std::string("Shader is empty: ") + path);
    }

    return bytes;
  }

}

namespace Iskra {
  void DDGIExperiment::OnInit(nvrhi::IDevice* device, const nvrhi::FramebufferInfo& framebufferInfo)
  {
    m_Device = device;
    m_Camera = Camera(0.1f, 100.0f, 45.0f, 16.0f / 9.0f);
    m_Camera.SetPosition(glm::vec3(0.0f, 0.0f, 5.0f));
    m_Camera.SetOrientation(glm::vec3(0.0, 0.0f, -1.0f));
    m_CameraController.SetCamera(m_Camera);

    LoadScene();
    CreateBindings();
    CreatePipeline(framebufferInfo);
  }

  void DDGIExperiment::LoadScene()
  {
    m_Model = ModelLoader::Load("assets/models/sponza/Sponza.gltf");

    m_DrawItems.clear();
    CollectDrawItems(m_Model.root, glm::mat4(1.0f), m_DrawItems);

    auto upload = m_Device->createCommandList();
    upload->open();

    for (const MeshData& mesh : m_Model.meshes) {
      m_GpuMeshes.push_back(UploadMesh(m_Device, upload.Get(), mesh));
    }

    upload->close();
    m_Device->executeCommandList(upload);

    if (!m_Device->waitForIdle()) {
      throw std::runtime_error("Failed to wait for GPU");
    }

    auto build = m_Device->createCommandList();
    build->open();

    m_RayTracingScene.Build(m_Device, build.Get(), m_Model, m_GpuMeshes);
    build->close();
    m_Device->executeCommandList(build);

    if (!m_Device->waitForIdle()) {
      throw std::runtime_error("Failed to wait for GPU");
    }
  }

  void DDGIExperiment::CreateBindings()
  {
    nvrhi::BufferDesc constantBufferDesc{};
    constantBufferDesc.byteSize = sizeof(ConstantBufferData);
    constantBufferDesc.debugName = "Constant Buffer";
    constantBufferDesc.isConstantBuffer = true;
    constantBufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
    constantBufferDesc.keepInitialState = true;
    constantBufferDesc.isVolatile = true;

    m_ConstantBuffer = m_Device->createBuffer(constantBufferDesc);

    if (!m_ConstantBuffer) {
      throw std::runtime_error("Failed to create mesh constant buffer");
    }

    nvrhi::BindingLayoutDesc bindingLayoutDesc{};
    bindingLayoutDesc.visibility = nvrhi::ShaderType::AllGraphics;
    bindingLayoutDesc.bindings = {
       nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
        nvrhi::BindingLayoutItem::RayTracingAccelStruct(0)
    };

    m_BindingLayout = m_Device->createBindingLayout(bindingLayoutDesc);

    if (!m_BindingLayout) {
      throw std::runtime_error("Failed to create camera binding layout");
    }

    nvrhi::BindingSetDesc bindingSetDesc{};
    bindingSetDesc.bindings = {
      { nvrhi::BindingSetItem::ConstantBuffer(0, m_ConstantBuffer) },
      { nvrhi::BindingSetItem::RayTracingAccelStruct(0, m_RayTracingScene.GetTLAS()) }
    };

    m_BindingSet = m_Device->createBindingSet(bindingSetDesc, m_BindingLayout);

    if (!m_BindingSet) {
      throw std::runtime_error("Failed to create camera binding set");
    }
  }

  void DDGIExperiment::CreatePipeline(const nvrhi::FramebufferInfo& outputInfo)
  {
    const auto vsBytes = ReadBinary(ISKRA_SHADER_DIR "/DDGI.vs.dxil");
    const auto psBytes = ReadBinary(ISKRA_SHADER_DIR "/DDGI.ps.dxil");

    nvrhi::ShaderDesc vsDesc{};
    vsDesc.shaderType = nvrhi::ShaderType::Vertex;
    vsDesc.debugName = "DDGI.vs";
    vsDesc.entryName = "VSMain";

    nvrhi::ShaderDesc psDesc{};
    psDesc.shaderType = nvrhi::ShaderType::Pixel;
    psDesc.debugName = "DDGI.ps";
    psDesc.entryName = "PSMain";

    auto vertexShader = m_Device->createShader(vsDesc, vsBytes.data(), vsBytes.size());
    auto pixelShader = m_Device->createShader(psDesc, psBytes.data(), psBytes.size());

    if (!vertexShader || !pixelShader) {
      throw std::runtime_error("Failed to create mesh shaders");
    }

    nvrhi::GraphicsPipelineDesc pipelineDesc{};
    pipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;
    pipelineDesc.VS = vertexShader;
    pipelineDesc.PS = pixelShader;

    pipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;

    pipelineDesc.renderState.depthStencilState.depthTestEnable = true;
    pipelineDesc.renderState.depthStencilState.depthWriteEnable = true;
    pipelineDesc.renderState.depthStencilState.stencilEnable = false;

    auto& depth = pipelineDesc.renderState.depthStencilState;
    depth.depthTestEnable = true;
    depth.depthWriteEnable = true;
    depth.depthFunc = nvrhi::ComparisonFunc::Less;
    depth.stencilEnable = false;

    nvrhi::VertexAttributeDesc vertexAttributes[2]{};
    vertexAttributes[0].name = "POSITION";
    vertexAttributes[1].name = "COLOR";

    vertexAttributes[0].format = nvrhi::Format::RGB32_FLOAT;
    vertexAttributes[1].format = nvrhi::Format::RGB32_FLOAT;

    vertexAttributes[0].offset = 0;
    vertexAttributes[1].offset = offsetof(Vertex, normal);

    vertexAttributes[0].elementStride = sizeof(Vertex);
    vertexAttributes[1].elementStride = sizeof(Vertex);

    vertexAttributes[0].bufferIndex = 0;
    vertexAttributes[1].bufferIndex = 0;

    auto inputLayout = m_Device->createInputLayout(vertexAttributes, 2, vertexShader);

    pipelineDesc.addBindingLayout(m_BindingLayout);

    pipelineDesc.inputLayout = inputLayout;

    m_Pipeline = m_Device->createGraphicsPipeline(pipelineDesc, outputInfo);

    if (!m_Pipeline) {
      throw std::runtime_error("Failed to create mesh pipeline");
    }
  }

  void DDGIExperiment::RenderGeometry(const FrameContext& frame)
  {
    for (const MeshDrawItem& item : m_DrawItems) {
      ConstantBufferData constants{};
      constants.model = item.worldTransform;
      constants.view = m_Camera.GetViewMatrix();
      constants.projection = m_Camera.GetProjectionMatrix();
      constants.normalMatrix = glm::transpose(glm::inverse(item.worldTransform));
      constants.toLight = m_ToLight;

      frame.commandList->writeBuffer(
        m_ConstantBuffer, &constants, sizeof(constants));

      nvrhi::GraphicsState state{};
      state.pipeline = m_Pipeline.Get();
      state.framebuffer = frame.framebuffer;
      state.addBindingSet(m_BindingSet);
      state.viewport.addViewportAndScissorRect(
        frame.framebuffer->getFramebufferInfo().getViewport());

      m_MeshRenderer.Draw(frame.commandList,
        m_GpuMeshes.at(item.meshIndex), state);
    }
  }

  void DDGIExperiment::OnUpdate(float deltaTime)
  {
    m_CameraController.OnUpdate(deltaTime);
    m_ToLight = glm::vec3(
      cos(m_SunElevation) * sin(m_SunAzimuth),
      sin(m_SunElevation),
      cos(m_SunElevation) * cos(m_SunAzimuth));
  }

  void DDGIExperiment::OnInput(float deltaTime, Input& input)
  {
    m_CameraController.OnInput(deltaTime, input);
  }

  void DDGIExperiment::OnRender(FrameContext& frame)
  {
    frame.commandList->clearDepthStencilTexture(
      frame.depthBuffer,
      nvrhi::AllSubresources,
      true, 1.0f, false, 0);

    frame.commandList->clearTextureFloat(
      frame.backbuffer,
      nvrhi::AllSubresources,
      nvrhi::Color(0.15f, 0.12f, 0.20f, 1.0f));

    RenderGeometry(frame);
  }

  void DDGIExperiment::OnUiRender(FrameContext& frame)
  {
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340.0f, 180.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Lighting");
    ImGui::SliderAngle("Sun azimuth", &m_SunAzimuth, -180.0f, 180.0f);
    ImGui::SliderAngle("Sun elevation", &m_SunElevation, 1.0f, 89.0f);
    ImGui::Text("Direction: %.2f, %.2f, %.2f", m_ToLight.x, m_ToLight.y, m_ToLight.z);
    ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
    ImGui::End();
  }

  void DDGIExperiment::OnDestroy()
  {
  }

  void DDGIExperiment::OnResize(int width, int height)
  {
  }

  std::string_view DDGIExperiment::GetName() const
  {
    return std::string_view("DDGI Experiment");
  }

}
