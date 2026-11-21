#pragma once

#include <graphics/GpuData.h>
#include <assets/MeshData.h>
#include <assets/MaterialData.h>
#include <assets/TextureData.h>

namespace Iskra {
GpuMesh UploadMesh(nvrhi::IDevice *device, nvrhi::ICommandList *commandList,
                   const MeshData &meshData);

GpuMaterial UploadMaterial(nvrhi::IDevice *device,
                           nvrhi::ICommandList *commandList,
                           const Material &materialData);

GpuTexture UploadTexture(nvrhi::IDevice *device,
                         nvrhi::ICommandList *commandList,
                         const Texture &textureData);
}  // namespace Iskra