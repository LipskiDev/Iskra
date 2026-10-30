#include <graphics/RayTracingScene.h>

#include <stdexcept>

namespace Iskra {
  void RayTracingScene::Build(
    nvrhi::IDevice* device,
    nvrhi::ICommandList* commandList,
    const ModelData& model,
    const std::vector<GpuMesh>& gpuMeshes,
    const glm::mat4& placement)
  {
    if (!device->queryFeatureSupport(nvrhi::Feature::RayQuery)) {
      throw std::runtime_error("Inline ray tracing is unsupported");
    }

    m_BLASes.clear();
    m_BLASes.reserve(gpuMeshes.size());

    for (const GpuMesh& mesh : gpuMeshes) {
      m_BLASes.push_back(
        BuildBLAS(device, commandList, mesh));
    }

    std::vector<nvrhi::rt::InstanceDesc> instances;

    CollectRayTracingInstances(
      model.root,
      placement,
      m_BLASes,
      instances);

    if (instances.empty()) {
      throw std::runtime_error("No ray-tracing instances");
    }

    nvrhi::rt::AccelStructDesc desc{};
    desc.isTopLevel = true;
    desc.topLevelMaxInstances = instances.size();
    desc.buildFlags =
      nvrhi::rt::AccelStructBuildFlags::PreferFastTrace;
    desc.debugName = "Scene TLAS";

    m_TLAS = device->createAccelStruct(desc);

    if (!m_TLAS) {
      throw std::runtime_error("Failed to create TLAS");
    }

    commandList->buildTopLevelAccelStruct(
      m_TLAS,
      instances.data(),
      instances.size(),
      desc.buildFlags);
  }

  nvrhi::rt::AccelStructHandle RayTracingScene::BuildBLAS(nvrhi::IDevice* device, nvrhi::ICommandList* commandList, const GpuMesh& mesh)
  {
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
    desc.bottomLevelGeometries = { geometry };
    desc.buildFlags = nvrhi::rt::AccelStructBuildFlags::PreferFastTrace;
    desc.debugName = "Mesh BLAS";

    auto blas = device->createAccelStruct(desc);

    if (!blas) {
      throw std::runtime_error("Failed to create BLAS for mesh");
    };

    commandList->buildBottomLevelAccelStruct(blas, &geometry, 1, desc.buildFlags);

    return blas;
  }

  void RayTracingScene::CollectRayTracingInstances(
    const ModelNode& node,
    const glm::mat4& parentTransform,
    const std::vector<nvrhi::rt::AccelStructHandle>& blases,
    std::vector<nvrhi::rt::InstanceDesc>& instances)
  {
    const glm::mat4 world =
      parentTransform * node.localTransform;

    for (const MeshInstanceData& mesh : node.meshInstances) {
      nvrhi::rt::InstanceDesc instance{};

      for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 4; ++column) {
          instance.transform[row * 4 + column] = world[column][row];
        }
      }

      instance.bottomLevelAS = blases.at(mesh.meshIndex).Get();
      instance.instanceMask = 0xff;
      instance.instanceID =
        static_cast<uint32_t>(instances.size());

      instances.push_back(instance);
    }

    for (const ModelNode& child : node.children) {
      CollectRayTracingInstances(
        child, world, blases, instances);
    }
  }
}