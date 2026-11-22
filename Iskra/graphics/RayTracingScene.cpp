#include <graphics/RayTracingScene.h>

#include <stdexcept>

namespace Iskra {
void RayTracingScene::Build(nvrhi::IDevice *device,
                            nvrhi::ICommandList *commandList,
                            std::span<const RenderInstance> renderInstances,
                            const AssetRegistry &assets) {
  if (!device->queryFeatureSupport(nvrhi::Feature::RayQuery)) {
    throw std::runtime_error("Inline ray tracing is unsupported");
  }

  m_BLASes.clear();
  m_BLASes.reserve(assets.GetMeshCount());

  for (uint32_t i = 0; i < assets.GetMeshCount(); ++i) {
    m_BLASes.push_back(
        BuildBLAS(device, commandList, assets.GetGpuMesh(MeshHandle(i))));
  }

  if (renderInstances.empty()) {
    throw std::runtime_error("No ray-tracing instances");
  }

  std::vector<nvrhi::rt::InstanceDesc> instances;
  instances.reserve(renderInstances.size());
  for (uint32_t instanceIndex = 0; instanceIndex < renderInstances.size();
       ++instanceIndex) {
    const RenderInstance &source = renderInstances[instanceIndex];
    nvrhi::rt::InstanceDesc instance{};

    for (int row = 0; row < 3; ++row) {
      for (int column = 0; column < 4; ++column) {
        instance.transform[row * 4 + column] =
            source.worldTransform[column][row];
      }
    }

    instance.bottomLevelAS = m_BLASes.at(source.mesh.GetIndex()).Get();
    instance.instanceMask = static_cast<uint8_t>(source.visibilityMask);
    instance.instanceID = instanceIndex;
    instances.push_back(instance);
  }

  nvrhi::rt::AccelStructDesc desc{};
  desc.isTopLevel = true;
  desc.topLevelMaxInstances = instances.size();
  desc.buildFlags = nvrhi::rt::AccelStructBuildFlags::PreferFastTrace;
  desc.debugName = "Scene TLAS";

  m_TLAS = device->createAccelStruct(desc);

  if (!m_TLAS) {
    throw std::runtime_error("Failed to create TLAS");
  }

  commandList->buildTopLevelAccelStruct(m_TLAS, instances.data(),
                                        instances.size(), desc.buildFlags);
}

nvrhi::rt::AccelStructHandle RayTracingScene::BuildBLAS(
    nvrhi::IDevice *device, nvrhi::ICommandList *commandList,
    const GpuMesh &mesh) {
  nvrhi::rt::GeometryTriangles triangles{};
  triangles.vertexBuffer = mesh.vertexBuffer;
  triangles.vertexFormat = nvrhi::Format::RGB32_FLOAT;
  triangles.vertexOffset = offsetof(Vertex, position);
  triangles.vertexStride = sizeof(Vertex);
  triangles.vertexCount = mesh.vertexCount;

  triangles.indexBuffer = mesh.indexBuffer;
  triangles.indexFormat = nvrhi::Format::R32_UINT;
  triangles.indexOffset = 0;
  triangles.indexCount = mesh.indexCount;

  nvrhi::rt::GeometryDesc geometry{};
  geometry.setTriangles(triangles);
  geometry.flags = nvrhi::rt::GeometryFlags::Opaque;

  nvrhi::rt::AccelStructDesc desc{};
  desc.isTopLevel = false;
  desc.bottomLevelGeometries = {geometry};
  desc.buildFlags = nvrhi::rt::AccelStructBuildFlags::PreferFastTrace;
  desc.debugName = "Mesh BLAS";

  auto blas = device->createAccelStruct(desc);

  if (!blas) {
    throw std::runtime_error("Failed to create BLAS for mesh");
  };

  commandList->buildBottomLevelAccelStruct(blas, &geometry, 1, desc.buildFlags);

  return blas;
}

}  // namespace Iskra
