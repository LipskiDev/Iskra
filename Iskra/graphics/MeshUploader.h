#pragma once

#include <graphics/GpuMesh.h>
#include <assets/MeshData.h>

namespace Iskra {
  GpuMesh UploadMesh(nvrhi::IDevice* device, nvrhi::ICommandList* commandList, const MeshData& meshData);
}