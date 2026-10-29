#include "TrianglePass.h"
#include <core/Swapchain.h>

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
  TrianglePass::TrianglePass(nvrhi::IDevice* device, nvrhi::IFramebuffer* framebuffer)
  {
    const auto vsBytes = ReadBinary("Iskra/shaders/Triangle.vs.dxil");
    const auto psBytes = ReadBinary("Iskra/shaders/Triangle.ps.dxil");

    nvrhi::ShaderDesc vsDesc{};
    vsDesc.shaderType = nvrhi::ShaderType::Vertex;
    vsDesc.debugName = "Triangle.vs";
    vsDesc.entryName = "VSMain";

    nvrhi::ShaderDesc psDesc{};
    psDesc.shaderType = nvrhi::ShaderType::Pixel;
    psDesc.debugName = "Triangle.ps";
    psDesc.entryName = "PSMain";

    auto vertexShader = device->createShader(vsDesc, vsBytes.data(), vsBytes.size());
    auto pixelShader = device->createShader(psDesc, psBytes.data(), psBytes.size());

    if (!vertexShader || !pixelShader) {
      throw std::runtime_error("Failed to create trianle shaders");
    }

    nvrhi::GraphicsPipelineDesc pipelineDesc{};
    pipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;
    pipelineDesc.VS = vertexShader;
    pipelineDesc.PS = pixelShader;

    pipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;

    pipelineDesc.renderState.depthStencilState.depthTestEnable = false;
    pipelineDesc.renderState.depthStencilState.depthWriteEnable = false;
    pipelineDesc.renderState.depthStencilState.stencilEnable = false;

    m_Pipeline = device->createGraphicsPipeline(pipelineDesc, framebuffer->getFramebufferInfo());

    if (!m_Pipeline) {
      throw std::runtime_error("Failed to create triangle pipeline");
    }



  }
  void TrianglePass::Render(const FrameContext& frame)
  {
    nvrhi::GraphicsState state{};
    state.pipeline = m_Pipeline.Get();
    state.framebuffer = frame.framebuffer;

    state.viewport.addViewportAndScissorRect(frame.framebuffer->getFramebufferInfo().getViewport());

    frame.commandList->setGraphicsState(state);

    nvrhi::DrawArguments drawArgs{};
    drawArgs.vertexCount = 3;

    frame.commandList->draw(drawArgs);
  }
}