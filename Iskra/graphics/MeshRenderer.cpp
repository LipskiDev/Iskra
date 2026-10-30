#include <graphics/MeshRenderer.h>

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
  void MeshRenderer::Initialize(nvrhi::IDevice* device, const nvrhi::FramebufferInfo& outputInfo)
  {
    m_Device = device;
    nvrhi::BufferDesc constantBufferDesc{};
    constantBufferDesc.byteSize = sizeof(ConstantBufferData);
    constantBufferDesc.debugName = "Constant Buffer";
    constantBufferDesc.isConstantBuffer = true;
    constantBufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
    constantBufferDesc.keepInitialState = true;
    constantBufferDesc.isVolatile = true;

    m_ConstantBuffer = device->createBuffer(constantBufferDesc);

    if (!m_ConstantBuffer) {
      throw std::runtime_error("Failed to create mesh constant buffer");
    }

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

    auto inputLayout = device->createInputLayout(vertexAttributes, 2, vertexShader);

    nvrhi::BindingLayoutDesc bindingLayoutDesc{};
    bindingLayoutDesc.visibility = nvrhi::ShaderType::Vertex;
    bindingLayoutDesc.bindings = {
      { nvrhi::BindingLayoutItem::VolatileConstantBuffer(0) }
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

    m_Pipeline = m_Device->createGraphicsPipeline(pipelineDesc, outputInfo);

    if (!m_Pipeline) {
      throw std::runtime_error("Failed to create mesh pipeline");
    }
  }

  void MeshRenderer::Render(const FrameContext& frame, const ModelData& model, const std::vector<GpuMesh>& gpuMeshes, const Camera& camera, const glm::mat4& placement)
  {
    DrawNode(frame, model.root, placement, gpuMeshes, camera);
  }

  void MeshRenderer::DrawNode(
    const FrameContext& frame,
    const ModelNode& node,
    const glm::mat4& parentTransform,
    const std::vector<GpuMesh>& gpuMeshes,
    const Camera& camera)
  {
    const glm::mat4 world =
      parentTransform * node.localTransform;

    for (const MeshInstanceData& instance : node.meshInstances) {
      const GpuMesh& mesh = gpuMeshes.at(instance.meshIndex);

      ConstantBufferData constants{};
      constants.model = world;
      constants.view = camera.GetViewMatrix();
      constants.projection = camera.GetProjectionMatrix();

      frame.commandList->writeBuffer(
        m_ConstantBuffer, &constants, sizeof(constants));

      nvrhi::GraphicsState state{};
      state.pipeline = m_Pipeline.Get();
      state.framebuffer = frame.framebuffer;
      state.addBindingSet(m_BindingSet);

      state.viewport.addViewportAndScissorRect(
        frame.framebuffer->getFramebufferInfo().getViewport());

      nvrhi::VertexBufferBinding vertices{};
      vertices.buffer = mesh.vertexBuffer;
      vertices.slot = 0;
      vertices.offset = 0;
      state.addVertexBuffer(vertices);

      nvrhi::IndexBufferBinding indices{};
      indices.buffer = mesh.indexBuffer;
      indices.format = nvrhi::Format::R32_UINT;
      indices.offset = 0;
      state.setIndexBuffer(indices);

      frame.commandList->setGraphicsState(state);

      nvrhi::DrawArguments args{};
      args.vertexCount = mesh.indexCount;
      frame.commandList->drawIndexed(args);
    }

    for (const ModelNode& child : node.children) {
      DrawNode(frame, child, world, gpuMeshes, camera);
    }
  }
}
