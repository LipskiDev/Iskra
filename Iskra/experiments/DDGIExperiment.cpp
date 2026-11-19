#include <experiments/DDGIExperiment.h>

#include <graphics/MeshUploader.h>
#include <graphics/ModelTraversal.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <glm/ext/scalar_constants.hpp>

namespace {

  std::vector<glm::vec3> BuildProbePositions(const Iskra::DDGIVolumeDesc& desc)
  {
    glm::vec3 volumeCenter = desc.center;
    glm::vec3 volumeExtents = desc.halfSize;
    std::vector<glm::vec3> probePositions;

    for (int x = 0; x < desc.probeCount.x; ++x) {
      for (int y = 0; y < desc.probeCount.y; ++y) {
        for (int z = 0; z < desc.probeCount.z; ++z) {
          glm::vec3 probePos = volumeCenter + glm::vec3(
            (x + 0.5f) / desc.probeCount.x * 2.0f * volumeExtents.x - volumeExtents.x,
            (y + 0.5f) / desc.probeCount.y * 2.0f * volumeExtents.y - volumeExtents.y,
            (z + 0.5f) / desc.probeCount.z * 2.0f * volumeExtents.z - volumeExtents.z);
          probePositions.push_back(probePos);
        }
      }
    }

    return probePositions;
  }

  std::vector<char> ReadBinary(const char* path)
  {
    std::ifstream file(path, std::ios::binary);

    if (!file) {
      throw std::runtime_error(
        std::string("Cannot open shader: ") + path);
    }

    std::vector<char> bytes{
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    };

    if (bytes.empty()) {
      throw std::runtime_error(
        std::string("Shader is empty: ") + path);
    }

    return bytes;
  }

}

namespace Iskra {
  void DDGIExperiment::OnInit(nvrhi::IDevice* device, const nvrhi::FramebufferInfo& framebufferInfo)
  {
    m_Device = device;
    m_Camera = Camera(0.1f, 100.0f, 45.0f, 16.0f / 9.0f);
    m_Camera.SetPosition(glm::vec3(0.0f, 0.0f, 5.0f));
    m_Camera.SetOrientation(glm::vec3(0.0, 0.0f, -1.0f));
    m_CameraController.SetCamera(m_Camera);

    m_AssetRegistry.Initialize(m_Device, nullptr);
    LoadScene();
    m_AssetRegistry.UpdateResourcesIfDirty();

    nvrhi::SamplerDesc samplerDesc;
    samplerDesc.setAllFilters(true);
    samplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Wrap);
    m_MaterialSampler = m_Device->createSampler(samplerDesc);
    if (!m_MaterialSampler) {
      throw std::runtime_error("Failed to create material sampler");
    }

    InitializeDDGI(DDGIVolumeDesc{
        "DDGI Volume",
        glm::vec3(0.0f, 5.0f, 0.0f),
        glm::vec3(10.0f, 8.0f, 10.0f),
        true,
        glm::vec3(8, 8, 8),
        0.97f,
        1e27f,
        128,
        6
      }, framebufferInfo);

    CreateBindings();
    CreatePipeline(framebufferInfo);

  }

  void DDGIExperiment::LoadScene()
  {
    m_Model = m_AssetRegistry.LoadModel("assets/models/sponza/Sponza.gltf");

    m_DrawItems.clear();
    CollectDrawItems(m_Model.root, glm::mat4(1.0f), m_DrawItems);

    auto upload = m_Device->createCommandList();
    upload->open();

    for (const MeshData& mesh : m_Model.meshes) {
      m_GpuMeshes.push_back(UploadMesh(m_Device, upload.Get(), mesh));
    }

    upload->close();
    m_Device->executeCommandList(upload);

    if (!m_Device->waitForIdle()) {
      throw std::runtime_error("Failed to wait for GPU");
    }

    auto build = m_Device->createCommandList();
    build->open();

    m_RayTracingScene.Build(m_Device, build.Get(), m_Model, m_GpuMeshes);
    build->close();
    m_Device->executeCommandList(build);

    if (!m_Device->waitForIdle()) {
      throw std::runtime_error("Failed to wait for GPU");
    }
  }

  void DDGIExperiment::CreateBindings()
  {
    nvrhi::BufferDesc constantBufferDesc{};
    constantBufferDesc.byteSize = sizeof(ConstantBufferData);
    constantBufferDesc.debugName = "Constant Buffer";
    constantBufferDesc.isConstantBuffer = true;
    constantBufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
    constantBufferDesc.keepInitialState = true;
    constantBufferDesc.isVolatile = true;

    m_ConstantBuffer = m_Device->createBuffer(constantBufferDesc);

    if (!m_ConstantBuffer) {
      throw std::runtime_error("Failed to create mesh constant buffer");
    }
    
    nvrhi::BufferDesc debugConstantsBufferDesc{};
    debugConstantsBufferDesc.byteSize = sizeof(DebugConstants);
    debugConstantsBufferDesc.debugName = "Debug Constants Buffer";
    debugConstantsBufferDesc.isConstantBuffer = true;
    debugConstantsBufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
    debugConstantsBufferDesc.keepInitialState = true;
    debugConstantsBufferDesc.isVolatile = true;

    m_DDGIResources.debugConstants = m_Device->createBuffer(debugConstantsBufferDesc);

    if (m_DDGIResources.debugConstants == nullptr)
    {
      throw std::runtime_error("Failed to create debug constants buffer");
    }

    nvrhi::BindingLayoutDesc indirectSamplingLayoutDesc{};
    indirectSamplingLayoutDesc.visibility = nvrhi::ShaderType::Pixel;
    indirectSamplingLayoutDesc.bindings = {
       nvrhi::BindingLayoutItem::Texture_UAV(0),
       nvrhi::BindingLayoutItem::Texture_UAV(1),
    };

    m_DDGIResources.indirectSamplingLayout = m_Device->createBindingLayout(indirectSamplingLayoutDesc);

    if (m_DDGIResources.indirectSamplingLayout == nullptr)
    {
      throw std::runtime_error("Failed to create indirect sampling binding layout");
    }

    nvrhi::BindingSetDesc indirectSamplingBindingSetDesc;
    indirectSamplingBindingSetDesc.bindings = {
      { nvrhi::BindingSetItem::Texture_UAV(0, m_DDGIResources.probeGridIrradiance[0]) },
      { nvrhi::BindingSetItem::Texture_UAV(1, m_DDGIResources.probeGridDistance[0]) }
    };

    m_DDGIResources.indirectSamplingBindingSet[0] = m_Device->createBindingSet(indirectSamplingBindingSetDesc, m_DDGIResources.indirectSamplingLayout);

    indirectSamplingBindingSetDesc.bindings = {
      { nvrhi::BindingSetItem::Texture_UAV(0, m_DDGIResources.probeGridIrradiance[1]) },
      { nvrhi::BindingSetItem::Texture_UAV(1, m_DDGIResources.probeGridDistance[1]) }
    };

    m_DDGIResources.indirectSamplingBindingSet[1] = m_Device->createBindingSet(indirectSamplingBindingSetDesc, m_DDGIResources.indirectSamplingLayout);

    if (m_DDGIResources.indirectSamplingBindingSet[0] == nullptr || m_DDGIResources.indirectSamplingBindingSet[1] == nullptr)
    {
      throw std::runtime_error("Failed to create indirect sampling binding set");
    }

    nvrhi::BindingLayoutDesc bindingLayoutDesc{};
    bindingLayoutDesc.visibility = nvrhi::ShaderType::AllGraphics;
    bindingLayoutDesc.bindings = {
       nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
       nvrhi::BindingLayoutItem::VolatileConstantBuffer(1),
       nvrhi::BindingLayoutItem::VolatileConstantBuffer(2),
       nvrhi::BindingLayoutItem::ConstantBuffer(3),
       nvrhi::BindingLayoutItem::Sampler(0),
       nvrhi::BindingLayoutItem::RayTracingAccelStruct(0),
    };

    m_BindingLayout = m_Device->createBindingLayout(bindingLayoutDesc);

    if (!m_BindingLayout) {
      throw std::runtime_error("Failed to create camera binding layout");
    }

    nvrhi::BindingSetDesc bindingSetDesc{};
    bindingSetDesc.bindings = {
      { nvrhi::BindingSetItem::ConstantBuffer(0, m_ConstantBuffer) },
      { nvrhi::BindingSetItem::ConstantBuffer(1, m_DDGIResources.ddgiConstants) },
      { nvrhi::BindingSetItem::ConstantBuffer(2, m_DDGIResources.debugConstants) },
      { nvrhi::BindingSetItem::ConstantBuffer(3, m_AssetRegistry.GetMaterialBuffer()) },
      { nvrhi::BindingSetItem::Sampler(0, m_MaterialSampler) },
      { nvrhi::BindingSetItem::RayTracingAccelStruct(0, m_RayTracingScene.GetTLAS()) }
    };

    m_BindingSet = m_Device->createBindingSet(bindingSetDesc, m_BindingLayout);

    if (!m_BindingSet) {
      throw std::runtime_error("Failed to create camera binding set");
    }
  }

  void DDGIExperiment::CreatePipeline(const nvrhi::FramebufferInfo& outputInfo)
  {
    const auto vsBytes = ReadBinary(ISKRA_SHADER_DIR "/DDGI.vs.dxil");
    const auto psBytes = ReadBinary(ISKRA_SHADER_DIR "/DDGI.ps.dxil");

    nvrhi::ShaderDesc vsDesc{};
    vsDesc.shaderType = nvrhi::ShaderType::Vertex;
    vsDesc.debugName = "DDGI.vs";
    vsDesc.entryName = "VSMain";

    nvrhi::ShaderDesc psDesc{};
    psDesc.shaderType = nvrhi::ShaderType::Pixel;
    psDesc.debugName = "DDGI.ps";
    psDesc.entryName = "PSMain";

    auto vertexShader = m_Device->createShader(vsDesc, vsBytes.data(), vsBytes.size());
    auto pixelShader = m_Device->createShader(psDesc, psBytes.data(), psBytes.size());

    if (!vertexShader || !pixelShader) {
      throw std::runtime_error("Failed to create mesh shaders");
    }

    nvrhi::GraphicsPipelineDesc pipelineDesc{};
    pipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;
    pipelineDesc.VS = vertexShader;
    pipelineDesc.PS = pixelShader;

    pipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;

    pipelineDesc.renderState.depthStencilState.depthTestEnable = true;
    pipelineDesc.renderState.depthStencilState.depthWriteEnable = true;
    pipelineDesc.renderState.depthStencilState.stencilEnable = false;

    auto& depth = pipelineDesc.renderState.depthStencilState;
    depth.depthTestEnable = true;
    depth.depthWriteEnable = true;
    depth.depthFunc = nvrhi::ComparisonFunc::Less;
    depth.stencilEnable = false;

    nvrhi::VertexAttributeDesc vertexAttributes[5]{};
    vertexAttributes[0].name = "POSITION";
    vertexAttributes[1].name = "COLOR";
    vertexAttributes[2].name = "TEXCOORD";
    vertexAttributes[3].name = "TANGENT";
    vertexAttributes[4].name = "BITANGENT";

    vertexAttributes[0].format = nvrhi::Format::RGB32_FLOAT;
    vertexAttributes[1].format = nvrhi::Format::RGB32_FLOAT;
    vertexAttributes[2].format = nvrhi::Format::RG32_FLOAT;
	vertexAttributes[3].format = nvrhi::Format::RGB32_FLOAT;
	vertexAttributes[4].format = nvrhi::Format::RGB32_FLOAT;

    vertexAttributes[0].offset = 0;
    vertexAttributes[1].offset = offsetof(Vertex, normal);
    vertexAttributes[2].offset = offsetof(Vertex, uv);
	vertexAttributes[3].offset = offsetof(Vertex, tangent);
	vertexAttributes[4].offset = offsetof(Vertex, bitangent);

    vertexAttributes[0].elementStride = sizeof(Vertex);
    vertexAttributes[1].elementStride = sizeof(Vertex);
    vertexAttributes[2].elementStride = sizeof(Vertex);
	vertexAttributes[3].elementStride = sizeof(Vertex);
	vertexAttributes[4].elementStride = sizeof(Vertex);

    vertexAttributes[0].bufferIndex = 0;
    vertexAttributes[1].bufferIndex = 0;
    vertexAttributes[2].bufferIndex = 0;
	vertexAttributes[3].bufferIndex = 0;
	vertexAttributes[4].bufferIndex = 0;

    auto inputLayout = m_Device->createInputLayout(vertexAttributes, 5, vertexShader);

    pipelineDesc.addBindingLayout(m_BindingLayout);
    pipelineDesc.addBindingLayout(m_DDGIResources.indirectSamplingLayout);
    pipelineDesc.addBindingLayout(m_AssetRegistry.GetBindlessTextureBindingLayout());

    pipelineDesc.inputLayout = inputLayout;

    m_Pipeline = m_Device->createGraphicsPipeline(pipelineDesc, outputInfo);

    if (!m_Pipeline) {
      throw std::runtime_error("Failed to create mesh pipeline");
    }
  }

  void DDGIExperiment::RenderGeometry(const FrameContext& frame)
  {
    DDGIConstants ddgiConstants{};
    ddgiConstants.lightColor = glm::vec4(m_LightColor, 1.0f);
    ddgiConstants.probeGridOrigin = m_DDGIResources.desc.center - m_DDGIResources.desc.halfSize;
    ddgiConstants.probeGridSpacing = 2.0f * m_DDGIResources.desc.halfSize / m_DDGIResources.desc.probeCount;
    ddgiConstants.probeGridSize = m_DDGIResources.desc.probeCount;
    frame.commandList->writeBuffer(m_DDGIResources.ddgiConstants, &ddgiConstants, sizeof(ddgiConstants));

    for (const MeshDrawItem& item : m_DrawItems) {
      ConstantBufferData constants{};
      constants.model = item.worldTransform;
      constants.view = m_Camera.GetViewMatrix();
      constants.projection = m_Camera.GetProjectionMatrix();
      constants.normalMatrix = glm::transpose(glm::inverse(item.worldTransform));
      constants.toLight = m_ToLight;
      constants.cameraPosition = m_Camera.GetPosition();
      constants.materialIndex = item.materialIndex;

      frame.commandList->writeBuffer(
        m_ConstantBuffer, &constants, sizeof(constants));

      DebugConstants debugMode{};
      int debug = 0;
      debug = debug | (m_UseDDGI ? 0 : 1);
      debug = debug | (m_ShowIndirectLighting ? 2 : 0);
      frame.commandList->writeBuffer(m_DDGIResources.debugConstants, &debug, sizeof(debug));

      nvrhi::GraphicsState state{};
      state.pipeline = m_Pipeline.Get();
      state.framebuffer = frame.framebuffer;
      state.addBindingSet(m_BindingSet);
      state.addBindingSet(m_DDGIResources.indirectSamplingBindingSet[m_DDGIResources.currentProbeAtlas]);
      state.addBindingSet(m_AssetRegistry.GetBindlessTextureDescriptorTable());
      state.viewport.addViewportAndScissorRect(
        frame.framebuffer->getFramebufferInfo().getViewport());

      m_MeshRenderer.Draw(frame.commandList,
        m_GpuMeshes.at(item.meshIndex), state);
    }
  }

  void DDGIExperiment::RenderProbeVisualization(const FrameContext& frame)
  {
    DDGIConstants ddgiConstants{};
    ddgiConstants.lightColor = glm::vec4(m_LightColor, 1.0f);
    ddgiConstants.irradianceQuadSize = m_DDGIResources.desc.probeIrradianceResolution;
    ddgiConstants.probeGridOrigin = m_DDGIResources.desc.center - m_DDGIResources.desc.halfSize;
    ddgiConstants.probeGridSpacing = 2.0f * m_DDGIResources.desc.halfSize / m_DDGIResources.desc.probeCount;
    ddgiConstants.probeGridSize = m_DDGIResources.desc.probeCount;
    frame.commandList->writeBuffer(m_DDGIResources.ddgiConstants, &ddgiConstants, sizeof(ddgiConstants));

    CameraConstants cameraConstants{};
    cameraConstants.view = m_Camera.GetViewMatrix();
    cameraConstants.projection = m_Camera.GetProjectionMatrix();

    frame.commandList->writeBuffer(
      m_DDGIResources.cameraConstants, &cameraConstants, sizeof(cameraConstants));

    nvrhi::MeshletState state{};
    state.pipeline = m_DDGIResources.probeVisualizationPipeline.Get();
    state.framebuffer = frame.framebuffer;
    state.addBindingSet(m_DDGIResources.probeVisualizationBindingSet[m_DDGIResources.currentProbeAtlas]);
    state.viewport.addViewportAndScissorRect(
      frame.framebuffer->getFramebufferInfo().getViewport());

    frame.commandList->setMeshletState(state);
    frame.commandList->dispatchMesh(static_cast<uint32_t>(m_DDGIResources.desc.probeCount.x * m_DDGIResources.desc.probeCount.y * m_DDGIResources.desc.probeCount.z), 1, 1);
  }

  void DDGIExperiment::UpdateProbeIrradiances(const FrameContext& frame)
  {
    DDGIConstants ddgiConstants{};
    ddgiConstants.lightColor = glm::vec4(m_LightColor, 1.0f);

    // random angle over [0, 2*pi] in radians
    float randomAngle = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f * glm::pi<float>();

    ddgiConstants.toLight = m_ToLight;
    ddgiConstants.randomAngle = randomAngle;
    ddgiConstants.probeHysteresis = m_DDGIResources.desc.probeHysteresis;
    glm::vec3 probeSpacing = 2.0f * m_DDGIResources.desc.halfSize / m_DDGIResources.desc.probeCount;
    ddgiConstants.probeGridOrigin = m_DDGIResources.desc.center - m_DDGIResources.desc.halfSize;
    ddgiConstants.probeGridSpacing = probeSpacing;
    ddgiConstants.probeGridSize = m_DDGIResources.desc.probeCount;
    ddgiConstants.probeMaxRayDistance = std::min(m_DDGIResources.desc.probeMaxRayDistance,
      1.5f * std::max({ probeSpacing.x, probeSpacing.y, probeSpacing.z }));

    frame.commandList->writeBuffer(
      m_DDGIResources.ddgiConstants, &ddgiConstants, sizeof(ddgiConstants));

    nvrhi::ComputeState state{};
    state.pipeline = m_DDGIResources.probeRayTracingPipeline.Get();
    state.addBindingSet(m_DDGIResources.probeRayTracingBindingSet[m_DDGIResources.currentProbeAtlas]);
    state.addBindingSet(m_AssetRegistry.GetBindlessTextureDescriptorTable());
    frame.commandList->setComputeState(state);
    frame.commandList->dispatch(
      static_cast<uint32_t>(m_DDGIResources.desc.probeCount.x * m_DDGIResources.desc.probeCount.y * m_DDGIResources.desc.probeCount.z), 1, 1);

    uint32_t previousAtlas = m_DDGIResources.currentProbeAtlas;
    uint32_t nextAtlas = 1 - previousAtlas;
    for (uint32_t pass = 0; pass < 2; ++pass) {
      auto texture = pass == 0 ? m_DDGIResources.probeGridIrradiance[nextAtlas]
        : m_DDGIResources.probeGridDistance[nextAtlas];
      const auto& textureDesc = texture->getDesc();
      ddgiConstants.irradiance = pass;
      ddgiConstants.irradianceTextureSize = textureDesc.width;
      ddgiConstants.irradianceQuadSize = pass == 0 ? m_DDGIResources.desc.probeIrradianceResolution
        : m_DDGIResources.desc.probeDistanceResolution;
      frame.commandList->writeBuffer(
        m_DDGIResources.ddgiConstants, &ddgiConstants, sizeof(ddgiConstants));

      nvrhi::ComputeState updateState{};
      updateState.pipeline = m_DDGIResources.probeIrradianceUpdatePipeline.Get();
      updateState.addBindingSet(pass == 0 ? m_DDGIResources.probeIrradianceUpdateBindingSet[previousAtlas]
        : m_DDGIResources.probeVisibilityUpdateBindingSet[previousAtlas]);
      frame.commandList->setComputeState(updateState);
      frame.commandList->dispatch((textureDesc.width + 7) / 8, (textureDesc.height + 7) / 8, 1);
    }
    m_DDGIResources.currentProbeAtlas = nextAtlas;
  }

  void DDGIExperiment::InitializeDDGI(DDGIVolumeDesc desc, const nvrhi::FramebufferInfo& outputInfo)
  {
    if (desc.probeNumRays != 128 || desc.probeIrradianceResolution <= 0 || desc.probeDistanceResolution <= 0 ||
      desc.probeCount.x <= 0 || desc.probeCount.y <= 0 || desc.probeCount.z <= 0 ||
      desc.probeCount.x != std::floor(desc.probeCount.x) ||
      desc.probeCount.y != std::floor(desc.probeCount.y) ||
      desc.probeCount.z != std::floor(desc.probeCount.z)) {
      throw std::runtime_error("DDGI requires positive integer probe counts, positive resolutions and 128 rays per probe");
    }
    m_DDGIResources.currentProbeAtlas = 0;
    m_DDGIResources.desc = desc;
    InitializeDDGIResources(desc);
    InitializeDDGIBindings(desc);
    InitDDGIPipeline(outputInfo);
  }

  void DDGIExperiment::InitializeDDGIResources(DDGIVolumeDesc desc)
  {
    auto probePositions = BuildProbePositions(desc);

    nvrhi::BufferDesc probePositionsBufferDesc{};
    probePositionsBufferDesc.byteSize = probePositions.size() * sizeof(glm::vec3);
    probePositionsBufferDesc.debugName = "Probe Positions Buffer";
    probePositionsBufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
    probePositionsBufferDesc.keepInitialState = true;
    probePositionsBufferDesc.structStride = sizeof(glm::vec3);

    m_DDGIResources.probePositions = m_Device->createBuffer(probePositionsBufferDesc);

    nvrhi::BufferDesc cameraConstantsBufferDesc{};
    cameraConstantsBufferDesc.byteSize = sizeof(CameraConstants);
    cameraConstantsBufferDesc.debugName = "Camera Constants Buffer";
    cameraConstantsBufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
    cameraConstantsBufferDesc.keepInitialState = true;
    cameraConstantsBufferDesc.isVolatile = true;
    cameraConstantsBufferDesc.isConstantBuffer = true;

    m_DDGIResources.cameraConstants = m_Device->createBuffer(cameraConstantsBufferDesc);

    nvrhi::BufferDesc randomDirectionsBufferDesc{};
    randomDirectionsBufferDesc.byteSize = 1 * sizeof(DDGIConstants);
    randomDirectionsBufferDesc.debugName = "Random Directions Buffer";
    randomDirectionsBufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
    randomDirectionsBufferDesc.keepInitialState = true;
    randomDirectionsBufferDesc.isConstantBuffer = true;
    randomDirectionsBufferDesc.isVolatile = true;
    // randomDirectionsBufferDesc.structStride = sizeof(glm::vec3);

    m_DDGIResources.ddgiConstants = m_Device->createBuffer(randomDirectionsBufferDesc);

    // Pack mesh positions and globally rebased indices for ray-query hit lookup.
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> uvs;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> meshIndexOffsets;
    for (const MeshData& mesh : m_Model.meshes) {
      const uint32_t vertexOffset = static_cast<uint32_t>(positions.size());
      meshIndexOffsets.push_back(static_cast<uint32_t>(indices.size()));
      for (const Vertex& vertex : mesh.vertices) {
        positions.push_back(vertex.position);
        uvs.push_back(vertex.uv);
      }
      for (uint32_t index : mesh.indices) {
        indices.push_back(vertexOffset + index);
      }
    }

    // CollectDrawItems and CollectRayTracingInstances use the same node/mesh order.
    std::vector<uint32_t> instanceIndexOffsets;
    std::vector<int32_t> instanceMaterialIndices;
    for (const MeshDrawItem& item : m_DrawItems) {
      instanceIndexOffsets.push_back(meshIndexOffsets.at(item.meshIndex));
      instanceMaterialIndices.push_back(item.materialIndex);
    }

    auto createSceneBuffer = [&](size_t byteSize, uint32_t stride, const char* name) {
      nvrhi::BufferDesc bufferDesc{};
      bufferDesc.byteSize = byteSize;
      bufferDesc.structStride = stride;
      bufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;
      bufferDesc.keepInitialState = true;
      bufferDesc.debugName = name;
      auto buffer = m_Device->createBuffer(bufferDesc);
      if (!buffer) {
        throw std::runtime_error(std::string("Failed to create ") + name);
      }
      return buffer;
      };
    m_DDGIResources.sceneVertexPositions = createSceneBuffer(
      positions.size() * sizeof(glm::vec3), sizeof(glm::vec3), "DDGI Scene Positions");
    m_DDGIResources.sceneVertexUVs = createSceneBuffer(
      uvs.size() * sizeof(glm::vec2), sizeof(glm::vec2), "DDGI Scene UVs");
    m_DDGIResources.sceneIndices = createSceneBuffer(
      indices.size() * sizeof(uint32_t), sizeof(uint32_t), "DDGI Scene Indices");
    m_DDGIResources.instanceIndexOffsets = createSceneBuffer(
      instanceIndexOffsets.size() * sizeof(uint32_t), sizeof(uint32_t), "DDGI Instance Index Offsets");
    m_DDGIResources.instanceMaterialIndices = createSceneBuffer(
      instanceMaterialIndices.size() * sizeof(int32_t), sizeof(int32_t), "DDGI Instance Material Indices");

    auto upload = m_Device->createCommandList();
    upload->open();
    upload->writeBuffer(m_DDGIResources.sceneVertexPositions, positions.data(), positions.size() * sizeof(glm::vec3));
    upload->writeBuffer(m_DDGIResources.sceneVertexUVs, uvs.data(), uvs.size() * sizeof(glm::vec2));
    upload->writeBuffer(m_DDGIResources.sceneIndices, indices.data(), indices.size() * sizeof(uint32_t));
    upload->writeBuffer(m_DDGIResources.instanceIndexOffsets, instanceIndexOffsets.data(), instanceIndexOffsets.size() * sizeof(uint32_t));
    upload->writeBuffer(m_DDGIResources.instanceMaterialIndices, instanceMaterialIndices.data(), instanceMaterialIndices.size() * sizeof(int32_t));
    upload->writeBuffer(m_DDGIResources.probePositions, probePositions.data(), probePositions.size() * sizeof(glm::vec3));
    upload->close();
    m_Device->executeCommandList(upload);

    nvrhi::TextureDesc textureDesc{};
    textureDesc.width = static_cast<uint32_t>(desc.probeCount.x * desc.probeCount.y * desc.probeCount.z);
    textureDesc.height = desc.probeNumRays;
    textureDesc.format = nvrhi::Format::RGBA16_FLOAT;
    textureDesc.debugName = "Probe Radiance Distance Texture";
    textureDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
    textureDesc.keepInitialState = true;
    textureDesc.dimension = nvrhi::TextureDimension::Texture2D;
    textureDesc.isUAV = true;

    m_DDGIResources.rayProbeRadianceDistance = m_Device->createTexture(textureDesc);

    uint32_t probeIrradianceResolution = desc.probeIrradianceResolution;
    nvrhi::TextureDesc probeGridIrradianceTextureDesc{};
    probeGridIrradianceTextureDesc.width =
      (probeIrradianceResolution + 2) *        // per probe irradiance quad with border for bilinear filtering
      static_cast<uint32_t>(desc.probeCount.x * desc.probeCount.y); // linear probe tiles per row

    probeGridIrradianceTextureDesc.height =
      (probeIrradianceResolution + 2) *        // per probe irradiance quad with border for bilinear filtering
      static_cast<uint32_t>(desc.probeCount.z);                     // probe volume Z dimension

    probeGridIrradianceTextureDesc.format = nvrhi::Format::RGBA16_FLOAT;
    probeGridIrradianceTextureDesc.debugName = "Probe Grid Irradiance TextureAtlas";
    probeGridIrradianceTextureDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
    probeGridIrradianceTextureDesc.keepInitialState = true;
    probeGridIrradianceTextureDesc.dimension = nvrhi::TextureDimension::Texture2D;
    probeGridIrradianceTextureDesc.isUAV = true;

    m_DDGIResources.probeGridIrradiance[0] = m_Device->createTexture(probeGridIrradianceTextureDesc);
    m_DDGIResources.probeGridIrradiance[1] = m_Device->createTexture(probeGridIrradianceTextureDesc);

    uint32_t probeDistanceResolution = desc.probeDistanceResolution;
    nvrhi::TextureDesc probeGridDistanceTextureDesc{};
    probeGridDistanceTextureDesc.width =
      (probeDistanceResolution + 2) *        // per probe distance quad with border for bilinear filtering
      static_cast<uint32_t>(desc.probeCount.x * desc.probeCount.y); // linear probe tiles per row
    probeGridDistanceTextureDesc.height =
      (probeDistanceResolution + 2) *        // per probe distance quad with border for bilinear filtering
      static_cast<uint32_t>(desc.probeCount.z);                   // probe volume Z dimension

    probeGridDistanceTextureDesc.format = nvrhi::Format::RGBA16_FLOAT;
    probeGridDistanceTextureDesc.debugName = "Probe Grid Distance TextureAtlas";
    probeGridDistanceTextureDesc.initialState = nvrhi::ResourceStates::UnorderedAccess;
    probeGridDistanceTextureDesc.keepInitialState = true;
    probeGridDistanceTextureDesc.dimension = nvrhi::TextureDimension::Texture2D;
    probeGridDistanceTextureDesc.isUAV = true;

    m_DDGIResources.probeGridDistance[0] = m_Device->createTexture(probeGridDistanceTextureDesc);
    m_DDGIResources.probeGridDistance[1] = m_Device->createTexture(probeGridDistanceTextureDesc);

    if (!m_DDGIResources.rayProbeRadianceDistance || !m_DDGIResources.probeGridIrradiance[0] ||
      !m_DDGIResources.probeGridIrradiance[1] || !m_DDGIResources.probeGridDistance[0] ||
      !m_DDGIResources.probeGridDistance[1]) {
      throw std::runtime_error("Failed to create DDGI textures");
    }

    auto clear = m_Device->createCommandList();
    clear->open();
    for (uint32_t atlas = 0; atlas < 2; ++atlas) {
      clear->clearTextureFloat(m_DDGIResources.probeGridIrradiance[atlas], nvrhi::AllSubresources, nvrhi::Color(0.0f));
      clear->clearTextureFloat(m_DDGIResources.probeGridDistance[atlas], nvrhi::AllSubresources, nvrhi::Color(0.0f));
    }
    clear->close();
    m_Device->executeCommandList(clear);

  }

  void DDGIExperiment::InitializeDDGIBindings(DDGIVolumeDesc desc)
  {
    nvrhi::BindingLayoutDesc probeVisualizationLayoutDesc{};
    probeVisualizationLayoutDesc.visibility = nvrhi::ShaderType::Mesh | nvrhi::ShaderType::Pixel;
    probeVisualizationLayoutDesc.bindings = {
       nvrhi::BindingLayoutItem::StructuredBuffer_SRV(0),
       nvrhi::BindingLayoutItem::VolatileConstantBuffer(1),
       nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
       nvrhi::BindingLayoutItem::Texture_UAV(0)
    };

    nvrhi::BindingLayoutDesc probeRayTracingLayoutDesc{};
    probeRayTracingLayoutDesc.visibility = nvrhi::ShaderType::All;
    probeRayTracingLayoutDesc.bindings = {
       nvrhi::BindingLayoutItem::RayTracingAccelStruct(0),
       nvrhi::BindingLayoutItem::StructuredBuffer_SRV(1),
       nvrhi::BindingLayoutItem::StructuredBuffer_SRV(2),
       nvrhi::BindingLayoutItem::StructuredBuffer_SRV(3),
       nvrhi::BindingLayoutItem::StructuredBuffer_SRV(4),
       nvrhi::BindingLayoutItem::StructuredBuffer_SRV(5),
       nvrhi::BindingLayoutItem::StructuredBuffer_SRV(6),
       nvrhi::BindingLayoutItem::Sampler(0),
       nvrhi::BindingLayoutItem::ConstantBuffer(1),
       nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
       nvrhi::BindingLayoutItem::Texture_UAV(0),
       nvrhi::BindingLayoutItem::Texture_UAV(1),
       nvrhi::BindingLayoutItem::Texture_UAV(2),
    };

    m_DDGIResources.probeRayTracingLayout = m_Device->createBindingLayout(probeRayTracingLayoutDesc);

    m_DDGIResources.probeVisualizationLayout = m_Device->createBindingLayout(probeVisualizationLayoutDesc);

    if (!m_DDGIResources.probeVisualizationLayout) {
      throw std::runtime_error("Failed to create probe visualization binding layout");
    }

    for (uint32_t atlas = 0; atlas < 2; ++atlas) {
      nvrhi::BindingSetDesc probeVisualizationBindingSet{};
      probeVisualizationBindingSet.bindings = {
        { nvrhi::BindingSetItem::StructuredBuffer_SRV(0, m_DDGIResources.probePositions) },
        { nvrhi::BindingSetItem::ConstantBuffer(0, m_DDGIResources.cameraConstants) },
        { nvrhi::BindingSetItem::ConstantBuffer(1, m_DDGIResources.ddgiConstants) },
        { nvrhi::BindingSetItem::Texture_UAV(0, m_DDGIResources.probeGridIrradiance[atlas]) }
      };
      m_DDGIResources.probeVisualizationBindingSet[atlas] = m_Device->createBindingSet(
        probeVisualizationBindingSet, m_DDGIResources.probeVisualizationLayout);
      if (!m_DDGIResources.probeVisualizationBindingSet[atlas]) {
        throw std::runtime_error("Failed to create probe visualization binding set");
      }
    }

    for (uint32_t atlas = 0; atlas < 2; ++atlas) {
      nvrhi::BindingSetDesc probeRayTracingBindingSet{};
      probeRayTracingBindingSet.bindings = {
        { nvrhi::BindingSetItem::RayTracingAccelStruct(0, m_RayTracingScene.GetTLAS()) },
        { nvrhi::BindingSetItem::StructuredBuffer_SRV(1, m_DDGIResources.probePositions) },
        { nvrhi::BindingSetItem::StructuredBuffer_SRV(2, m_DDGIResources.sceneVertexPositions) },
        { nvrhi::BindingSetItem::StructuredBuffer_SRV(3, m_DDGIResources.sceneIndices) },
        { nvrhi::BindingSetItem::StructuredBuffer_SRV(4, m_DDGIResources.instanceIndexOffsets) },
        { nvrhi::BindingSetItem::StructuredBuffer_SRV(5, m_DDGIResources.sceneVertexUVs) },
        { nvrhi::BindingSetItem::StructuredBuffer_SRV(6, m_DDGIResources.instanceMaterialIndices) },
        { nvrhi::BindingSetItem::Sampler(0, m_MaterialSampler) },
        { nvrhi::BindingSetItem::ConstantBuffer(1, m_AssetRegistry.GetMaterialBuffer()) },
        { nvrhi::BindingSetItem::ConstantBuffer(0, m_DDGIResources.ddgiConstants) },
        { nvrhi::BindingSetItem::Texture_UAV(0, m_DDGIResources.rayProbeRadianceDistance) },
        { nvrhi::BindingSetItem::Texture_UAV(1, m_DDGIResources.probeGridIrradiance[atlas]) },
        { nvrhi::BindingSetItem::Texture_UAV(2, m_DDGIResources.probeGridDistance[atlas]) }
      };

      m_DDGIResources.probeRayTracingBindingSet[atlas] = m_Device->createBindingSet(probeRayTracingBindingSet, m_DDGIResources.probeRayTracingLayout);

      if (!m_DDGIResources.probeRayTracingBindingSet[atlas]) {
        throw std::runtime_error("Failed to create probe ray tracing binding set");
      }
    }
    nvrhi::BindingLayoutDesc probeUpdateLayoutDesc{};
    probeUpdateLayoutDesc.visibility = nvrhi::ShaderType::Compute;
    probeUpdateLayoutDesc.bindings = {
      nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),
      nvrhi::BindingLayoutItem::Texture_UAV(0),
      nvrhi::BindingLayoutItem::Texture_UAV(1),
      nvrhi::BindingLayoutItem::Texture_UAV(2)
    };
    m_DDGIResources.probeIrradianceUpdateLayout = m_Device->createBindingLayout(probeUpdateLayoutDesc);
    if (!m_DDGIResources.probeRayTracingLayout || !m_DDGIResources.probeIrradianceUpdateLayout) {
      throw std::runtime_error("Failed to create DDGI compute binding layouts");
    }

    for (uint32_t atlas = 0; atlas < 2; ++atlas) {
      for (uint32_t pass = 0; pass < 2; ++pass) {
        auto previous = pass == 0 ? m_DDGIResources.probeGridIrradiance[atlas] : m_DDGIResources.probeGridDistance[atlas];
        auto next = pass == 0 ? m_DDGIResources.probeGridIrradiance[1 - atlas] : m_DDGIResources.probeGridDistance[1 - atlas];
        nvrhi::BindingSetDesc probeUpdateBindingSet{};
        probeUpdateBindingSet.bindings = {
          { nvrhi::BindingSetItem::ConstantBuffer(0, m_DDGIResources.ddgiConstants) },
          { nvrhi::BindingSetItem::Texture_UAV(0, previous) },
          { nvrhi::BindingSetItem::Texture_UAV(1, m_DDGIResources.rayProbeRadianceDistance) },
          { nvrhi::BindingSetItem::Texture_UAV(2, next) }
        };
        auto bindingSet = m_Device->createBindingSet(probeUpdateBindingSet, m_DDGIResources.probeIrradianceUpdateLayout);
        if (!bindingSet) {
          throw std::runtime_error("Failed to create probe update binding set");
        }
        if (pass == 0) {
          m_DDGIResources.probeIrradianceUpdateBindingSet[atlas] = bindingSet;
        }
        else {
          m_DDGIResources.probeVisibilityUpdateBindingSet[atlas] = bindingSet;
        }
      }
    }
  }

  void DDGIExperiment::InitDDGIPipeline(const nvrhi::FramebufferInfo& outputInfo)
  {
    const auto ddgiMsBytes = ReadBinary(ISKRA_SHADER_DIR "/ProbeVisualization.ms.dxil");
    const auto ddgiPsBytes = ReadBinary(ISKRA_SHADER_DIR "/ProbeVisualization.ps.dxil");
    const auto ddgiProbeRayTracingBytes = ReadBinary(ISKRA_SHADER_DIR "/DDGIProbeRayTracing.cs.dxil");

    nvrhi::ShaderDesc ddgiPsDesc{};
    ddgiPsDesc.shaderType = nvrhi::ShaderType::Pixel;
    ddgiPsDesc.debugName = "ProbeVisualization.ps";
    ddgiPsDesc.entryName = "PSMain";

    nvrhi::ShaderDesc ddgiMsDesc{};
    ddgiMsDesc.shaderType = nvrhi::ShaderType::Mesh;
    ddgiMsDesc.debugName = "ProbeVisualization.ms";
    ddgiMsDesc.entryName = "MSMain";

    nvrhi::ShaderDesc ddgiProbeRayTracingDesc{};
    ddgiProbeRayTracingDesc.shaderType = nvrhi::ShaderType::Compute;
    ddgiProbeRayTracingDesc.debugName = "ProbeRayTracing.cs";
    ddgiProbeRayTracingDesc.entryName = "CSMain";

    m_DDGIResources.probeVisualizationMeshShader = m_Device->createShader(ddgiMsDesc, ddgiMsBytes.data(), ddgiMsBytes.size());
    m_DDGIResources.probeVisualizationPixelShader = m_Device->createShader(ddgiPsDesc, ddgiPsBytes.data(), ddgiPsBytes.size());
    m_DDGIResources.probeRayTracingComputeShader = m_Device->createShader(ddgiProbeRayTracingDesc, ddgiProbeRayTracingBytes.data(), ddgiProbeRayTracingBytes.size());

    if (!m_DDGIResources.probeVisualizationMeshShader || !m_DDGIResources.probeVisualizationPixelShader) {
      throw std::runtime_error("Failed to create probe visualization shaders");
    }

    if (!m_DDGIResources.probeRayTracingComputeShader) {
      throw std::runtime_error("Failed to create probe ray tracing compute shader");
    }

    nvrhi::MeshletPipelineDesc probeVisualizationPipelineDesc{};
    probeVisualizationPipelineDesc.primType = nvrhi::PrimitiveType::TriangleList;
    probeVisualizationPipelineDesc.MS = m_DDGIResources.probeVisualizationMeshShader;
    probeVisualizationPipelineDesc.PS = m_DDGIResources.probeVisualizationPixelShader;
    probeVisualizationPipelineDesc.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
    probeVisualizationPipelineDesc.renderState.depthStencilState.depthTestEnable = true;
    probeVisualizationPipelineDesc.renderState.depthStencilState.depthWriteEnable = true;
    probeVisualizationPipelineDesc.renderState.depthStencilState.stencilEnable = false;

    auto& probeDepth = probeVisualizationPipelineDesc.renderState.depthStencilState;
    probeDepth.depthTestEnable = true;
    probeDepth.depthWriteEnable = true;
    probeDepth.depthFunc = nvrhi::ComparisonFunc::Less;

    probeVisualizationPipelineDesc.addBindingLayout(m_DDGIResources.probeVisualizationLayout);
    m_DDGIResources.probeVisualizationPipeline = m_Device->createMeshletPipeline(probeVisualizationPipelineDesc, outputInfo);

    if (!m_DDGIResources.probeVisualizationPipeline) {
      throw std::runtime_error("Failed to create probe visualization pipeline");
    }

    nvrhi::ComputePipelineDesc probeRayTracingPipelineDesc{};
    probeRayTracingPipelineDesc.CS = m_DDGIResources.probeRayTracingComputeShader;
    probeRayTracingPipelineDesc.addBindingLayout(m_DDGIResources.probeRayTracingLayout);
    probeRayTracingPipelineDesc.addBindingLayout(m_AssetRegistry.GetBindlessTextureBindingLayout());

    m_DDGIResources.probeRayTracingPipeline = m_Device->createComputePipeline(probeRayTracingPipelineDesc);

    if (!m_DDGIResources.probeRayTracingPipeline) {
      throw std::runtime_error("Failed to create probe irradiance update pipeline");
    }
    const auto probeUpdateBytes = ReadBinary(ISKRA_SHADER_DIR "/DDGIProbeIrradianceVisibilityUpdate.cs.dxil");
    nvrhi::ShaderDesc probeUpdateDesc{};
    probeUpdateDesc.shaderType = nvrhi::ShaderType::Compute;
    probeUpdateDesc.debugName = "DDGIProbeIrradianceVisibilityUpdate.cs";
    probeUpdateDesc.entryName = "CSMain";
    m_DDGIResources.probeIrradianceUpdateComputeShader = m_Device->createShader(
      probeUpdateDesc, probeUpdateBytes.data(), probeUpdateBytes.size());
    if (!m_DDGIResources.probeIrradianceUpdateComputeShader) {
      throw std::runtime_error("Failed to create probe update compute shader");
    }

    nvrhi::ComputePipelineDesc probeUpdatePipelineDesc{};
    probeUpdatePipelineDesc.CS = m_DDGIResources.probeIrradianceUpdateComputeShader;
    probeUpdatePipelineDesc.addBindingLayout(m_DDGIResources.probeIrradianceUpdateLayout);
    m_DDGIResources.probeIrradianceUpdatePipeline = m_Device->createComputePipeline(probeUpdatePipelineDesc);
    if (!m_DDGIResources.probeIrradianceUpdatePipeline) {
      throw std::runtime_error("Failed to create probe update pipeline");
    }
  }

  void DDGIExperiment::OnUpdate(float deltaTime)
  {
    m_CameraController.OnUpdate(deltaTime);
    m_AssetRegistry.UpdateResourcesIfDirty();
    m_ToLight = glm::vec3(
      cos(m_SunElevation) * sin(m_SunAzimuth),
      sin(m_SunElevation),
      cos(m_SunElevation) * cos(m_SunAzimuth));
  }

  void DDGIExperiment::OnInput(float deltaTime, Input& input)
  {
    m_CameraController.OnInput(deltaTime, input);
  }

  void DDGIExperiment::ResetProbeHistory(const FrameContext& frame)
  {
    for (uint32_t atlas = 0; atlas < 2; ++atlas) {
      frame.commandList->clearTextureFloat(m_DDGIResources.probeGridIrradiance[atlas],
        nvrhi::AllSubresources, nvrhi::Color(0.0f));
      frame.commandList->clearTextureFloat(m_DDGIResources.probeGridDistance[atlas],
        nvrhi::AllSubresources, nvrhi::Color(0.0f));
    }
    m_DDGIResources.currentProbeAtlas = 0;
    m_ResetProbeHistory = false;
  }

  void DDGIExperiment::OnRender(FrameContext& frame)
  {
    if (m_ProbeResourcesDirty) {
      // Rebuild only when counts or atlas resolutions change, after old GPU work completes.
      if (!m_Device->waitForIdle())
        throw std::runtime_error("Failed to wait for DDGI resource rebuild");
      const auto desc = m_DDGIResources.desc;
      const auto outputInfo = frame.framebuffer->getFramebufferInfo();
      InitializeDDGI(desc, outputInfo);
      CreateBindings();
      CreatePipeline(outputInfo);
      m_ProbeResourcesDirty = false;
      m_ProbePositionsDirty = false;
    }
    if (m_ProbePositionsDirty) {
      const auto positions = BuildProbePositions(m_DDGIResources.desc);
      frame.commandList->writeBuffer(m_DDGIResources.probePositions,
        positions.data(), positions.size() * sizeof(glm::vec3));
      m_ProbePositionsDirty = false;
    }
    if (m_ResetProbeHistory)
      ResetProbeHistory(frame);

    frame.commandList->clearDepthStencilTexture(
      frame.depthBuffer,
      nvrhi::AllSubresources,
      true, 1.0f, false, 0);

    frame.commandList->clearTextureFloat(
      frame.backbuffer,
      nvrhi::AllSubresources,
      nvrhi::Color(0.15f, 0.12f, 0.20f, 1.0f));

    UpdateProbeIrradiances(frame);
    RenderGeometry(frame);
    if (m_ShowProbes)
      RenderProbeVisualization(frame);

  }

  void DDGIExperiment::OnUiRender(FrameContext& frame)
  {
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420.0f, 520.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Lighting");
    m_ResetProbeHistory |= ImGui::SliderAngle("Sun azimuth", &m_SunAzimuth, -180.0f, 180.0f);
    m_ResetProbeHistory |= ImGui::SliderAngle("Sun elevation", &m_SunElevation, 1.0f, 89.0f);
    ImGui::Text("Direction: %.2f, %.2f, %.2f", m_ToLight.x, m_ToLight.y, m_ToLight.z);
    m_ResetProbeHistory |= ImGui::ColorEdit3("Light color", &m_LightColor.x);
    ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
    // Checkbox to enable and disable the probe visualization
    ImGui::Checkbox("Show probes", &m_ShowProbes);
    ImGui::Checkbox("Use DDGI", &m_UseDDGI);
    ImGui::Checkbox("Show Indirect Lighting", &m_ShowIndirectLighting);
    if (ImGui::CollapsingHeader("Probe volume", ImGuiTreeNodeFlags_DefaultOpen)) {
      auto& desc = m_DDGIResources.desc;
      bool moved = ImGui::DragFloat3("Volume center", &desc.center.x, 0.05f,
        -10000.0f, 10000.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
      glm::vec3 spacing = 2.0f * desc.halfSize / desc.probeCount;
      if (ImGui::DragFloat3("Probe spacing", &spacing.x, 0.01f,
        0.05f, 100.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
        desc.halfSize = 0.5f * spacing * desc.probeCount;
        moved = true;
      }
      if (ImGui::DragFloat3("Volume half size", &desc.halfSize.x, 0.05f,
        0.05f, 1000.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp))
        moved = true;
      m_ProbePositionsDirty |= moved;
      m_ResetProbeHistory |= moved;

      int counts[3] = { int(desc.probeCount.x), int(desc.probeCount.y), int(desc.probeCount.z) };
      bool rebuilt = false;
      if (ImGui::DragInt3("Probe counts", counts, 0.1f, 1, 16, "%d", ImGuiSliderFlags_AlwaysClamp)) {
        // Preserve spacing and center when changing the number of cells.
        spacing = 2.0f * desc.halfSize / desc.probeCount;
        desc.probeCount = glm::vec3(counts[0], counts[1], counts[2]);
        desc.halfSize = 0.5f * spacing * desc.probeCount;
        rebuilt = true;
      }
      rebuilt |= ImGui::SliderInt("Irradiance resolution", &desc.probeIrradianceResolution, 2, 16, "%d", ImGuiSliderFlags_AlwaysClamp);
      rebuilt |= ImGui::SliderInt("Distance resolution", &desc.probeDistanceResolution, 2, 16, "%d", ImGuiSliderFlags_AlwaysClamp);
      m_ProbeResourcesDirty |= rebuilt;
      m_ResetProbeHistory |= rebuilt;
      m_ResetProbeHistory |= ImGui::SliderFloat("History weight", &desc.probeHysteresis, 0.0f, 0.99f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
      float maxDistance = std::min(desc.probeMaxRayDistance, 1000.0f);
      if (ImGui::DragFloat("Distance moment limit", &maxDistance, 0.1f,
        0.05f, 1000.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
        desc.probeMaxRayDistance = maxDistance;
        m_ResetProbeHistory = true;
      }
      const auto count = desc.probeCount;
      ImGui::Text("%d probes, %d rays per probe", int(count.x * count.y * count.z), desc.probeNumRays);
      if (ImGui::Button("Reset accumulation"))
        m_ResetProbeHistory = true;
    }
    ImGui::End();
  }

  void DDGIExperiment::OnDestroy()
  {
  }

  void DDGIExperiment::OnResize(int width, int height)
  {
  }

  std::string_view DDGIExperiment::GetName() const
  {
    return std::string_view("DDGI Experiment");
  }

}
