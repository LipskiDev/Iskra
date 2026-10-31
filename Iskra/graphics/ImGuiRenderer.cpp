#include <graphics/ImGuiRenderer.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
  std::vector<char> ReadShader(const char* path)
  {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
      throw std::runtime_error(std::string("Cannot open UI shader: ") + path);
    }

    return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
  }
}

namespace Iskra {
  void ImGuiRenderer::Initialize(nvrhi::IDevice* device, const nvrhi::FramebufferInfo& outputInfo)
  {
    m_Device = device;

    nvrhi::BufferDesc constants{};
    constants.byteSize = sizeof(ConstantBufferData);
    constants.isConstantBuffer = true;
    constants.isVolatile = true;
    constants.initialState = nvrhi::ResourceStates::ConstantBuffer;
    constants.keepInitialState = true;
    constants.debugName = "ImGui constants";
    m_ConstantBuffer = device->createBuffer(constants);
    m_Sampler = device->createSampler(nvrhi::SamplerDesc{});

    nvrhi::BindingLayoutDesc layout{};
    layout.visibility = nvrhi::ShaderType::AllGraphics;
    layout.bindings = {
      nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
      nvrhi::BindingLayoutItem::Texture_SRV(0),
      nvrhi::BindingLayoutItem::Sampler(0)
    };
    m_BindingLayout = device->createBindingLayout(layout);

    const auto vsBytes = ReadShader(ISKRA_SHADER_DIR "/ImGui.vs.dxil");
    const auto psBytes = ReadShader(ISKRA_SHADER_DIR "/ImGui.ps.dxil");
    nvrhi::ShaderDesc vsDesc{};
    vsDesc.shaderType = nvrhi::ShaderType::Vertex;
    vsDesc.entryName = "VSMain";
    vsDesc.debugName = "ImGui VS";
    nvrhi::ShaderDesc psDesc{};
    psDesc.shaderType = nvrhi::ShaderType::Pixel;
    psDesc.entryName = "PSMain";
    psDesc.debugName = "ImGui PS";
    auto vs = device->createShader(vsDesc, vsBytes.data(), vsBytes.size());
    auto ps = device->createShader(psDesc, psBytes.data(), psBytes.size());

    if (!m_ConstantBuffer || !m_Sampler || !m_BindingLayout || !vs || !ps) {
      throw std::runtime_error("Failed to create ImGui resources");
    }

    nvrhi::VertexAttributeDesc attributes[3]{};
    attributes[0].name = "POSITION";
    attributes[0].format = nvrhi::Format::RG32_FLOAT;
    attributes[0].offset = offsetof(ImDrawVert, pos);
    attributes[1].name = "TEXCOORD";
    attributes[1].format = nvrhi::Format::RG32_FLOAT;
    attributes[1].offset = offsetof(ImDrawVert, uv);
    attributes[2].name = "COLOR";
    attributes[2].format = nvrhi::Format::RGBA8_UNORM;
    attributes[2].offset = offsetof(ImDrawVert, col);
    for (auto& attribute : attributes) {
      attribute.elementStride = sizeof(ImDrawVert);
      attribute.bufferIndex = 0;
    }

    nvrhi::GraphicsPipelineDesc pipeline{};
    pipeline.VS = vs;
    pipeline.PS = ps;
    pipeline.inputLayout = device->createInputLayout(attributes, 3, vs);
    if (!pipeline.inputLayout) {
      throw std::runtime_error("Failed to create ImGui input layout");
    }
    pipeline.addBindingLayout(m_BindingLayout);
    pipeline.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
    pipeline.renderState.rasterState.scissorEnable = true;
    pipeline.renderState.depthStencilState.depthTestEnable = false;
    pipeline.renderState.depthStencilState.depthWriteEnable = false;
    pipeline.renderState.depthStencilState.stencilEnable = false;
    auto& blend = pipeline.renderState.blendState.targets[0];
    blend.blendEnable = true;
    blend.srcBlend = nvrhi::BlendFactor::SrcAlpha;
    blend.destBlend = nvrhi::BlendFactor::InvSrcAlpha;
    blend.srcBlendAlpha = nvrhi::BlendFactor::One;
    blend.destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
    m_Pipeline = device->createGraphicsPipeline(pipeline, outputInfo);
    if (!m_Pipeline) {
      throw std::runtime_error("Failed to create ImGui pipeline");
    }

    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "Iskra_NVRHI";
    io.BackendRendererUserData = this;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
    ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
    platform.Renderer_TextureMaxWidth = 16384;
    platform.Renderer_TextureMaxHeight = 16384;
  }

  void ImGuiRenderer::CreateBuffers(size_t vertexCount, size_t indexCount)
  {
    if (vertexCount > m_VertexCapacity) {
      m_VertexCapacity = vertexCount + 5000;
      nvrhi::BufferDesc desc{};
      desc.byteSize = m_VertexCapacity * sizeof(ImDrawVert);
      desc.isVertexBuffer = true;
      desc.initialState = nvrhi::ResourceStates::VertexBuffer;
      desc.keepInitialState = true;
      desc.debugName = "ImGui vertices";
      m_VertexBuffer = m_Device->createBuffer(desc);
    }

    if (indexCount > m_IndexCapacity) {
      m_IndexCapacity = indexCount + 10000;
      nvrhi::BufferDesc desc{};
      desc.byteSize = m_IndexCapacity * sizeof(ImDrawIdx);
      desc.isIndexBuffer = true;
      desc.initialState = nvrhi::ResourceStates::IndexBuffer;
      desc.keepInitialState = true;
      desc.debugName = "ImGui indices";
      m_IndexBuffer = m_Device->createBuffer(desc);
    }

    if (!m_VertexBuffer || !m_IndexBuffer) {
      throw std::runtime_error("Failed to create ImGui geometry buffers");
    }
  }

  void ImGuiRenderer::UpdateTexture(nvrhi::ICommandList* commandList, ImTextureData* texture)
  {
    if (texture->Status == ImTextureStatus_WantDestroy) {
      if (texture->UnusedFrames > 0) {
        m_Textures.erase(texture->GetTexID());
        texture->SetTexID(ImTextureID_Invalid);
        texture->BackendUserData = nullptr;
        texture->SetStatus(ImTextureStatus_Destroyed);
      }
      return;
    }

    if (texture->Status != ImTextureStatus_WantCreate && texture->Status != ImTextureStatus_WantUpdates) {
      return;
    }

    if (texture->Status == ImTextureStatus_WantCreate) {
      nvrhi::TextureDesc desc{};
      desc.width = texture->Width;
      desc.height = texture->Height;
      desc.format = nvrhi::Format::RGBA8_UNORM;
      desc.initialState = nvrhi::ResourceStates::ShaderResource;
      desc.keepInitialState = true;
      desc.debugName = "ImGui texture";

      TextureResources resources;
      resources.texture = m_Device->createTexture(desc);
      if (!resources.texture) {
        throw std::runtime_error("Failed to create ImGui texture");
      }

      nvrhi::BindingSetDesc bindings{};
      bindings.bindings = {
        nvrhi::BindingSetItem::ConstantBuffer(0, m_ConstantBuffer),
        nvrhi::BindingSetItem::Texture_SRV(0, resources.texture),
        nvrhi::BindingSetItem::Sampler(0, m_Sampler)
      };
      resources.bindings = m_Device->createBindingSet(bindings, m_BindingLayout);
      if (!resources.bindings) {
        throw std::runtime_error("Failed to create ImGui texture bindings");
      }

      const ImTextureID id = m_NextTextureId++;
      m_Textures.emplace(id, resources);
      texture->SetTexID(id);
    }

    auto& resources = m_Textures.at(texture->GetTexID());
    // Upload the whole atlas on updates. The existing frame fence protects reuse.
    if (texture->Format == ImTextureFormat_RGBA32) {
      commandList->writeTexture(resources.texture, 0, 0, texture->GetPixels(), texture->GetPitch());
    }
    else {
      const auto* pixels = static_cast<const unsigned char*>(texture->GetPixels());
      std::vector<unsigned char> rgba(static_cast<size_t>(texture->Width) * texture->Height * 4);
      for (size_t i = 0; i < rgba.size() / 4; ++i) {
        rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = pixels[i];
      }
      commandList->writeTexture(resources.texture, 0, 0, rgba.data(), texture->Width * 4);
    }
    texture->SetStatus(ImTextureStatus_OK);
  }

  void ImGuiRenderer::Render(nvrhi::ICommandList* commandList,
    nvrhi::IFramebuffer* framebuffer,
    ImDrawData* drawData)
  {
    if (!drawData) {
      return;
    }
    if (drawData->Textures) {
      for (ImTextureData* texture : *drawData->Textures) {
        UpdateTexture(commandList, texture);
      }
    }

    const int width = static_cast<int>(drawData->DisplaySize.x * drawData->FramebufferScale.x);
    const int height = static_cast<int>(drawData->DisplaySize.y * drawData->FramebufferScale.y);
    if (width <= 0 || height <= 0 || drawData->TotalVtxCount == 0 || drawData->TotalIdxCount == 0) {
      return;
    }

    CreateBuffers(drawData->TotalVtxCount, drawData->TotalIdxCount);
    m_Vertices.clear();
    m_Indices.clear();
    for (const ImDrawList* list : drawData->CmdLists) {
      m_Vertices.insert(m_Vertices.end(), list->VtxBuffer.begin(), list->VtxBuffer.end());
      m_Indices.insert(m_Indices.end(), list->IdxBuffer.begin(), list->IdxBuffer.end());
    }
    commandList->writeBuffer(m_VertexBuffer, m_Vertices.data(), m_Vertices.size() * sizeof(ImDrawVert));
    commandList->writeBuffer(m_IndexBuffer, m_Indices.data(), m_Indices.size() * sizeof(ImDrawIdx));

    ConstantBufferData constants{};
    constants.scale[0] = 2.0f / drawData->DisplaySize.x;
    constants.scale[1] = -2.0f / drawData->DisplaySize.y;
    constants.translate[0] = -1.0f - drawData->DisplayPos.x * constants.scale[0];
    constants.translate[1] = 1.0f - drawData->DisplayPos.y * constants.scale[1];
    commandList->writeBuffer(m_ConstantBuffer, &constants, sizeof(constants));

    uint32_t vertexOffset = 0;
    uint32_t indexOffset = 0;
    for (const ImDrawList* list : drawData->CmdLists) {
      for (const ImDrawCmd& draw : list->CmdBuffer) {
        if (draw.UserCallback) {
          if (draw.UserCallback != ImDrawCallback_ResetRenderState) {
            draw.UserCallback(list, &draw);
          }
          // Every draw below restores the full graphics state.
          commandList->writeBuffer(m_ConstantBuffer, &constants, sizeof(constants));
          continue;
        }

        const ImVec2 origin = drawData->DisplayPos;
        const ImVec2 scale = drawData->FramebufferScale;
        const int minX = static_cast<int>(std::clamp(std::floor((draw.ClipRect.x - origin.x) * scale.x), 0.0f, static_cast<float>(width)));
        const int minY = static_cast<int>(std::clamp(std::floor((draw.ClipRect.y - origin.y) * scale.y), 0.0f, static_cast<float>(height)));
        const int maxX = static_cast<int>(std::clamp(std::ceil((draw.ClipRect.z - origin.x) * scale.x), 0.0f, static_cast<float>(width)));
        const int maxY = static_cast<int>(std::clamp(std::ceil((draw.ClipRect.w - origin.y) * scale.y), 0.0f, static_cast<float>(height)));
        if (maxX <= minX || maxY <= minY || draw.ElemCount == 0) {
          continue;
        }

        const auto texture = m_Textures.find(draw.GetTexID());
        if (texture == m_Textures.end()) {
          throw std::runtime_error("ImGui draw references an unregistered texture");
        }

        nvrhi::GraphicsState state{};
        state.pipeline = m_Pipeline;
        state.framebuffer = framebuffer;
        state.addBindingSet(texture->second.bindings);
        state.viewport.addViewport(nvrhi::Viewport(static_cast<float>(width), static_cast<float>(height)));
        state.viewport.addScissorRect(nvrhi::Rect(minX, maxX, minY, maxY));
        nvrhi::VertexBufferBinding vertices{};
        vertices.buffer = m_VertexBuffer;
        state.addVertexBuffer(vertices);
        nvrhi::IndexBufferBinding indices{};
        indices.buffer = m_IndexBuffer;
        indices.format = sizeof(ImDrawIdx) == 2 ? nvrhi::Format::R16_UINT : nvrhi::Format::R32_UINT;
        state.setIndexBuffer(indices);
        commandList->setGraphicsState(state);

        nvrhi::DrawArguments args{};
        args.vertexCount = draw.ElemCount;
        args.startIndexLocation = indexOffset + draw.IdxOffset;
        args.startVertexLocation = vertexOffset + draw.VtxOffset;
        commandList->drawIndexed(args);
      }
      vertexOffset += list->VtxBuffer.Size;
      indexOffset += list->IdxBuffer.Size;
    }
  }

  void ImGuiRenderer::Shutdown()
  {
    if (ImGui::GetCurrentContext() && ImGui::GetIO().BackendRendererUserData == this) {
      for (ImTextureData* texture : ImGui::GetPlatformIO().Textures) {
        if (m_Textures.find(texture->GetTexID()) != m_Textures.end()) {
          texture->SetTexID(ImTextureID_Invalid);
          texture->SetStatus(ImTextureStatus_Destroyed);
          texture->BackendUserData = nullptr;
        }
      }
      ImGuiIO& io = ImGui::GetIO();
      io.BackendRendererName = nullptr;
      io.BackendRendererUserData = nullptr;
      io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
    }
    m_Textures.clear();
    m_Pipeline = nullptr;
    m_BindingLayout = nullptr;
    m_Sampler = nullptr;
    m_ConstantBuffer = nullptr;
    m_VertexBuffer = nullptr;
    m_IndexBuffer = nullptr;
    m_VertexCapacity = m_IndexCapacity = 0;
    m_Device = nullptr;
  }
}
