#pragma once

#include <experiments/Experiment.h>
#include <scene/Camera.h>
#include <scene/FlyingCameraController.h>
#include <assets/ModelData.h>
#include <graphics/GpuData.h>
#include <graphics/MeshRenderer.h>
#include <graphics/MeshDrawItem.h>
#include <graphics/RayTracingScene.h>
#include <assets/AssetRegistry.h>

namespace Iskra {

struct DDGIVolumeDesc {
  std::string name;
  glm::vec3 center;
  glm::vec3 halfSize;

  bool showProbes = true;
  glm::vec3 probeCount = {8, 8, 8};

  float probeHysteresis = 0.97f;

  float probeMaxRayDistance = 1e27f;

  int probeNumRays = 128;

  int probeIrradianceResolution = 6;
  int probeDistanceResolution = 6;
};

struct CameraConstants {
  glm::mat4 view;
  glm::mat4 projection;
};

struct DebugConstants {
  int debugMode;
};

struct DDGIResources {
  DDGIVolumeDesc desc;
  nvrhi::BufferHandle probePositions;
  nvrhi::BufferHandle cameraConstants;
  nvrhi::BufferHandle ddgiConstants;
  nvrhi::BufferHandle debugConstants;
  nvrhi::BufferHandle sceneVertexPositions;
  nvrhi::BufferHandle sceneVertexUVs;
  nvrhi::BufferHandle sceneIndices;
  nvrhi::BufferHandle instanceIndexOffsets;
  nvrhi::BufferHandle instanceMaterialIndices;

  nvrhi::TextureHandle rayProbeRadianceDistance;
  nvrhi::TextureHandle probeGridIrradiance[2];
  nvrhi::TextureHandle probeGridDistance[2];
  nvrhi::TextureHandle probeOffsets;

  nvrhi::BindingLayoutHandle probeVisualizationLayout;
  nvrhi::BindingSetHandle probeVisualizationBindingSet[2];

  nvrhi::BindingLayoutHandle probeRayTracingLayout;
  nvrhi::BindingSetHandle probeRayTracingBindingSet[2];

  nvrhi::BindingLayoutHandle indirectSamplingLayout;
  nvrhi::BindingSetHandle indirectSamplingBindingSet[2];

  nvrhi::BindingLayoutHandle probeIrradianceUpdateLayout;
  nvrhi::BindingSetHandle probeIrradianceUpdateBindingSet[2];
  nvrhi::BindingSetHandle probeVisibilityUpdateBindingSet[2];
  uint32_t currentProbeAtlas = 0;

  nvrhi::ShaderHandle probeVisualizationMeshShader;
  nvrhi::ShaderHandle probeVisualizationPixelShader;
  nvrhi::ShaderHandle probeRayTracingComputeShader;
  nvrhi::ShaderHandle probeIrradianceUpdateComputeShader;
  nvrhi::ShaderHandle probeVisibilityUpdateComputeShader;
  nvrhi::ShaderHandle probeOffsetsComputeShader;
  nvrhi::ShaderHandle probeSamplingComputeShader;

  nvrhi::MeshletPipelineHandle probeVisualizationPipeline;
  nvrhi::ComputePipelineHandle probeRayTracingPipeline;
  nvrhi::ComputePipelineHandle probeIrradianceUpdatePipeline;
};

class DDGIExperiment : public Experiment {
 public:
  void OnInit(nvrhi::IDevice *device,
              const nvrhi::FramebufferInfo &framebufferInfo) override;
  ~DDGIExperiment() = default;

  void OnUpdate(float deltaTime) override;
  void OnInput(float deltaTime, Input &input) override;
  void OnRender(FrameContext &frame) override;
  void OnUiRender(FrameContext &frame) override;
  void OnDestroy() override;
  void OnResize(int width, int height) override;

  std::string_view GetName() const override;

 private:
  void LoadScene();
  void CreateBindings();
  void CreatePipeline(const nvrhi::FramebufferInfo &outputInfo);
  void RenderGeometry(const FrameContext &frame);
  void RenderProbeVisualization(const FrameContext &frame);
  void UpdateProbeIrradiances(const FrameContext &frame);
  void ResetProbeHistory(const FrameContext &frame);

  // DDGI
  void InitializeDDGI(DDGIVolumeDesc desc,
                      const nvrhi::FramebufferInfo &outputInfo);
  void InitializeDDGIResources(DDGIVolumeDesc desc);
  void InitializeDDGIBindings(DDGIVolumeDesc desc);
  void InitDDGIPipeline(const nvrhi::FramebufferInfo &outputInfo);
  DDGIResources m_DDGIResources;
  bool m_ProbePositionsDirty = false;
  bool m_ProbeResourcesDirty = false;
  bool m_ResetProbeHistory = false;

  Camera m_Camera;
  FlyingCameraController m_CameraController;
  AssetRegistry m_AssetRegistry;

  ModelData m_Model;
  std::vector<GpuMesh> m_GpuMeshes;
  std::vector<MeshDrawItem> m_DrawItems;

  nvrhi::GraphicsPipelineHandle m_Pipeline;
  nvrhi::BindingLayoutHandle m_BindingLayout;
  nvrhi::BindingSetHandle m_BindingSet;
  nvrhi::SamplerHandle m_MaterialSampler;
  nvrhi::BufferHandle m_ConstantBuffer;

  struct ConstantBufferData {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 normalMatrix;
    glm::vec3 toLight;
    float lightPadding = 0.0f;
    glm::vec3 cameraPosition;
    uint32_t materialIndex;
  };

  struct alignas(16) DDGIConstants {
    glm::vec3 toLight;
    float randomAngle;

    uint32_t irradiance;  // 0 = irradiance, 1 = visibility
    uint32_t irradianceTextureSize;
    uint32_t irradianceQuadSize;
    float probeHysteresis;
    float probeMaxRayDistance;
    glm::vec3 probeGridOrigin;
    glm::vec3 probeGridSpacing;
    float padding = 0.0f;
    glm::vec3 probeGridSize;
    float gridSizePadding = 0.0f;
    glm::vec4 lightColor;
  };

  struct DDGIUpdateConstants {
    uint32_t irradiance;  // 0 = irradiance, 1 = visibility
    uint32_t textureDimensions;
  };

  struct GBuffer {
    nvrhi::TextureHandle depth;
    nvrhi::TextureHandle normal;
  };

  float m_SunAzimuth = glm::radians(-124.0f);
  float m_SunElevation = glm::radians(70.0f);
  glm::vec3 m_LightColor = {1.0f, 1.0f, 1.0f};
  bool m_UseDDGI = true;
  bool m_ShowIndirectLighting = false;
  bool m_ShowProbes = false;
  glm::vec3 m_ToLight = {0.0f, 1.0f, 0.0f};

  MeshRenderer m_MeshRenderer;
  RayTracingScene m_RayTracingScene;
};
}  // namespace Iskra
