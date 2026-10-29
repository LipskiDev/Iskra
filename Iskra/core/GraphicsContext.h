#pragma once

#include <nvrhi/nvrhi.h>
#include <nvrhi/d3d12.h>
#include <wrl/client.h>

namespace Iskra {

  // static GraphicsConfig& GetGraphicsConfig() {
  //   GraphicsConfig config;
  //   return config;
  // }

  struct GraphicsConfig
  {
    bool enableValidationLayers = false;
  };

  class GraphicsContext {
  public:
    explicit GraphicsContext(const GraphicsConfig& config);

    nvrhi::IDevice* GetDevice() const {
      return m_Device.Get();
    }

    ID3D12Device* GetNativeDevice() const {
      return m_NativeDevice.Get();
    }

    ID3D12CommandQueue* GetGraphicsQueue() const {
      return m_GraphicsQueue.Get();
    }

  private:
    class MessageCallback final : public nvrhi::IMessageCallback {
    public:
      void message(nvrhi::MessageSeverity severity, const char* text) override;
    };

    MessageCallback m_MessageCallback;
    Microsoft::WRL::ComPtr<ID3D12Device> m_NativeDevice;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_GraphicsQueue;
    nvrhi::DeviceHandle m_Device = nullptr;
  };
}