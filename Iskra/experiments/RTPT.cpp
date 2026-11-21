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
}  // namespace

namespace Iskra::RTPT {
void RTPTExperiment::OnInit(nvrhi::IDevice *device,
                            const nvrhi::FramebufferInfo &framebufferInfo) {
  m_Device = device;
  if (!device->queryFeatureSupport(nvrhi::Feature::RayTracingPipeline)) {
    throw std::runtime_error("Ray tracing pipelines are unsupported");
  }
  struct Sphere {
    float center[3];
    float radius;
  };
  static_assert(sizeof(Sphere) == 4 * sizeof(float));

  constexpr size_t maxSpheres = 488;

  std::vector<Sphere> spheres;
  std::vector<Material> materials;
  spheres.reserve(maxSpheres);
  materials.reserve(maxSpheres);

  // Deterministic, fast random-number generator.
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

  auto addSphere = [&](float x, float y, float z, float radius,
                       const Material &material) {
    spheres.push_back({{x, y, z}, radius});
    materials.push_back(material);
  };

  // Ground
  Material groundMaterial = {.albedo = {0.5f, 0.5f, 0.5f},
                             .type = MaterialType::Lambertian,
                             .emission = {0.f, 0.f, 0.f},
                             .roughness = 0.f,
                             .ior = 1.f};

  addSphere(0.f, -1000.f, 0.f, 1000.f, groundMaterial);

  // Random small spheres
  for (int a = -11; a < 11; ++a) {
    for (int b = -11; b < 11; ++b) {
      const float chooseMaterial = randomFloat();

      const float centerX = static_cast<float>(a) + 0.9f * randomFloat();

      const float centerY = 0.2f;

      const float centerZ = static_cast<float>(b) + 0.9f * randomFloat();

      const float deltaX = centerX - 4.f;
      const float deltaY = centerY - 0.2f;
      const float deltaZ = centerZ;

      const float distanceSquared =
          deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;

      // Equivalent to:
      // (center - point3(4, 0.2, 0)).length() > 0.9
      if (distanceSquared <= 0.9f * 0.9f)
        continue;

      Material sphereMaterial = {};

      if (chooseMaterial < 0.8f) {
        // Diffuse: color::random() * color::random()
        sphereMaterial = {.albedo = {randomFloat() * randomFloat(),
                                     randomFloat() * randomFloat(),
                                     randomFloat() * randomFloat()},
                          .type = MaterialType::Lambertian,
                          .emission = {0.f, 0.f, 0.f},
                          .roughness = 0.f,
                          .ior = 1.f};
      } else if (chooseMaterial < 0.95f) {
        // Metal
        sphereMaterial = {
            .albedo = {randomFloatRange(0.5f, 1.f), randomFloatRange(0.5f, 1.f),
                       randomFloatRange(0.5f, 1.f)},
            .type = MaterialType::Metal,
            .emission = {0.f, 0.f, 0.f},
            .roughness = randomFloatRange(0.f, 0.5f),
            .ior = 1.f};
      } else {
        // Glass
        sphereMaterial = {.albedo = {1.f, 1.f, 1.f},
                          .type = MaterialType::Dielectric,
                          .emission = {0.f, 0.f, 0.f},
                          .roughness = 0.f,
                          .ior = 1.5f};
      }

      addSphere(centerX, centerY, centerZ, 0.2f, sphereMaterial);
    }
  }

  // Large glass sphere
  Material material1 = {.albedo = {1.f, 1.f, 1.f},
                        .type = MaterialType::Dielectric,
                        .emission = {0.f, 0.f, 0.f},
                        .roughness = 0.f,
                        .ior = 1.5f};

  addSphere(0.f, 1.f, 0.f, 1.f, material1);

  // Large brown diffuse sphere
  Material material2 = {.albedo = {0.4f, 0.2f, 0.1f},
                        .type = MaterialType::Lambertian,
                        .emission = {0.f, 0.f, 0.f},
                        .roughness = 0.f,
                        .ior = 1.f};

  addSphere(-4.f, 1.f, 0.f, 1.f, material2);

  // Large polished metal sphere
  Material material3 = {.albedo = {0.7f, 0.6f, 0.5f},
                        .type = MaterialType::Metal,
                        .emission = {0.f, 0.f, 0.f},
                        .roughness = 0.f,
                        .ior = 1.f};

  addSphere(4.f, 1.f, 0.f, 1.f, material3);

  const uint32_t amountSpheres = static_cast<uint32_t>(spheres.size());
  std::vector<nvrhi::rt::GeometryAABB> bounds(amountSpheres);

  for (uint32_t i = 0; i < amountSpheres; ++i) {
    const Sphere &sphere = spheres[i];
    bounds[i].minX = sphere.center[0] - sphere.radius;
    bounds[i].minY = sphere.center[1] - sphere.radius;
    bounds[i].minZ = sphere.center[2] - sphere.radius;
    bounds[i].maxX = sphere.center[0] + sphere.radius;
    bounds[i].maxY = sphere.center[1] + sphere.radius;
    bounds[i].maxZ = sphere.center[2] + sphere.radius;
  }

  m_AabbsBuffer = device->createBuffer(
      nvrhi::BufferDesc()
          .setByteSize(sizeof(nvrhi::rt::GeometryAABB) * amountSpheres)
          .setIsAccelStructBuildInput(true)
          .setInitialState(nvrhi::ResourceStates::AccelStructBuildInput)
          .setKeepInitialState(true)
          .setDebugName("AABBs Buffer"));

  if (!m_AabbsBuffer) {
    throw std::runtime_error("Failed to create AABBs buffer");
  }

  m_SpheresBuffer = device->createBuffer(
      nvrhi::BufferDesc()
          .setByteSize(sizeof(Sphere) * amountSpheres)
          .setStructStride(sizeof(Sphere))
          .setInitialState(nvrhi::ResourceStates::ShaderResource)
          .setKeepInitialState(true)
          .setDebugName("Spheres Buffer"));

  if (!m_SpheresBuffer) {
    throw std::runtime_error("Failed to create spheres buffer");
  }

  m_MaterialsBuffer = device->createBuffer(
      nvrhi::BufferDesc()
          .setByteSize(sizeof(Material) * amountSpheres)
          .setStructStride(sizeof(Material))
          .setInitialState(nvrhi::ResourceStates::ShaderResource)
          .setKeepInitialState(true)
          .setDebugName("Materials Buffer"));

  if (!m_MaterialsBuffer) {
    throw std::runtime_error("Failed to create materials buffer");
  }

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

  auto upload = device->createCommandList();
  if (!upload) {
    throw std::runtime_error("Failed to create bounds upload command list");
  }
  upload->open();
  upload->writeBuffer(m_AabbsBuffer, bounds.data(),
                      sizeof(nvrhi::rt::GeometryAABB) * amountSpheres);
  upload->writeBuffer(m_SpheresBuffer, spheres.data(),
                      sizeof(Sphere) * amountSpheres);
  upload->writeBuffer(m_MaterialsBuffer, materials.data(),
                      sizeof(Material) * amountSpheres);
  upload->close();
  device->executeCommandList(upload);

  nvrhi::rt::GeometryAABBs aabbs;
  aabbs.setBuffer(m_AabbsBuffer);
  aabbs.setCount(amountSpheres);
  aabbs.setStride(sizeof(nvrhi::rt::GeometryAABB));

  nvrhi::rt::GeometryDesc geometry;
  geometry.setAABBs(aabbs);
  geometry.setFlags(nvrhi::rt::GeometryFlags::Opaque);

  nvrhi::rt::AccelStructDesc blasDesc{};
  blasDesc.bottomLevelGeometries = {geometry};
  blasDesc.buildFlags = nvrhi::rt::AccelStructBuildFlags::PreferFastTrace;
  blasDesc.debugName = "Sphere BLAS";
  m_BLAS = device->createAccelStruct(blasDesc);
  if (!m_BLAS) {
    throw std::runtime_error("Failed to create sphere BLAS");
  }

  nvrhi::rt::AccelStructDesc tlasDesc{};
  tlasDesc.isTopLevel = true;
  tlasDesc.topLevelMaxInstances = 1;
  tlasDesc.buildFlags = nvrhi::rt::AccelStructBuildFlags::PreferFastTrace;
  tlasDesc.debugName = "Sphere TLAS";
  m_TLAS = device->createAccelStruct(tlasDesc);
  if (!m_TLAS) {
    throw std::runtime_error("Failed to create sphere TLAS");
  }

  nvrhi::rt::InstanceDesc instance{};
  instance.setBLAS(m_BLAS)
      .setInstanceMask(0xff)
      .setInstanceContributionToHitGroupIndex(0);

  auto build = device->createCommandList();
  if (!build) {
    throw std::runtime_error(
        "Failed to create acceleration structure command list");
  }
  build->open();
  build->buildBottomLevelAccelStruct(m_BLAS, &geometry, 1, blasDesc.buildFlags);
  build->buildTopLevelAccelStruct(m_TLAS, &instance, 1, tlasDesc.buildFlags);
  build->close();
  device->executeCommandList(build);

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

  if (!rayGen || !miss || !intersection || !closestHitSphere) {
    throw std::runtime_error("Failed to get shaders from library");
  }

  nvrhi::BindingLayoutDesc bindingLayoutDesc{};
  bindingLayoutDesc.visibility = nvrhi::ShaderType::AllRayTracing;
  bindingLayoutDesc.bindings = {
      nvrhi::BindingLayoutItem::RayTracingAccelStruct(0),
      nvrhi::BindingLayoutItem::StructuredBuffer_SRV(1),
      nvrhi::BindingLayoutItem::StructuredBuffer_SRV(2),
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
  m_ShaderTable->addHitGroup("SphereHitGroup");
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
      nvrhi::BindingSetItem::StructuredBuffer_SRV(2, m_MaterialsBuffer),
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

  ImGui::End();
}

void RTPTExperiment::OnDestroy() {
  m_RayTracingBindingSet = nullptr;
  m_Output = nullptr;
  m_ShaderTable = nullptr;
  m_Pipeline = nullptr;
  m_RayTracingBindingLayout = nullptr;
  m_TLAS = nullptr;
  m_MaterialsBuffer = nullptr;
  m_BLAS = nullptr;
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
