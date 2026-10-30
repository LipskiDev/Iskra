#pragma once

#include <nvrhi/nvrhi.h>
#include <assets/ModelData.h>
#include <graphics/GpuMesh.h>

namespace Iskra {
  class RayTracingScene {
  public:
    void Build(nvrhi::IDevice* device, 
      nvrhi::ICommandList* commandList, 
      const ModelData& model, 
      const std::vector<GpuMesh>& gpuMeshes, 
      const glm::mat4& placemnt = glm::mat4(1.0));

    nvrhi::rt::IAccelStruct* GetTLAS() const {
      return m_TLAS.Get();
    }

  private:
    std::vector<nvrhi::rt::AccelStructHandle> m_BLASes;
    nvrhi::rt::AccelStructHandle m_TLAS;

    nvrhi::rt::AccelStructHandle BuildBLAS(nvrhi::IDevice* device,
      nvrhi::ICommandList* commandList,
      const GpuMesh& mesh);

    void CollectRayTracingInstances(const ModelNode& node, 
      const glm::mat4& parentTransform,
      const std::vector<nvrhi::rt::AccelStructHandle>& blases,
      std::vector<nvrhi::rt::InstanceDesc>& instances);

  };
}