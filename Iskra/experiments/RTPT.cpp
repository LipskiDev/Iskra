#include <experiments/RTPT.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <iterator>
#include <cmath>
#include <imgui.h>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <graphics/ModelTraversal.h>
#include <scene/components/Components.h>

namespace {
std::vector<char> ReadBinary(const char *path) {
  std::ifstream file(path, std::ios::binary);

  if (!file) {
    throw std::runtime_error(std::string("Cannot open shader: ") + path);
  }

  std::vector<char> bytes{std::istreambuf_iterator<char>(file),
                          std::istreambuf_iterator<char>()};

  if (bytes.empty()) {
    throw std::runtime_error(std::string("Shader is empty: ") + path);
  }

  return bytes;
}

Iskra::RTPT::PathTracingMaterialGpu ConvertToPathTracingMaterial(
    const Iskra::Material &material) {
  return {
      .baseColor = glm::vec3(material.baseColorFactor),
      .metallic = material.metallicFactor,
      .emission = material.emissiveFactor,
      .roughness = material.roughnessFactor,
      .transmission = material.transmissionFactor,
      .ior = material.indexOfRefraction,
  };
}

}  // namespace

namespace Iskra::RTPT {
void RTPTExperiment::OnInit(nvrhi::IDevice *device,
                            const nvrhi::FramebufferInfo &framebufferInfo) {
  m_Device = device;
  if (!device->queryFeatureSupport(nvrhi::Feature::RayTracingPipeline)) {
    throw std::runtime_error("Ray tracing pipelines are unsupported");
  }

  m_AssetRegistry.Initialize(m_Device);

  m_UniformBuffer = device->createBuffer(
      nvrhi::BufferDesc()
          .setByteSize(sizeof(UniformBuffer))
          .setStructStride(sizeof(UniformBuffer))
          .setInitialState(nvrhi::ResourceStates::ShaderResource)
          .setKeepInitialState(true)
          .setDebugName("Uniform Buffer")
          .setIsConstantBuffer(true));

  if (!m_UniformBuffer) {
    throw std::runtime_error("Failed to create uniform buffer");
  }

  const auto shaderBytes = ReadBinary(ISKRA_SHADER_DIR "/rtpt.lib.dxil");

  auto library =
      device->createShaderLibrary(shaderBytes.data(), shaderBytes.size());
  if (!library) {
    throw std::runtime_error("Failed to create shader library");
  }

  auto rayGen = library->getShader("RayGen", nvrhi::ShaderType::RayGeneration);
  auto miss = library->getShader("Miss", nvrhi::ShaderType::Miss);
  auto intersection =
      library->getShader("SphereIntersection", nvrhi::ShaderType::Intersection);
  auto closestHitSphere =
      library->getShader("SphereClosestHit", nvrhi::ShaderType::ClosestHit);
  auto closestHitTriangle =
      library->getShader("TriangleClosestHit", nvrhi::ShaderType::ClosestHit);

  if (!rayGen || !miss || !intersection || !closestHitSphere ||
      !closestHitTriangle) {
    throw std::runtime_error("Failed to get shaders from library");
  }

  nvrhi::BindingLayoutDesc bindingLayoutDesc{};
  bindingLayoutDesc.visibility = nvrhi::ShaderType::AllRayTracing;
  bindingLayoutDesc.bindings = {
      nvrhi::BindingLayoutItem::RayTracingAccelStruct(0),
      nvrhi::BindingLayoutItem::StructuredBuffer_SRV(1),
      nvrhi::BindingLayoutItem::StructuredBuffer_SRV(2),
      nvrhi::BindingLayoutItem::StructuredBuffer_SRV(3),
      nvrhi::BindingLayoutItem::StructuredBuffer_SRV(4),
      nvrhi::BindingLayoutItem::StructuredBuffer_SRV(5),
      nvrhi::BindingLayoutItem::Texture_UAV(0),
      nvrhi::BindingLayoutItem::Texture_UAV(1),
      nvrhi::BindingLayoutItem::ConstantBuffer(0)};
  m_RayTracingBindingLayout = device->createBindingLayout(bindingLayoutDesc);
  if (!m_RayTracingBindingLayout) {
    throw std::runtime_error("Failed to create ray tracing binding layout");
  }

  nvrhi::rt::PipelineDesc desc{};
  desc.addBindingLayout(m_RayTracingBindingLayout);

  desc.addShader(
      nvrhi::rt::PipelineShaderDesc().setExportName("RayGen").setShader(
          rayGen));
  desc.addShader(
      nvrhi::rt::PipelineShaderDesc().setExportName("Miss").setShader(miss));

  desc.addHitGroup(nvrhi::rt::PipelineHitGroupDesc()
                       .setExportName("TriangleHitGroup")
                       .setClosestHitShader(closestHitTriangle));
  desc.addHitGroup(nvrhi::rt::PipelineHitGroupDesc()
                       .setExportName("SphereHitGroup")
                       .setClosestHitShader(closestHitSphere)
                       .setIntersectionShader(intersection)
                       .setIsProceduralPrimitive(true));

  desc.maxPayloadSize = 4 * 3 * sizeof(float) + 3 * sizeof(uint32_t);
  desc.maxAttributeSize = 3 * sizeof(float);
  desc.maxRecursionDepth = 1;

  m_Pipeline = device->createRayTracingPipeline(desc);
  if (!m_Pipeline) {
    throw std::runtime_error("Failed to create ray tracing pipeline");
  }

  m_ShaderTable = m_Pipeline->createShaderTable();
  if (!m_ShaderTable) {
    throw std::runtime_error("Failed to create ray tracing shader table");
  }
  m_ShaderTable->setRayGenerationShader("RayGen");
  m_ShaderTable->addMissShader("Miss");
  m_ShaderTable->addHitGroup("TriangleHitGroup");
  m_ShaderTable->addHitGroup("SphereHitGroup");

  CreateSceneEntities();
  ExtractScene();
  UploadScene();
  BuildAccelerationStructures();
}

void RTPTExperiment::CreateOutput(uint32_t width, uint32_t height,
                                  nvrhi::Format format) {
  nvrhi::TextureDesc desc{};
  desc.width = width;
  desc.height = height;
  desc.format = format;
  desc.isUAV = true;
  desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
  desc.keepInitialState = true;
  desc.debugName = "RTPT Output";
  auto output = m_Device->createTexture(desc);
  if (!output) {
    throw std::runtime_error("Failed to create ray tracing output texture");
  }

  nvrhi::TextureDesc accumulationDesc{};
  accumulationDesc.width = width;
  accumulationDesc.height = height;
  accumulationDesc.format = nvrhi::Format::RGBA32_FLOAT;
  accumulationDesc.isUAV = true;
  accumulationDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
  accumulationDesc.keepInitialState = true;
  accumulationDesc.debugName = "RTPT Accumulation";
  m_AccumulationTexture = m_Device->createTexture(accumulationDesc);
  if (!m_AccumulationTexture) {
    throw std::runtime_error(
        "Failed to create ray tracing accumulation texture");
  }

  nvrhi::BindingSetDesc bindings{};
  bindings.bindings = {
      nvrhi::BindingSetItem::RayTracingAccelStruct(0, m_TLAS),
      nvrhi::BindingSetItem::StructuredBuffer_SRV(1, m_SpheresBuffer),
      nvrhi::BindingSetItem::StructuredBuffer_SRV(2, m_VerticesBuffer),
      nvrhi::BindingSetItem::StructuredBuffer_SRV(3, m_IndicesBuffer),
      nvrhi::BindingSetItem::StructuredBuffer_SRV(4, m_MaterialsBuffer),
      nvrhi::BindingSetItem::StructuredBuffer_SRV(5, m_TriangleInstancesBuffer),
      nvrhi::BindingSetItem::Texture_UAV(0, output),
      nvrhi::BindingSetItem::Texture_UAV(1, m_AccumulationTexture),
      nvrhi::BindingSetItem::ConstantBuffer(0, m_UniformBuffer)};
  auto bindingSet =
      m_Device->createBindingSet(bindings, m_RayTracingBindingLayout);
  if (!bindingSet) {
    throw std::runtime_error("Failed to create ray tracing binding set");
  }
  m_Output = output;
  m_RayTracingBindingSet = bindingSet;
}

void RTPTExperiment::CreateSceneEntities() {
  auto addSphere = [&](float x, float y, float z, float radius,
                       const Iskra::Material &material) {
    Entity entity = m_World.CreateEntity();
    const MaterialHandle materialHandle = m_AssetRegistry.AddMaterial(material);

    TransformComponent &transform = entity.AddComponent<TransformComponent>();
    transform.translation = {x, y, z};

    entity.AddComponent<AnalyticalSphereComponent>(AnalyticalSphereComponent{
        .radius = radius, .material = materialHandle});
  };

  unsigned int randomState = 0xA341316Cu;
  auto randomFloat = [&randomState]() -> float {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return static_cast<float>(randomState & 0x00FFFFFFu) /
           static_cast<float>(0x01000000u);
  };
  auto randomFloatRange = [&randomFloat](float minimum,
                                         float maximum) -> float {
    return minimum + (maximum - minimum) * randomFloat();
  };

  addSphere(0.0f, -1000.0f, 0.0f, 1000.0f,
            Iskra::Material{.baseColorFactor = {0.5f, 0.5f, 0.5f, 1.0f},
                            .emissiveFactor = {0.0f, 0.0f, 0.0f},
                            .roughnessFactor = 0.0f,
                            .indexOfRefraction = 1.0f});

  for (int a = -11; a < 11; ++a) {
    for (int b = -11; b < 11; ++b) {
      const float chooseMaterial = randomFloat();
      const float centerX = static_cast<float>(a) + 0.9f * randomFloat();
      const float centerY = 0.2f;
      const float centerZ = static_cast<float>(b) + 0.9f * randomFloat();

      const float deltaX = centerX - 4.0f;
      const float deltaY = centerY - 0.2f;
      const float deltaZ = centerZ;
      const float distanceSquared =
          deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;
      if (distanceSquared <= 0.9f * 0.9f) {
        continue;
      }

      Material sphereMaterial{};
      if (chooseMaterial < 0.8f) {
        sphereMaterial = Iskra::Material{
            .baseColorFactor = {randomFloat() * randomFloat(),
                                randomFloat() * randomFloat(),
                                randomFloat() * randomFloat(), 1.0f},
            .emissiveFactor = {0.0f, 0.0f, 0.0f},
            .roughnessFactor = 0.0f,
            .indexOfRefraction = 1.0f,
        };
      } else if (chooseMaterial < 0.95f) {
        sphereMaterial = Iskra::Material{
            .baseColorFactor = {randomFloatRange(0.5f, 1.0f),
                                randomFloatRange(0.5f, 1.0f),
                                randomFloatRange(0.5f, 1.0f), 1.0f},
            .emissiveFactor = {0.0f, 0.0f, 0.0f},
            .metallicFactor = 1.0f,
            .roughnessFactor = randomFloatRange(0.0f, 0.5f),
            .indexOfRefraction = 1.0f,
        };
      } else {
        sphereMaterial = Iskra::Material{
            .baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f},
            .emissiveFactor = {0.0f, 0.0f, 0.0f},
            .roughnessFactor = 0.0f,
            .transmissionFactor = 1.0f,
            .indexOfRefraction = 1.5f,
        };
      }

      addSphere(centerX, centerY, centerZ, 0.2f, sphereMaterial);
    }
  }

  addSphere(0.0f, 1.0f, 0.0f, 1.0f,
            Iskra::Material{.baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f},
                            .emissiveFactor = {0.0f, 0.0f, 0.0f},
                            .roughnessFactor = 0.0f,
                            .transmissionFactor = 1.0f,
                            .indexOfRefraction = 1.5f});

  addSphere(-4.0f, 1.0f, 0.0f, 1.0f,
            Iskra::Material{.baseColorFactor = {0.4f, 0.2f, 0.1f, 1.0f},
                            .emissiveFactor = {0.0f, 0.0f, 0.0f},
                            .roughnessFactor = 0.0f,
                            .indexOfRefraction = 1.0f});

  addSphere(4.0f, 1.0f, 0.0f, 1.0f,
            Iskra::Material{.baseColorFactor = {0.7f, 0.6f, 0.5f, 1.0f},
                            .emissiveFactor = {0.0f, 0.0f, 0.0f},
                            .metallicFactor = 1.0f,
                            .roughnessFactor = 0.0f,
                            .indexOfRefraction = 1.0f});

  const ModelHandle cornellBox =
      m_AssetRegistry.LoadModel("assets/models/cornell_box.glb");

  const ModelHandle stanfordDragon =
      m_AssetRegistry.LoadModel("assets/models/stanford_dragon.glb");

  m_AssetRegistry.UpdateResourcesIfDirty();

  const glm::mat4 cornellTransform =
      glm::translate(glm::mat4(1.0f), glm::vec3(-16.0f, 0.0f, -16.0f));

  const glm::mat4 dragonTransform =
      glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 5.0f)) *
      glm::scale(glm::mat4(1.0f), glm::vec3(100.0f));

  Entity cornellBoxEntity = m_World.CreateEntity("Cornell Box");
  TransformComponent &cornellBoxTransform =
      cornellBoxEntity.AddComponent<TransformComponent>();
  cornellBoxTransform.localTransform = cornellTransform;
  cornellBoxTransform.worldTransform = cornellTransform;
  cornellBoxTransform.dirty = false;
  cornellBoxEntity.AddComponent<ModelComponent>(
      ModelComponent{.model = cornellBox});

  Entity dragonEntity = m_World.CreateEntity("Stanford Dragon");
  TransformComponent &dragonModelTransform =
      dragonEntity.AddComponent<TransformComponent>();
  dragonModelTransform.localTransform = dragonTransform;
  dragonModelTransform.worldTransform = dragonTransform;
  dragonModelTransform.dirty = false;
  dragonEntity.AddComponent<ModelComponent>(
      ModelComponent{.model = stanfordDragon});
}

void RTPTExperiment::ExtractScene() {
  spheres.clear();
  vertices.clear();
  indices.clear();
  materials.clear();
  triangleInstances.clear();
  bounds.clear();
  m_RenderInstances.clear();

  const World &world = m_World;

  world.Query<TransformComponent, AnalyticalSphereComponent>().Each(
      [&](Entity entity, const TransformComponent &transform,
          const AnalyticalSphereComponent &sphere) {
        const glm::vec3 center = transform.translation;
        const float radius = sphere.radius;
        const Iskra::Material &material =
            m_AssetRegistry.GetMaterial(sphere.material);

        const uint32_t materialIndex = static_cast<uint32_t>(materials.size());
        materials.push_back(ConvertToPathTracingMaterial(material));

        spheres.push_back({.center = {center.x, center.y, center.z},
                           .radius = radius,
                           .materialIndex = materialIndex});

        bounds.push_back({center.x - radius, center.y - radius,
                          center.z - radius, center.x + radius,
                          center.y + radius, center.z + radius});
      });

  world.Query<TransformComponent, MeshComponent>().Each(
      [&](Entity entity, const TransformComponent &transform,
          const MeshComponent &mesh) {
        m_RenderInstances.push_back({
            .entity = entity.GetId(),
            .mesh = mesh.mesh,
            .material = mesh.material,
            .worldTransform = transform.worldTransform,
            .visibilityMask = mesh.visibilityMask,
        });
      });

  world.Query<TransformComponent, ModelComponent>().Each(
      [&](Entity entity, const TransformComponent &transform,
          const ModelComponent &model) {
        CollectRenderInstances(m_AssetRegistry.GetModel(model.model).root,
                               transform.worldTransform, entity.GetId(),
                               model.visibilityMask, m_RenderInstances);
      });

  for (const RenderInstance &instance : m_RenderInstances) {
    const MeshData &meshData = m_AssetRegistry.GetMesh(instance.mesh);
    const uint32_t vertexOffset = static_cast<uint32_t>(vertices.size());
    const uint32_t indexOffset = static_cast<uint32_t>(indices.size());
    const uint32_t materialIndex = static_cast<uint32_t>(materials.size());

    vertices.reserve(vertices.size() + meshData.vertices.size());
    for (const Vertex &vertex : meshData.vertices) {
      vertices.push_back(
          {.position = vertex.position, .normal = vertex.normal});
    }
    indices.insert(indices.end(), meshData.indices.begin(),
                   meshData.indices.end());

    const Iskra::Material &sourceMaterial =
        m_AssetRegistry.GetMaterial(instance.material);
    materials.push_back(ConvertToPathTracingMaterial(sourceMaterial));

    triangleInstances.push_back({.vertexOffset = vertexOffset,
                                 .indexOffset = indexOffset,
                                 .materialIndex = materialIndex});
  }
}

void RTPTExperiment::UploadScene() {
  if (spheres.size() > MaxSpheres || bounds.size() != spheres.size() ||
      triangleInstances.size() != m_RenderInstances.size() ||
      vertices.empty() || indices.empty() || materials.empty()) {
    throw std::runtime_error("Extracted RTPT scene exceeds buffer capacity");
  }

  m_AabbsBuffer = m_Device->createBuffer(
      nvrhi::BufferDesc()
          .setByteSize(sizeof(nvrhi::rt::GeometryAABB) * bounds.size())
          .setIsAccelStructBuildInput(true)
          .setInitialState(nvrhi::ResourceStates::AccelStructBuildInput)
          .setKeepInitialState(true)
          .setDebugName("RTPT AABBs"));

  auto createStructuredBuffer = [&](size_t byteSize, size_t stride,
                                    const char *debugName) {
    nvrhi::BufferHandle buffer = m_Device->createBuffer(
        nvrhi::BufferDesc()
            .setByteSize(byteSize)
            .setStructStride(stride)
            .setInitialState(nvrhi::ResourceStates::ShaderResource)
            .setKeepInitialState(true)
            .setDebugName(debugName));
    if (!buffer) {
      throw std::runtime_error(std::string("Failed to create ") + debugName);
    }
    return buffer;
  };

  if (!m_AabbsBuffer) {
    throw std::runtime_error("Failed to create RTPT AABBs buffer");
  }
  m_SpheresBuffer = createStructuredBuffer(sizeof(Sphere) * spheres.size(),
                                           sizeof(Sphere), "RTPT Spheres");
  m_VerticesBuffer = createStructuredBuffer(sizeof(RTVertex) * vertices.size(),
                                            sizeof(RTVertex), "RTPT Vertices");
  m_IndicesBuffer = createStructuredBuffer(sizeof(uint32_t) * indices.size(),
                                           sizeof(uint32_t), "RTPT Indices");
  m_MaterialsBuffer =
      createStructuredBuffer(sizeof(PathTracingMaterialGpu) * materials.size(),
                             sizeof(PathTracingMaterialGpu), "RTPT Materials");
  m_TriangleInstancesBuffer = createStructuredBuffer(
      sizeof(TriangleInstanceData) * triangleInstances.size(),
      sizeof(TriangleInstanceData), "RTPT Triangle Instances");

  auto upload = m_Device->createCommandList();
  if (!upload) {
    throw std::runtime_error("Failed to create bounds upload command list");
  }
  upload->open();
  upload->writeBuffer(m_AabbsBuffer, bounds.data(),
                      sizeof(nvrhi::rt::GeometryAABB) * bounds.size());
  upload->writeBuffer(m_SpheresBuffer, spheres.data(),
                      sizeof(Sphere) * spheres.size());
  upload->writeBuffer(m_VerticesBuffer, vertices.data(),
                      sizeof(RTVertex) * vertices.size());
  upload->writeBuffer(m_IndicesBuffer, indices.data(),
                      sizeof(uint32_t) * indices.size());
  upload->writeBuffer(m_MaterialsBuffer, materials.data(),
                      sizeof(PathTracingMaterialGpu) * materials.size());
  upload->writeBuffer(m_TriangleInstancesBuffer, triangleInstances.data(),
                      sizeof(TriangleInstanceData) * triangleInstances.size());
  upload->close();
  m_Device->executeCommandList(upload);
}

void RTPTExperiment::BuildAccelerationStructures() {
  nvrhi::rt::GeometryAABBs aabbs;
  aabbs.setBuffer(m_AabbsBuffer);
  aabbs.setCount(static_cast<uint32_t>(spheres.size()));
  aabbs.setStride(sizeof(nvrhi::rt::GeometryAABB));

  nvrhi::rt::GeometryDesc geometry;
  geometry.setAABBs(aabbs);
  geometry.setFlags(nvrhi::rt::GeometryFlags::Opaque);

  nvrhi::rt::AccelStructDesc blasDesc{};
  blasDesc.bottomLevelGeometries = {geometry};
  blasDesc.buildFlags = nvrhi::rt::AccelStructBuildFlags::PreferFastTrace;
  blasDesc.debugName = "Sphere BLAS";
  m_SphereBLAS = m_Device->createAccelStruct(blasDesc);
  if (!m_SphereBLAS) {
    throw std::runtime_error("Failed to create sphere BLAS");
  }

  m_TriangleBLASes.clear();
  m_TriangleBLASes.reserve(m_AssetRegistry.GetMeshCount());
  std::vector<nvrhi::rt::GeometryDesc> triangleGeometries;
  triangleGeometries.reserve(m_AssetRegistry.GetMeshCount());

  for (uint32_t meshIndex = 0; meshIndex < m_AssetRegistry.GetMeshCount();
       ++meshIndex) {
    const GpuMesh &mesh = m_AssetRegistry.GetGpuMesh(MeshHandle(meshIndex));

    nvrhi::rt::GeometryTriangles triangles{};
    triangles.vertexBuffer = mesh.vertexBuffer;
    triangles.vertexFormat = nvrhi::Format::RGB32_FLOAT;
    triangles.vertexOffset = offsetof(Vertex, position);
    triangles.vertexStride = sizeof(Vertex);
    triangles.vertexCount = mesh.vertexCount;
    triangles.indexBuffer = mesh.indexBuffer;
    triangles.indexFormat = nvrhi::Format::R32_UINT;
    triangles.indexCount = mesh.indexCount;

    nvrhi::rt::GeometryDesc triangleGeometry{};
    triangleGeometry.setTriangles(triangles);
    triangleGeometry.flags = nvrhi::rt::GeometryFlags::Opaque;
    triangleGeometries.push_back(triangleGeometry);

    nvrhi::rt::AccelStructDesc triangleBLASDesc{};
    triangleBLASDesc.bottomLevelGeometries = {triangleGeometry};
    triangleBLASDesc.buildFlags =
        nvrhi::rt::AccelStructBuildFlags::PreferFastTrace;
    triangleBLASDesc.debugName = "RTPT Triangle BLAS";
    auto triangleBLAS = m_Device->createAccelStruct(triangleBLASDesc);
    if (!triangleBLAS) {
      throw std::runtime_error("Failed to create triangle BLAS");
    }
    m_TriangleBLASes.push_back(triangleBLAS);
  }

  std::vector<nvrhi::rt::InstanceDesc> instances;
  instances.reserve(m_RenderInstances.size() + 1);
  for (uint32_t instanceIndex = 0; instanceIndex < m_RenderInstances.size();
       ++instanceIndex) {
    const RenderInstance &source = m_RenderInstances[instanceIndex];
    nvrhi::rt::InstanceDesc triangleInstance{};
    for (int row = 0; row < 3; ++row) {
      for (int column = 0; column < 4; ++column) {
        triangleInstance.transform[row * 4 + column] =
            source.worldTransform[column][row];
      }
    }
    triangleInstance.setBLAS(m_TriangleBLASes.at(source.mesh.GetIndex()))
        .setInstanceID(instanceIndex)
        .setInstanceMask(static_cast<uint8_t>(source.visibilityMask))
        .setInstanceContributionToHitGroupIndex(0);
    instances.push_back(triangleInstance);
  }

  nvrhi::rt::InstanceDesc sphereInstance{};
  sphereInstance.setBLAS(m_SphereBLAS)
      .setInstanceMask(0xff)
      .setInstanceContributionToHitGroupIndex(1);
  instances.push_back(sphereInstance);

  nvrhi::rt::AccelStructDesc tlasDesc{};
  tlasDesc.isTopLevel = true;
  tlasDesc.topLevelMaxInstances = instances.size();
  tlasDesc.buildFlags = nvrhi::rt::AccelStructBuildFlags::PreferFastTrace;
  tlasDesc.debugName = "RTPT Mixed TLAS";
  m_TLAS = m_Device->createAccelStruct(tlasDesc);
  if (!m_TLAS) {
    throw std::runtime_error("Failed to create sphere TLAS");
  }

  auto build = m_Device->createCommandList();
  if (!build) {
    throw std::runtime_error(
        "Failed to create acceleration structure command list");
  }
  build->open();
  build->buildBottomLevelAccelStruct(m_SphereBLAS, &geometry, 1,
                                     blasDesc.buildFlags);
  for (uint32_t meshIndex = 0; meshIndex < m_TriangleBLASes.size();
       ++meshIndex) {
    build->buildBottomLevelAccelStruct(
        m_TriangleBLASes[meshIndex], &triangleGeometries[meshIndex], 1,
        nvrhi::rt::AccelStructBuildFlags::PreferFastTrace);
  }
  build->buildTopLevelAccelStruct(m_TLAS, instances.data(), instances.size(),
                                  tlasDesc.buildFlags);
  build->close();
  m_Device->executeCommandList(build);
  if (!m_Device->waitForIdle()) {
    throw std::runtime_error("Failed to build RTPT acceleration structures");
  }
}

void RTPTExperiment::OnUpdate(float deltaTime) {}

void RTPTExperiment::OnInput(float deltaTime, Input &input) {
  const auto [mouseX, mouseY] = input.GetMousePosition();
  const glm::vec2 mousePosition(mouseX, mouseY);
  const bool looking = input.IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
  bool cameraChanged = false;

  if (looking && m_WasLooking) {
    const glm::vec2 delta = mousePosition - m_LastMousePosition;
    if (glm::dot(delta, delta) > 0.0f) {
      constexpr float mouseSensitivity = 0.005f;
      m_CameraYaw = std::remainder(m_CameraYaw + delta.x * mouseSensitivity,
                                   glm::two_pi<float>());
      m_CameraPitch = glm::clamp(m_CameraPitch - delta.y * mouseSensitivity,
                                 -glm::radians(89.0f), glm::radians(89.0f));
      cameraChanged = true;
    }
  }

  m_LastMousePosition = mousePosition;
  m_WasLooking = looking;

  m_CameraDirection = glm::vec3(
      std::sin(m_CameraYaw) * std::cos(m_CameraPitch), std::sin(m_CameraPitch),
      -std::cos(m_CameraYaw) * std::cos(m_CameraPitch));

  const glm::vec3 right(std::cos(m_CameraYaw), 0.0f, std::sin(m_CameraYaw));
  constexpr glm::vec3 worldUp(0.0f, 1.0f, 0.0f);

  glm::vec3 movement(0.0f);
  if (input.IsKeyPressed(GLFW_KEY_W))
    movement += m_CameraDirection;
  if (input.IsKeyPressed(GLFW_KEY_S))
    movement -= m_CameraDirection;
  if (input.IsKeyPressed(GLFW_KEY_D))
    movement += right;
  if (input.IsKeyPressed(GLFW_KEY_A))
    movement -= right;
  if (input.IsKeyPressed(GLFW_KEY_E))
    movement += worldUp;
  if (input.IsKeyPressed(GLFW_KEY_Q))
    movement -= worldUp;

  const float movementLengthSquared = glm::dot(movement, movement);
  if (movementLengthSquared > 0.0f && deltaTime > 0.0f) {
    constexpr float movementSpeed = 3.0f;
    const bool fast = input.IsKeyPressed(GLFW_KEY_LEFT_SHIFT) ||
                      input.IsKeyPressed(GLFW_KEY_RIGHT_SHIFT);
    const float speed = movementSpeed * (fast ? 4.0f : 1.0f);
    m_CameraPosition +=
        movement / std::sqrt(movementLengthSquared) * speed * deltaTime;
    cameraChanged = true;
  }

  if (cameraChanged) {
    m_Frame = 0;
  }
}

void RTPTExperiment::OnRender(FrameContext &frame) {
  if (m_AssetRegistry.materialsDirty_) {
    m_AssetRegistry.materialsDirty_ = false;
    m_Frame = 0;
    frame.commandList->writeBuffer(
        m_MaterialsBuffer, materials.data(),
        sizeof(PathTracingMaterialGpu) * materials.size());
  }
  m_AssetRegistry.UpdateResourcesIfDirty();

  UniformBuffer uniformData{};
  uniformData.frame = m_Frame;
  uniformData.seed = m_Seed;
  uniformData.cameraPosition[0] = m_CameraPosition.x;
  uniformData.cameraPosition[1] = m_CameraPosition.y;
  uniformData.cameraPosition[2] = m_CameraPosition.z;
  uniformData.tanHalfFovY = std::tan(glm::radians(45.0f));
  uniformData.cameraDirection[0] = m_CameraDirection.x;
  uniformData.cameraDirection[1] = m_CameraDirection.y;
  uniformData.cameraDirection[2] = m_CameraDirection.z;
  frame.commandList->writeBuffer(m_UniformBuffer, &uniformData,
                                 sizeof(uniformData));

  const auto &backbuffer = frame.backbuffer->getDesc();
  if (!m_Output || m_Output->getDesc().width != backbuffer.width ||
      m_Output->getDesc().height != backbuffer.height ||
      m_Output->getDesc().format != backbuffer.format) {
    CreateOutput(backbuffer.width, backbuffer.height, backbuffer.format);
  }

  nvrhi::rt::State state{};
  state.setShaderTable(m_ShaderTable).addBindingSet(m_RayTracingBindingSet);
  frame.commandList->setRayTracingState(state);
  frame.commandList->dispatchRays(
      nvrhi::rt::DispatchRaysArguments().setDimensions(backbuffer.width,
                                                       backbuffer.height));
  frame.commandList->copyTexture(frame.backbuffer, nvrhi::TextureSlice(),
                                 m_Output, nvrhi::TextureSlice());

  m_Frame++;
}

void RTPTExperiment::OnUiRender(FrameContext &frame) {
  ImGui::Begin("RTPT Experiment");
  ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
  ImGui::SliderInt("Seed", &m_Seed, 0, 10);

  if (ImGui::Button("Reset Accumulation")) {
    m_Frame = 0;
  }

  ImGui::Text("Material Editor");
  const std::span<const Iskra::Material> materials =
      m_AssetRegistry.GetMaterials();

  for (uint32_t index = 0; index < materials.size(); ++index) {
    const MaterialHandle handle(index);
    Material edited = materials[index];
    bool changed = false;

    changed |= ImGui::ColorEdit4("Base color", &edited.baseColorFactor.x);

    changed |=
        ImGui::SliderFloat("Metallic", &edited.metallicFactor, 0.0f, 1.0f);

    changed |=
        ImGui::SliderFloat("Roughness", &edited.roughnessFactor, 0.0f, 1.0f);

    changed |= ImGui::SliderFloat("Transmission", &edited.transmissionFactor,
                                  0.0f, 1.0f);

    changed |=
        ImGui::DragFloat("IOR", &edited.indexOfRefraction, 0.01f, 1.0f, 3.0f);

    changed |= ImGui::ColorEdit3("Emission", &edited.emissiveFactor.x);

    if (changed) {
      m_AssetRegistry.UpdateMaterial(handle, edited);
      m_AssetRegistry.materialsDirty_ = true;
      m_Frame = 0;
    }
  }
  ImGui::End();
}

void RTPTExperiment::OnDestroy() {
  m_AssetRegistry.Shutdown();
  m_RayTracingBindingSet = nullptr;
  m_Output = nullptr;
  m_ShaderTable = nullptr;
  m_Pipeline = nullptr;
  m_RayTracingBindingLayout = nullptr;
  m_TLAS = nullptr;
  m_TriangleBLASes.clear();
  m_TriangleInstancesBuffer = nullptr;
  m_MaterialsBuffer = nullptr;
  m_IndicesBuffer = nullptr;
  m_VerticesBuffer = nullptr;
  m_SphereBLAS = nullptr;
  m_SpheresBuffer = nullptr;
  m_AabbsBuffer = nullptr;
}

void RTPTExperiment::OnResize(int width, int height) {
  m_RayTracingBindingSet = nullptr;
  m_Output = nullptr;
  m_Frame = 0;
}

std::string_view RTPTExperiment::GetName() const {
  return "Real-Time Path Tracing";
}
}  // namespace Iskra::RTPT
