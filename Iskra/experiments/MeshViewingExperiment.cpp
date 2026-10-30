#include <experiments/MeshViewingExperiment.h>


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
  void MeshViewingExperiment::OnInit(nvrhi::IDevice* device, const nvrhi::FramebufferInfo& framebufferInfo)
  {
    m_Device = device;
    m_Camera = Camera(0.1f, 100.0f, 45.0f, 16.0f / 9.0f);
    m_Camera.SetPosition(glm::vec3(0.0f, 0.0f, 5.0f));
    m_Camera.SetOrientation(glm::vec3(0.0, 0.0f, -1.0f));
    m_CameraController.SetCamera(m_Camera);

    nvrhi::BufferDesc constantBufferDesc{};
    constantBufferDesc.byteSize = sizeof(ConstantBufferData);
    constantBufferDesc.debugName = "Constant Buffer";
    constantBufferDesc.isConstantBuffer = true;
    constantBufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
    constantBufferDesc.keepInitialState = true;

    m_ConstantBuffer = device->createBuffer(constantBufferDesc);

    const auto vsBytes = ReadBinary("Iskra/shaders/Cube.vs.dxil");
    const auto psBytes = ReadBinary("Iskra/shaders/Cube.ps.dxil");

    nvrhi::ShaderDesc vsDesc{};
    vsDesc.shaderType = nvrhi::ShaderType::Vertex;
    vsDesc.debugName = "Cube.vs";
    vsDesc.entryName = "VSMain";

    nvrhi::ShaderDesc psDesc{};
    psDesc.shaderType = nvrhi::ShaderType::Pixel;
    psDesc.debugName = "Cube.ps";
    psDesc.entryName = "PSMain";

    auto vertexShader = device->createShader(vsDesc, vsBytes.data(), vsBytes.size());
    auto pixelShader = device->createShader(psDesc, psBytes.data(), psBytes.size());

    if (!vertexShader || !pixelShader) {
      throw std::runtime_error("Failed to create cube shaders");
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
    vertexAttributes[1].offset = offsetof(Vertex, color);

    vertexAttributes[0].elementStride = sizeof(Vertex);
    vertexAttributes[1].elementStride = sizeof(Vertex);

    vertexAttributes[0].bufferIndex = 0;
    vertexAttributes[1].bufferIndex = 0;

    auto inputLayout = device->createInputLayout(vertexAttributes, 2, vertexShader);

    nvrhi::BindingLayoutDesc bindingLayoutDesc{};
    bindingLayoutDesc.visibility = nvrhi::ShaderType::Vertex;
    bindingLayoutDesc.bindings = {
      { nvrhi::BindingLayoutItem::ConstantBuffer(0) }
    };

    m_BindingLayout = device->createBindingLayout(bindingLayoutDesc);

    if (!m_BindingLayout) {
      throw std::runtime_error("Failed to create camera binding layout");
    }

    nvrhi::BindingSetDesc bindingSetDesc{};
    bindingSetDesc.bindings = {
      { nvrhi::BindingSetItem::ConstantBuffer(0, m_ConstantBuffer) }
    };

    m_BindingSet = device->createBindingSet(bindingSetDesc, m_BindingLayout);

    if (!m_BindingSet) {
      throw std::runtime_error("Failed to create camera binding set");
    }

    pipelineDesc.addBindingLayout(m_BindingLayout);

    pipelineDesc.inputLayout = inputLayout;

    m_Pipeline = m_Device->createGraphicsPipeline(pipelineDesc, framebufferInfo);

    if (!m_Pipeline) {
      throw std::runtime_error("Failed to create cube pipeline");
    }

    Vertex vertices[] = {
       { { -1.0f, -1.0f, -1.0f },{ 1.0f, 0.0f, 0.0f } },
       { { 1.0f, -1.0f, -1.0f },{ 0.0f, 1.0f, 0.0f } },
       { { 1.0f,  1.0f, -1.0f },{ 0.0f, 0.0f, 1.0f } },
       { { -1.0f,  1.0f, -1.0f },{ 1.0f, 1.0f, 0.0f } },
       { { -1.0f, -1.0f,  1.0f },{ 1.0f, 0.5f, 0.5f } },
       { { 1.0f, -1.0f,  1.0f },{ 0.5f, 1.0f, 0.5f } },
       { { 1.0f,  1.0f,  1.0f },{ 0.5f, 0.5f, 1.0f } },
       { { -1.0f,  1.0f,  1.0f },{ 1.0f, 1.0f, 1.0f } }
    };

    unsigned int indices[] = {
      0, 1, 2, 2, 3, 0,
      4, 5, 6, 6, 7, 4,
      0, 1, 5, 5, 4, 0,
      2, 3, 7, 7, 6, 2,
      0, 3, 7, 7, 4, 0,
      1, 2, 6, 6, 5, 1
    };

    nvrhi::BufferDesc vertexBufferDesc{};
    vertexBufferDesc.byteSize = sizeof(vertices);
    vertexBufferDesc.debugName = "Vertex Buffer";
    vertexBufferDesc.isVertexBuffer = true;
    vertexBufferDesc.initialState = nvrhi::ResourceStates::VertexBuffer;
    vertexBufferDesc.keepInitialState = true;

    nvrhi::BufferDesc indexBufferDesc{};
    indexBufferDesc.byteSize = sizeof(indices);
    indexBufferDesc.debugName = "Index Buffer";
    indexBufferDesc.isIndexBuffer = true;
    indexBufferDesc.initialState = nvrhi::ResourceStates::IndexBuffer;
    indexBufferDesc.keepInitialState = true;


    m_VertexBuffer = device->createBuffer(vertexBufferDesc);
    m_IndexBuffer = device->createBuffer(indexBufferDesc);

    auto upload = device->createCommandList();
    upload->open();

    upload->writeBuffer(m_VertexBuffer, vertices, sizeof(vertices));
    upload->writeBuffer(m_IndexBuffer, indices, sizeof(indices));

    upload->close();
    device->executeCommandList(upload);
  }

  void MeshViewingExperiment::OnUpdate(float deltaTime)
  {
    m_CameraController.OnUpdate(deltaTime);
  }

  void MeshViewingExperiment::OnInput(float deltaTime, Input& input)
  {
    m_CameraController.OnInput(deltaTime, input);
  }

  void MeshViewingExperiment::OnRender(FrameContext &frame)
  {
    frame.commandList->clearDepthStencilTexture(
      frame.depthBuffer,
      nvrhi::AllSubresources,
      true,
      1.0f,
      false,
      0);

    frame.commandList->clearTextureFloat(
      frame.backbuffer,
      nvrhi::AllSubresources,
      nvrhi::Color(0.15, 0.12f, 0.20f, 1.0f));

    ConstantBufferData cbData{};
    cbData.model = glm::mat4(1.0f);
    cbData.view = m_Camera.GetViewMatrix();
    cbData.projection = m_Camera.GetProjectionMatrix();

    frame.commandList->writeBuffer(m_ConstantBuffer, &cbData, sizeof(cbData));

    nvrhi::GraphicsState graphicsState{};
    graphicsState.bindings = { m_BindingSet };

    nvrhi::VertexBufferBinding vertexBufferBinding{};
    vertexBufferBinding.buffer = m_VertexBuffer;
    vertexBufferBinding.offset = 0;
    vertexBufferBinding.slot = 0;

    nvrhi::IndexBufferBinding indexBufferBinding{};
    indexBufferBinding.buffer = m_IndexBuffer;
    indexBufferBinding.offset = 0;
    indexBufferBinding.format = nvrhi::Format::R32_UINT;

    graphicsState.addVertexBuffer(vertexBufferBinding);
    graphicsState.setIndexBuffer(indexBufferBinding);
    graphicsState.pipeline = m_Pipeline;
    graphicsState.framebuffer = frame.framebuffer;
    graphicsState.viewport.addViewportAndScissorRect(frame.framebuffer->getFramebufferInfo().getViewport());

    frame.commandList->setGraphicsState(graphicsState);

    nvrhi::DrawArguments drawArgs{};
    drawArgs.vertexCount = 36;

    frame.commandList->drawIndexed(drawArgs);
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