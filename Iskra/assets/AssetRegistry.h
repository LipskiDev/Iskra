#pragma once

#include <assets/MaterialData.h>
#include <assets/MeshData.h>
#include <assets/ModelData.h>
#include <assets/TextureData.h>
#include <cstdint>
#include <nvrhi/nvrhi.h>
#include <assets/asset_handle.h>
#include <graphics/GpuData.h>
#include <cstddef>
#include <filesystem>
#include <assimp/mesh.h>
#include <assimp/material.h>
#include <assimp/texture.h>
#include <assimp/scene.h>
#include <span>
#include <unordered_map>
#include <vector>

namespace Iskra {
using TextureHandle = nvrhi::TextureHandle;

class AssetRegistry {
 public:
  void Initialize(nvrhi::IDevice *device);
  void UpdateResourcesIfDirty();
  void Shutdown();

  std::span<const Material> GetMaterials() const { return materials_; }

  // CPU Side: Load model data from disk and store it in the registry
  ModelHandle LoadModel(const std::filesystem::path &path);
  Texture LoadTexture(const std::filesystem::path &path);
  Material LoadMaterial(const std::filesystem::path &path);

  MeshHandle AddMesh(MeshData mesh);
  MaterialHandle AddMaterial(Material material);

  const MeshData &GetMesh(MeshHandle handle) const;
  const GpuMesh &GetGpuMesh(MeshHandle handle) const;
  Material &GetMaterial(MaterialHandle handle);
  const Material &GetMaterial(MaterialHandle handle) const;
  const ModelData &GetModel(ModelHandle handle) const;
  size_t GetMeshCount() const { return meshes_.size(); }

  void UpdateMaterial(MaterialHandle handle, const Material &material);

  // GPU Side: Upload data to GPU and return handles for the resources
  nvrhi::BindingLayoutHandle GetBindlessTextureBindingLayout();
  nvrhi::DescriptorTableHandle GetBindlessTextureDescriptorTable();
  nvrhi::BufferHandle GetMaterialBuffer() const { return materialBuffer_; }

  bool materialsDirty_ = false;
  bool meshesDirty_ = false;
  bool m_TexturesDirty = false;

 private:
  MeshData ConvertMesh(const aiMesh &mesh);
  Material ConvertMaterial(const aiMaterial &material, const aiScene &scene,
                           const std::filesystem::path &modelPath);
  int32_t ResolveTexture(const aiString &reference, const aiScene &scene,
                         const std::filesystem::path &modelPath);
  Texture ConvertTexture(const aiTexture &texture);
  ModelNode ConvertNode(const aiNode &node, const aiScene &scene,
                        std::span<const MeshHandle> meshHandles,
                        std::span<const MaterialHandle> materialHandles);

  void UploadMaterials();
  void UploadTextures();
  void UploadMeshes();

 private:
  uint32_t k_MaxMaterials = 1024;
  uint32_t k_MaxTextures = 4 * 1024;

  // CPU Side
  struct MeshAsset {
    MeshData data;
    GpuMesh gpu;
    bool gpuDirty = true;
  };

  std::vector<MeshAsset> meshes_;
  std::vector<Material> materials_;
  std::vector<ModelData> models_;
  std::vector<Texture> m_Textures;
  std::unordered_map<std::string, int32_t> m_TextureIndices;

  // GPU Side
  nvrhi::IDevice *m_Device;
  nvrhi::BufferHandle materialBuffer_;

  nvrhi::SamplerHandle m_DefaultSampler;

  // Bindless
  nvrhi::BindingLayoutHandle m_TexturesBindingLayout;
  nvrhi::DescriptorTableHandle m_DescriptorTable;
};
}  // namespace Iskra
