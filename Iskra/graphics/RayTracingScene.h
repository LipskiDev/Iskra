#pragma once

#include <nvrhi/nvrhi.h>
#include <assets/AssetRegistry.h>
#include <graphics/render_instance.h>

#include <span>

namespace Iskra {
class RayTracingScene {
 public:
  void Build(nvrhi::IDevice *device, nvrhi::ICommandList *commandList,
             std::span<const RenderInstance> instances,
             const AssetRegistry &assets);

  nvrhi::rt::IAccelStruct *GetTLAS() const { return m_TLAS.Get(); }

 private:
  std::vector<nvrhi::rt::AccelStructHandle> m_BLASes;
  nvrhi::rt::AccelStructHandle m_TLAS;

  nvrhi::rt::AccelStructHandle BuildBLAS(nvrhi::IDevice *device,
                                         nvrhi::ICommandList *commandList,
                                         const GpuMesh &mesh);
};
}  // namespace Iskra
