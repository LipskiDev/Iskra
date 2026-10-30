#include <graphics/passes/VertexAndIndexBuffersTestPass.h>

#include <core/Swapchain.h>

#include <glm/glm.hpp>


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
  // cube vertices
  struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
  };

  VertexAndIndexBuffersTestPass::VertexAndIndexBuffersTestPass(nvrhi::IDevice* device, nvrhi::IFramebuffer* framebuffer)
  {
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

    pipelineDesc.inputLayout = inputLayout;

    m_Pipeline = device->createGraphicsPipeline(pipelineDesc, framebuffer->getFramebufferInfo());

    if (!m_Pipeline) {
      throw std::runtime_error("Failed to create cube pipeline");
    }

    Vertex vertices[] = {
      { { -0.5464f, -0.3327f, 0.4212f },{ 1.0f, 0.0f, 0.0f } },
      { { 0.1464f, -0.4695f, 0.2332f },{ 0.0f, 1.0f, 0.0f } },
      { { 0.1464f,  0.2823f, 0.0964f },{ 0.0f, 0.0f, 1.0f } },
      { { -0.5464f,  0.4191f, 0.2844f },{ 1.0f, 1.0f, 0.0f } },

      { { -0.1464f, -0.2823f, 0.9036f },{ 1.0f, 0.5f, 0.5f } },
      { { 0.5464f, -0.4191f, 0.7156f },{ 0.5f, 1.0f, 0.5f } },
      { { 0.5464f,  0.3327f, 0.5788f },{ 0.5f, 0.5f, 1.0f } },
      { { -0.1464f,  0.4695f, 0.7668f },{ 1.0f, 1.0f, 1.0f } }
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

  void VertexAndIndexBuffersTestPass::Render(const FrameContext& frame)
  {
    
    nvrhi::GraphicsState state{};
    state.pipeline = m_Pipeline.Get();
    state.framebuffer = frame.framebuffer;
    state.viewport.addViewportAndScissorRect(frame.framebuffer->getFramebufferInfo().getViewport());

    nvrhi::VertexBufferBinding vertexBufferBinding{};
    vertexBufferBinding.buffer = m_VertexBuffer;
    vertexBufferBinding.slot = 0;
    vertexBufferBinding.offset = 0;

    state.addVertexBuffer(vertexBufferBinding);

    nvrhi::IndexBufferBinding indexBufferBinding{};
    indexBufferBinding.buffer = m_IndexBuffer;
    indexBufferBinding.format = nvrhi::Format::R32_UINT;
    indexBufferBinding.offset = 0;


    state.setIndexBuffer(indexBufferBinding);
    frame.commandList->setGraphicsState(state);
    nvrhi::DrawArguments drawArgs{};
    drawArgs.vertexCount = 36;
    drawArgs.startVertexLocation = 0;
    drawArgs.startIndexLocation = 0;

    frame.commandList->drawIndexed(drawArgs);
  }
}