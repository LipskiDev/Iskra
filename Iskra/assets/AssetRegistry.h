#pragma once

#include <cstdint>
#include <nvrhi/nvrhi.h>
#include "ModelData.h"
#include <filesystem>
#include <assimp/mesh.h>
#include <assimp/material.h>
#include <assimp/texture.h>
#include <assimp/scene.h>
#include <unordered_map>

namespace Iskra {
using AssetHandle = uint32_t;
using TextureHandle = nvrhi::TextureHandle;
using MaterialHandle = uint32_t;

class AssetRegistry {
 public:
  void Initialize(nvrhi::IDevice *device, nvrhi::ICommandList *commandList);
  void UpdateResourcesIfDirty();
  void Shutdown();

  // CPU Side: Load model data from disk and store it in the registry
  ModelData LoadModel(const std::filesystem::path &path);
  Texture LoadTexture(const std::filesystem::path &path);
  Material LoadMaterial(const std::filesystem::path &path);

  // GPU Side: Upload data to GPU and return handles for the resources
  nvrhi::BufferHandle GetVertexBuffer();
  nvrhi::BufferHandle GetIndexBuffer();
  nvrhi::BindingLayoutHandle GetBindlessTextureBindingLayout();
  nvrhi::DescriptorTableHandle GetBindlessTextureDescriptorTable();
  nvrhi::BufferHandle GetMaterialBuffer() { return m_MaterialBuffer; }

 private:
  MeshData ConvertMesh(const aiMesh &mesh);
  Material ConvertMaterial(const aiMaterial &material, const aiScene &scene,
                           const std::filesystem::path &modelPath);
  int32_t ResolveTexture(const aiString &reference, const aiScene &scene,
                         const std::filesystem::path &modelPath);
  Texture ConvertTexture(const aiTexture &texture);
  ModelNode ConvertNode(const aiNode &node, const aiScene &scene);

  void UploadMaterials();
  void UploadTextures();
  void UploadModels();

 private:
  uint32_t k_MaxMaterials = 1024;
  uint32_t k_MaxTextures = 4 * 1024;

  // CPU Side
  std::vector<Material> m_Materials;
  std::vector<ModelData> m_Models;
  std::vector<Texture> m_Textures;
  std::unordered_map<std::string, int32_t> m_TextureIndices;
  bool m_MaterialsDirty = false;
  bool m_ModelsDirty = false;
  bool m_TexturesDirty = false;

  // GPU Side
  nvrhi::BufferHandle m_VertexBuffer;
  nvrhi::BufferHandle m_IndexBuffer;

  nvrhi::IDevice *m_Device;
  nvrhi::ICommandList *m_CommandList;

  nvrhi::BufferHandle m_MaterialBuffer;

  nvrhi::SamplerHandle m_DefaultSampler;

  // Bindless
  nvrhi::BindingLayoutHandle m_TexturesBindingLayout;
  nvrhi::DescriptorTableHandle m_DescriptorTable;
};
}  // namespace Iskra
