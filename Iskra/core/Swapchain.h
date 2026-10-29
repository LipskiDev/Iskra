#pragma once

#include "GraphicsContext.h"
#include "window.h"

#include <dxgi1_4.h>
#include <array>
#include <optional>

namespace Iskra {

  struct FrameContext {
    nvrhi::ITexture* backbuffer;
    nvrhi::ITexture* depthBuffer;
    nvrhi::IFramebuffer* framebuffer;
    nvrhi::ICommandList* commandList;
  };

  class Swapchain {
  public:
    Swapchain(GraphicsContext& graphics, const Window& window);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    std::optional<FrameContext> BeginFrame();

    void Present(FrameContext& frame);
    void Resize(Extent2D newSize);

  private:
    void CreateBackbuffers();
    void ReleaseBackbuffers();

    static constexpr unsigned int BufferCount = 2;

    nvrhi::IDevice* m_Device;
    Extent2D m_Size{};

    Microsoft::WRL::ComPtr<IDXGIFactory4> m_Factory;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_Swapchain;

    std::array<nvrhi::TextureHandle, BufferCount> m_Backbuffers;
    std::array<nvrhi::FramebufferHandle, BufferCount> m_Framebuffers;
    nvrhi::CommandListHandle m_CommandList;
  };
}