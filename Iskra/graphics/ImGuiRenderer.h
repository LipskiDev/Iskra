#pragma once

#include <nvrhi/nvrhi.h>
#include <imgui.h>
#include <unordered_map>
#include <vector>

namespace Iskra {
  class ImGuiRenderer {
  public:
    void Initialize(nvrhi::IDevice* device, const nvrhi::FramebufferInfo& outputInfo);
    void Shutdown();

    void Render(nvrhi::ICommandList* commandList,
      nvrhi::IFramebuffer* framebuffer,
      ImDrawData* drawData);

  private:
    void UpdateTexture(nvrhi::ICommandList* commandList, ImTextureData* texture);
    void CreateBuffers(size_t vertexCount, size_t indexCount);

    struct TextureResources {
      nvrhi::TextureHandle texture;
      nvrhi::BindingSetHandle bindings;
    };

    struct ConstantBufferData {
      float scale[2];
      float translate[2];
    };

    nvrhi::IDevice* m_Device = nullptr;
    nvrhi::GraphicsPipelineHandle m_Pipeline;
    nvrhi::BindingLayoutHandle m_BindingLayout;
    nvrhi::SamplerHandle m_Sampler;
    nvrhi::BufferHandle m_ConstantBuffer;
    nvrhi::BufferHandle m_VertexBuffer;
    nvrhi::BufferHandle m_IndexBuffer;

    size_t m_VertexCapacity = 0;
    size_t m_IndexCapacity = 0;
    ImTextureID m_NextTextureId = 1;
    std::unordered_map<ImTextureID, TextureResources> m_Textures;
    std::vector<ImDrawVert> m_Vertices;
    std::vector<ImDrawIdx> m_Indices;
  };
}
