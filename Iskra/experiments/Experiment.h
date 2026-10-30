#pragma once

#include <string_view>
#include <core/Swapchain.h>
#include <core/Input.h>

namespace Iskra {
  class Experiment {
  public:
    virtual void OnInit(nvrhi::IDevice* device, const nvrhi::FramebufferInfo& framebufferInfo) = 0;
    virtual ~Experiment() = default;

    virtual void OnUpdate(float deltaTime) = 0;
    virtual void OnInput(float deltaTime, Input& input) = 0;
    virtual void OnRender(FrameContext &frame) = 0;
    virtual void OnUiRender(FrameContext &frame) = 0;
    virtual void OnDestroy() = 0;
    virtual void OnResize(int width, int height) = 0;

    virtual std::string_view GetName() const = 0;
  protected:
    nvrhi::IDevice* m_Device = nullptr;
  };
}