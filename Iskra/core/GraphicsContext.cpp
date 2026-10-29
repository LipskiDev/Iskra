#include "GraphicsContext.h"

#include <directx/d3d12sdklayers.h>
#include <nvrhi/validation.h>

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
  void CheckHR(HRESULT result, const char* operation) {
    if (FAILED(result)) {
      throw std::runtime_error(std::string(operation)
        + " failed, HRESULT = "
        + std::to_string(static_cast<unsigned long>(result)));
    }
  }
}

namespace Iskra {

  void GraphicsContext::MessageCallback::message(nvrhi::MessageSeverity severity, const char* text) {
    std::cerr << "[NVRHI " << static_cast<int>(severity) << "] " << text << '\n';
  }

  GraphicsContext::GraphicsContext(const GraphicsConfig& config)
  {
    if (config.enableValidationLayers) {
      Microsoft::WRL::ComPtr<ID3D12Debug> debug;

      CheckHR(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)), "D3D12GetDebugInterface");

      debug->EnableDebugLayer();
    }

    CheckHR(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_NativeDevice)), "D3D12CreateDevice");
    
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    CheckHR(m_NativeDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_GraphicsQueue)), "CreateCommandQueue");

    nvrhi::d3d12::DeviceDesc deviceDesc{};
    deviceDesc.errorCB = &m_MessageCallback;
    deviceDesc.pDevice = m_NativeDevice.Get();
    deviceDesc.pGraphicsCommandQueue = m_GraphicsQueue.Get();

    m_Device = nvrhi::d3d12::createDevice(deviceDesc);

    if (!m_Device) {
      throw std::runtime_error("Failed to create NVRHI device");
    }

    if (config.enableValidationLayers) {
      m_Device = nvrhi::validation::createValidationLayer(m_Device.Get());

      if (!m_Device) {
        throw std::runtime_error("Failed to create NVRHI validation layer");
      }
    }
  }
}
