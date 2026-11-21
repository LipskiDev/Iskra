#pragma once

#include <experiments/Experiment.h>

#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float2.hpp>
#include <cstdint>

namespace Iskra::RTPT {

enum class MaterialType : uint32_t { Lambertian, Metal, Dielectric, Emissive };

struct Material {
  glm::vec3 albedo;
  MaterialType type;

  glm::vec3 emission;
  float roughness;

  float ior;
  float padding[3];
};

static_assert(sizeof(Material) == 48);

class RTPTExperiment : public Experiment {
 public:
  void OnInit(nvrhi::IDevice *device,
              const nvrhi::FramebufferInfo &framebufferInfo) override;
  ~RTPTExperiment() = default;

  void OnUpdate(float deltaTime) override;
  void OnInput(float deltaTime, Input &input) override;
  void OnRender(FrameContext &frame) override;
  void OnUiRender(FrameContext &frame) override;
  void OnDestroy() override;
  void OnResize(int width, int height) override;

  std::string_view GetName() const override;

 private:
  void CreateOutput(uint32_t width, uint32_t height, nvrhi::Format format);

  struct UniformBuffer {
    float cameraPosition[3];
    float tanHalfFovY;
    float cameraDirection[3];
    int frame;
    int seed;
    int padding[3];
  };

  static_assert(sizeof(UniformBuffer) == 48);

  nvrhi::rt::AccelStructHandle m_BLAS;
  nvrhi::rt::AccelStructHandle m_TLAS;
  nvrhi::rt::PipelineHandle m_Pipeline;
  nvrhi::rt::ShaderTableHandle m_ShaderTable;
  nvrhi::TextureHandle m_Output;
  nvrhi::TextureHandle m_AccumulationTexture;
  nvrhi::BindingSetHandle m_RayTracingBindingSet;
  nvrhi::BufferHandle m_AabbsBuffer;
  nvrhi::BufferHandle m_SpheresBuffer;
  nvrhi::BufferHandle m_MaterialsBuffer;
  nvrhi::BufferHandle m_UniformBuffer;
  nvrhi::BindingLayoutHandle m_RayTracingBindingLayout;

  int m_Seed = 0;
  int m_Frame = 0;
  glm::vec3 m_CameraPosition{0.0f, 0.0f, 3.0f};
  glm::vec3 m_CameraDirection{0.0f, 0.0f, -1.0f};
  glm::vec2 m_LastMousePosition{0.0f};
  float m_CameraYaw = 0.0f;
  float m_CameraPitch = 0.0f;
  bool m_WasLooking = false;
};
}  // namespace Iskra::RTPT
