#include <graphics/MeshUploader.h>

#include <stdexcept>

namespace Iskra {
  GpuMesh UploadMesh(nvrhi::IDevice* device, nvrhi::ICommandList* commandList, const MeshData& meshData)
  {
    if (meshData.vertices.empty() || meshData.indices.empty()) {
      throw std::runtime_error("Cannot upload empty mesh data");
    }

    if (meshData.indices.size() > std::numeric_limits<uint32_t>::max()) {
      throw std::runtime_error("Mesh has too many indices to be uploaded as 32-bit unsigned integers");
    }

    nvrhi::BufferDesc vertexDesc{};
    vertexDesc.byteSize = sizeof(Vertex) * meshData.vertices.size();
    vertexDesc.isVertexBuffer = true;
    vertexDesc.initialState = nvrhi::ResourceStates::VertexBuffer;
    vertexDesc.keepInitialState = true;
    vertexDesc.isAccelStructBuildInput = true;
    vertexDesc.debugName = "Mesh Vertex Buffer";

    nvrhi::BufferDesc indexDesc{};
    indexDesc.byteSize = sizeof(uint32_t) * meshData.indices.size();
    indexDesc.isIndexBuffer = true;
    indexDesc.initialState = nvrhi::ResourceStates::IndexBuffer;
    indexDesc.keepInitialState = true;
    indexDesc.isAccelStructBuildInput = true;
    indexDesc.debugName = "Mesh Index Buffer";

    GpuMesh result{};
    result.vertexBuffer = device->createBuffer(vertexDesc);
    result.indexBuffer = device->createBuffer(indexDesc);
    result.vertexCount = static_cast<uint32_t>(meshData.vertices.size());
    result.indexCount = static_cast<uint32_t>(meshData.indices.size());

    if (!result.vertexBuffer || !result.indexBuffer) {
      throw std::runtime_error("Failed to create GPU buffers for mesh");
    }

    commandList->writeBuffer(result.vertexBuffer, meshData.vertices.data(), vertexDesc.byteSize);
    commandList->writeBuffer(result.indexBuffer, meshData.indices.data(), indexDesc.byteSize);

    return result;
  }

  
}