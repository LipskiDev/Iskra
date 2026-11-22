#pragma once

#include <experiments/Experiment.h>
#include <assets/AssetRegistry.h>
#include <graphics/render_instance.h>

#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float2.hpp>
#include <cstdint>

#include <scene/World.h>

namespace Iskra::RTPT {

enum class MaterialType : uint32_t { Lambertian, Metal, Dielectric, Emissive };

struct AnalyticalSphereComponent {
  float radius;
  MaterialHandle material;
};

struct PathTracingMaterialGpu {
  glm::vec3 baseColor;
  float metallic;

  glm::vec3 emission;
  float roughness;

  float transmission;
  float ior;
  float padding[2];
};

static_assert(sizeof(PathTracingMaterialGpu) == 48);

struct AABBComponent {
  glm::vec3 min;
  float padding0;
  glm::vec3 max;
  float padding1;
};

struct Sphere {
  float center[3];
  float radius;
  uint32_t materialIndex;
  uint32_t padding[3];
};

static_assert(sizeof(Sphere) == 32);

struct RTVertex {
  glm::vec3 position;
  glm::vec3 normal;
};

static_assert(sizeof(RTVertex) == 24);

struct TriangleInstanceData {
  uint32_t vertexOffset;
  uint32_t indexOffset;
  uint32_t materialIndex;
  uint32_t padding;
};

static_assert(sizeof(TriangleInstanceData) == 16);

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

  void CreateSceneEntities();
  void ExtractScene();
  void UploadScene();
  void BuildAccelerationStructures();

  struct UniformBuffer {
    float cameraPosition[3];
    float tanHalfFovY;
    float cameraDirection[3];
    int frame;
    int seed;
    int padding[3];
  };

  static_assert(sizeof(UniformBuffer) == 48);

  nvrhi::rt::AccelStructHandle m_SphereBLAS;
  std::vector<nvrhi::rt::AccelStructHandle> m_TriangleBLASes;
  nvrhi::rt::AccelStructHandle m_TLAS;
  nvrhi::rt::PipelineHandle m_Pipeline;
  nvrhi::rt::ShaderTableHandle m_ShaderTable;
  nvrhi::TextureHandle m_Output;
  nvrhi::TextureHandle m_AccumulationTexture;
  nvrhi::BindingSetHandle m_RayTracingBindingSet;
  nvrhi::BufferHandle m_AabbsBuffer;
  nvrhi::BufferHandle m_SpheresBuffer;
  nvrhi::BufferHandle m_VerticesBuffer;
  nvrhi::BufferHandle m_IndicesBuffer;
  nvrhi::BufferHandle m_MaterialsBuffer;
  nvrhi::BufferHandle m_TriangleInstancesBuffer;
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

  World m_World;
  AssetRegistry m_AssetRegistry;
  std::vector<RenderInstance> m_RenderInstances;

  static constexpr uint32_t MaxSpheres = 488;

  std::vector<Sphere> spheres;
  std::vector<RTVertex> vertices;
  std::vector<uint32_t> indices;
  std::vector<PathTracingMaterialGpu> materials;
  std::vector<TriangleInstanceData> triangleInstances;
  std::vector<nvrhi::rt::GeometryAABB> bounds;
};
}  // namespace Iskra::RTPT
