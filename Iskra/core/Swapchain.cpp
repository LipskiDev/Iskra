#include "Swapchain.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <stdexcept>
#include <string>

namespace {
  void CheckHR(HRESULT result, const char* operation) {
    if (FAILED(result)) {
      throw std::runtime_error(std::string(operation) + " failed");
    }
  }
}

namespace Iskra {
  Swapchain::Swapchain(GraphicsContext& graphics, const Window& window)
    : m_Device(graphics.GetDevice())
  {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window.GetHandle(), &width, &height);

    if (width <= 0 || height <= 0) {
      throw std::runtime_error("Cannot create swapchain with an empty framebuffer");
    }

    m_Size = {
      .width = static_cast<unsigned int>(width),
      .height = static_cast<unsigned int>(height)
    };

    CheckHR(CreateDXGIFactory2(0, IID_PPV_ARGS(&m_Factory)), "CreateDXGIFactory2");

    HWND hwnd = glfwGetWin32Window(window.GetHandle());

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = m_Size.width;
    desc.Height = m_Size.height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = BufferCount;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapchain;

    CheckHR(m_Factory->CreateSwapChainForHwnd(graphics.GetGraphicsQueue(), hwnd, &desc, nullptr, nullptr, &swapchain), "CreateSwapChainForHwnd");

    CheckHR(
      swapchain.As(&m_Swapchain),
      "Query IDXGISwapChain3");

    CheckHR(
      m_Factory->MakeWindowAssociation(
        hwnd, DXGI_MWA_NO_ALT_ENTER),
      "MakeWindowAssociation");

    CreateBackbuffers();

    m_CommandList = m_Device->createCommandList();

    if (!m_CommandList) {
      throw std::runtime_error("Failed to create command list");
    }
  }

  Swapchain::~Swapchain()
  {
    m_Device->waitForIdle();
  }

  std::optional<FrameContext> Swapchain::BeginFrame()
  {
    if (m_Size.width == 0 || m_Size.height == 0) return std::nullopt;

    const unsigned int index = m_Swapchain->GetCurrentBackBufferIndex();

    m_CommandList->open();

    FrameContext frame{};
    frame.backbuffer = m_Backbuffers[index].Get();
    frame.framebuffer = m_Framebuffers[index].Get();
    frame.depthBuffer = m_DepthBuffers[index].Get();
    frame.commandList = m_CommandList.Get();

    return frame;
  }

  void Swapchain::Present(FrameContext& frame)
  {
    frame.commandList->close();
    m_Device->executeCommandList(frame.commandList);

    CheckHR(m_Swapchain->Present(1, 0), "Present");

    // Simple synchronization for a first MVP
    if (!m_Device->waitForIdle()) {
      throw std::runtime_error("Failed to wait for GPU");
    }

    m_Device->runGarbageCollection();
  }

  void Swapchain::Resize(Extent2D newSize)
  {
    if (newSize.height == m_Size.height &&
      newSize.width == m_Size.width) {
      return;
    }

    // Keeps existing buffers while minimized; BeginFrame skips drawing
    if (newSize.width == 0 || newSize.height == 0) {
      m_Size = newSize;
      return;
    }

    if (!m_Device->waitForIdle()) {
      throw std::runtime_error("Failed to wait before resize");
    }

    // Discards the command list too, so it cannot retain old buffer.
    m_CommandList = nullptr;
    ReleaseBackbuffers();
    m_Device->runGarbageCollection();

    CheckHR(
      m_Swapchain->ResizeBuffers(
        BufferCount,
        newSize.width,
        newSize.height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        0),
      "ResizeBuffers");

    m_Size = newSize;
    CreateBackbuffers();

    m_CommandList = m_Device->createCommandList();

    if (!m_CommandList) {
      throw std::runtime_error("Failed to recreate command list");
    }
  }

  nvrhi::FramebufferInfo Swapchain::GetFramebufferInfo() const
  {
    return m_Framebuffers[0]->getFramebufferInfo();
  }

  void Swapchain::CreateBackbuffers()
  {
    for (unsigned int i = 0; i < BufferCount; ++i) {
      Microsoft::WRL::ComPtr<ID3D12Resource> nativeBuffer;

      CheckHR(m_Swapchain->GetBuffer(i, IID_PPV_ARGS(&nativeBuffer)), "GetBuffer");

      nvrhi::TextureDesc desc{};
      desc.width = m_Size.width;
      desc.height = m_Size.height;
      desc.format = nvrhi::Format::RGBA8_UNORM;
      desc.isRenderTarget = true;
      desc.initialState = nvrhi::ResourceStates::Present;
      desc.keepInitialState = true;

      nvrhi::TextureDesc depthDesc{};
      depthDesc.width = m_Size.width;
      depthDesc.height = m_Size.height;
      depthDesc.format = nvrhi::Format::D32;
      depthDesc.isRenderTarget = true;
      depthDesc.initialState = nvrhi::ResourceStates::DepthWrite;
      depthDesc.keepInitialState = true;
      depthDesc.debugName = "Depth Buffer";

      m_Backbuffers[i] = m_Device->createHandleForNativeTexture(
        nvrhi::ObjectTypes::D3D12_Resource,
        nvrhi::Object(nativeBuffer.Get()), 
        desc
      );


      if (!m_Backbuffers[i]) {
        throw std::runtime_error("Failed to wrap backbuffer");
      }

      m_DepthBuffers[i] = m_Device->createTexture(depthDesc);

      if (!m_DepthBuffers[i]) {
        throw std::runtime_error("Failed to create depth buffer");
      }

      nvrhi::FramebufferDesc framebufferDesc;
      framebufferDesc.addColorAttachment(m_Backbuffers[i]);
      framebufferDesc.setDepthAttachment(m_DepthBuffers[i]);

      m_Framebuffers[i] = m_Device->createFramebuffer(framebufferDesc);

      if (!m_Framebuffers[i]) {
        throw std::runtime_error("Failed to create framebuffer");
      }

    }
  }

  void Swapchain::ReleaseBackbuffers()
  {
    for (auto& framebuffer : m_Framebuffers) {
      framebuffer = nullptr;
    }

    for (auto& backbuffer : m_Backbuffers) {
      backbuffer = nullptr;
    }

    for (auto& depthBuffer : m_DepthBuffers) {
      depthBuffer = nullptr;
    }
  }

}
