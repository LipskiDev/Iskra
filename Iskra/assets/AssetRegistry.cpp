#include <assets/AssetRegistry.h>
#include <graphics/MeshUploader.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/GltfMaterial.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../../external/assimp/contrib/stb/stb_image.h"

#include <stdexcept>
#include <string>
#include <limits>
#include <memory>

namespace {
Iskra::Texture DecodeTexture(const stbi_uc *encoded, int byteCount,
                             const std::string &name) {
  int width = 0, height = 0, channels = 0;
  std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
      stbi_load_from_memory(encoded, byteCount, &width, &height, &channels,
                            STBI_rgb_alpha),
      stbi_image_free);
  if (!pixels) {
    throw std::runtime_error("Failed to decode texture: " + name);
  }
  Iskra::Texture result;
  result.name = name;
  result.width = static_cast<uint32_t>(width);
  result.height = static_cast<uint32_t>(height);
  result.data.assign(pixels.get(),
                     pixels.get() + static_cast<size_t>(width) * height * 4);
  return result;
}

glm::mat4 ConvertMatrix(const aiMatrix4x4 &assimpMatrix) {
  glm::mat4 glmMatrix;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      glmMatrix[j][i] = assimpMatrix[i][j];
    }
  }
  return glmMatrix;
}
}  // namespace

namespace Iskra {
void AssetRegistry::Initialize(nvrhi::IDevice *device) {
  m_Device = device;

  nvrhi::BindlessLayoutDesc layoutDesc;
  layoutDesc.maxCapacity = k_MaxTextures;
  layoutDesc.visibility = nvrhi::ShaderType::All;
  layoutDesc.registerSpaces = {nvrhi::BindingLayoutItem::Texture_SRV(2)};

  m_TexturesBindingLayout = m_Device->createBindlessLayout(layoutDesc);

  m_DescriptorTable = m_Device->createDescriptorTable(m_TexturesBindingLayout);
  m_Device->resizeDescriptorTable(m_DescriptorTable, k_MaxTextures);

  std::vector<uint8_t> whiteFallback = {255, 255, 255, 255};
  Texture whiteTexture;
  whiteTexture.name = "white_fallback";
  whiteTexture.width = 1;
  whiteTexture.height = 1;
  whiteTexture.data.resize(4);
  memcpy(whiteTexture.data.data(), whiteFallback.data(), 4 * sizeof(uint8_t));
  whiteTexture.gpuTextureIndex = static_cast<uint32_t>(m_Textures.size());
  m_Textures.push_back(std::move(whiteTexture));
  m_TexturesDirty = true;
}

void AssetRegistry::UpdateResourcesIfDirty() {
  if (materialsDirty_) {
    UploadMaterials();
    materialsDirty_ = false;
  }

  if (m_TexturesDirty) {
    UploadTextures();
    m_TexturesDirty = false;
  }

  if (meshesDirty_) {
    UploadMeshes();
    meshesDirty_ = false;
  }
}

void AssetRegistry::Shutdown() {}

ModelHandle AssetRegistry::LoadModel(const std::filesystem::path &path) {
  Assimp::Importer importer;

  const unsigned int flags = aiProcess_Triangulate |
                             aiProcess_JoinIdenticalVertices |
                             aiProcess_GenSmoothNormals |
                             aiProcess_SortByPType | aiProcess_CalcTangentSpace;

  const aiScene *scene = importer.ReadFile(path.string(), flags);

  if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) ||
      !scene->mRootNode) {
    throw std::runtime_error("Failed to import model: " + path.string());
  }

  ModelData model;
  std::vector<MaterialHandle> materialHandles;
  materialHandles.reserve(scene->mNumMaterials);

  for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
    materialHandles.push_back(
        AddMaterial(ConvertMaterial(*scene->mMaterials[i], *scene, path)));
  }

  std::vector<MeshHandle> meshHandles;
  meshHandles.reserve(scene->mNumMeshes);
  for (uint32_t i = 0; i < scene->mNumMeshes; ++i) {
    meshHandles.push_back(AddMesh(ConvertMesh(*scene->mMeshes[i])));
  }

  model.root =
      ConvertNode(*scene->mRootNode, *scene, meshHandles, materialHandles);

  if (models_.size() >= ModelHandle::InvalidIndex) {
    throw std::runtime_error("Model registry capacity exceeded");
  }

  const ModelHandle handle(static_cast<uint32_t>(models_.size()));
  models_.push_back(std::move(model));
  return handle;
}

Texture AssetRegistry::LoadTexture(const std::filesystem::path &path) {
  int width = 0, height = 0, channels = 0;
  std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
      stbi_load(path.string().c_str(), &width, &height, &channels,
                STBI_rgb_alpha),
      stbi_image_free);
  if (!pixels) {
    throw std::runtime_error("Failed to load texture: " + path.string());
  }
  Texture result;
  result.name = path.string();
  result.width = static_cast<uint32_t>(width);
  result.height = static_cast<uint32_t>(height);
  result.data.assign(pixels.get(),
                     pixels.get() + static_cast<size_t>(width) * height * 4);
  return result;
}

Material AssetRegistry::LoadMaterial(const std::filesystem::path &path) {
  // are there even material file formats?
  return Material();
}

MeshHandle AssetRegistry::AddMesh(MeshData mesh) {
  if (meshes_.size() >= MeshHandle::InvalidIndex) {
    throw std::runtime_error("Mesh registry capacity exceeded");
  }

  const MeshHandle handle(static_cast<uint32_t>(meshes_.size()));
  meshes_.push_back({.data = std::move(mesh)});
  meshesDirty_ = true;
  return handle;
}

MaterialHandle AssetRegistry::AddMaterial(Material material) {
  if (materials_.size() >= k_MaxMaterials) {
    throw std::runtime_error("Material registry capacity exceeded");
  }

  const MaterialHandle handle(static_cast<uint32_t>(materials_.size()));
  materials_.push_back(std::move(material));
  materialsDirty_ = true;
  return handle;
}

const MeshData &AssetRegistry::GetMesh(MeshHandle handle) const {
  if (!handle) {
    throw std::invalid_argument("Invalid mesh handle");
  }
  return meshes_.at(handle.GetIndex()).data;
}

const GpuMesh &AssetRegistry::GetGpuMesh(MeshHandle handle) const {
  if (!handle) {
    throw std::invalid_argument("Invalid mesh handle");
  }

  const MeshAsset &mesh = meshes_.at(handle.GetIndex());
  if (mesh.gpuDirty || !mesh.gpu.vertexBuffer || !mesh.gpu.indexBuffer) {
    throw std::runtime_error("Mesh has not been uploaded");
  }
  return mesh.gpu;
}

Material &AssetRegistry::GetMaterial(MaterialHandle handle) {
  if (!handle) {
    throw std::invalid_argument("Invalid material handle");
  }
  return materials_.at(handle.GetIndex());
}

const Material &AssetRegistry::GetMaterial(MaterialHandle handle) const {
  if (!handle) {
    throw std::invalid_argument("Invalid material handle");
  }
  return materials_.at(handle.GetIndex());
}

const ModelData &AssetRegistry::GetModel(ModelHandle handle) const {
  if (!handle) {
    throw std::invalid_argument("Invalid model handle");
  }
  return models_.at(handle.GetIndex());
}

void AssetRegistry::UpdateMaterial(MaterialHandle handle,
                                   const Material &material) {
  if (!handle) {
    throw std::invalid_argument("Invalid material handle");
  }
  materials_.at(handle.GetIndex()) = material;
  materialsDirty_ = true;
}

nvrhi::BindingLayoutHandle AssetRegistry::GetBindlessTextureBindingLayout() {
  return m_TexturesBindingLayout;
}

nvrhi::DescriptorTableHandle
AssetRegistry::GetBindlessTextureDescriptorTable() {
  return m_DescriptorTable;
}

MeshData AssetRegistry::ConvertMesh(const aiMesh &mesh) {
  MeshData meshData;
  meshData.vertices.reserve(mesh.mNumVertices);
  meshData.indices.reserve(static_cast<size_t>(mesh.mNumFaces) * 3);

  for (unsigned int i = 0; i < mesh.mNumVertices; ++i) {
    Vertex vertex{};
    vertex.position = glm::vec3(mesh.mVertices[i].x, mesh.mVertices[i].y,
                                mesh.mVertices[i].z);

    if (mesh.HasNormals()) {
      vertex.normal =
          glm::vec3(mesh.mNormals[i].x, mesh.mNormals[i].y, mesh.mNormals[i].z);
    }

    if (mesh.HasTextureCoords(0)) {
      const auto &uv = mesh.mTextureCoords[0][i];
      vertex.uv = {uv.x, 1.0 - uv.y};
    }

    if (mesh.HasTangentsAndBitangents()) {
      vertex.tangent = glm::vec3(mesh.mTangents[i].x, mesh.mTangents[i].y,
                                 mesh.mTangents[i].z);
      vertex.bitangent = glm::vec3(mesh.mBitangents[i].x, mesh.mBitangents[i].y,
                                   mesh.mBitangents[i].z);
    }

    meshData.vertices.push_back(vertex);
  }

  for (unsigned int i = 0; i < mesh.mNumFaces; ++i) {
    const aiFace &face = mesh.mFaces[i];
    if (face.mNumIndices != 3) {
      throw std::runtime_error("Non-triangular face found in mesh");
    }
    meshData.indices.push_back(face.mIndices[0]);
    meshData.indices.push_back(face.mIndices[1]);
    meshData.indices.push_back(face.mIndices[2]);
  }

  return meshData;
}

int32_t AssetRegistry::ResolveTexture(const aiString &reference,
                                      const aiScene &scene,
                                      const std::filesystem::path &modelPath) {
  const aiTexture *embedded = scene.GetEmbeddedTexture(reference.C_Str());
  const auto path =
      std::filesystem::absolute(modelPath.parent_path() / reference.C_Str())
          .lexically_normal();
  const std::string key =
      embedded
          ? std::filesystem::absolute(modelPath).lexically_normal().string() +
                "::" + reference.C_Str()
          : path.string();
  const auto found = m_TextureIndices.find(key);
  if (found != m_TextureIndices.end()) {
    return found->second;
  }
  if (m_Textures.size() >= k_MaxTextures) {
    throw std::runtime_error("Texture registry capacity exceeded");
  }
  Texture texture = embedded ? ConvertTexture(*embedded) : LoadTexture(path);
  const auto index = static_cast<int32_t>(m_Textures.size());
  texture.gpuTextureIndex = index;
  m_Textures.push_back(std::move(texture));
  m_TextureIndices.emplace(key, index);
  m_TexturesDirty = true;
  return index;
}

Material AssetRegistry::ConvertMaterial(
    const aiMaterial &material, const aiScene &scene,
    const std::filesystem::path &modelPath) {
  Material mat;
  aiString name;
  aiColor4D color;
  if (material.Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS ||
      material.Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
    mat.baseColorFactor = {color.r, color.g, color.b, color.a};
  }
  float factor;
  if (material.Get(AI_MATKEY_METALLIC_FACTOR, factor) == AI_SUCCESS) {
    mat.metallicFactor = factor;
  }
  if (material.Get(AI_MATKEY_ROUGHNESS_FACTOR, factor) == AI_SUCCESS) {
    mat.roughnessFactor = factor;
  }
  const auto textureIndex = [&](aiTextureType type,
                                aiTextureType fallback) -> int32_t {
    aiString reference;
    if (material.GetTexture(type, 0, &reference) != AI_SUCCESS &&
        material.GetTexture(fallback, 0, &reference) != AI_SUCCESS) {
      return 0;
    }
    return ResolveTexture(reference, scene, modelPath);
  };
  mat.baseColorTextureIndex =
      textureIndex(aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE);
  mat.normalTextureIndex =
      textureIndex(aiTextureType_NORMALS, aiTextureType_NORMALS);
  mat.occlusionTextureIndex =
      textureIndex(aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP);
  mat.emissiveTextureIndex =
      textureIndex(aiTextureType_EMISSION_COLOR, aiTextureType_EMISSIVE);
  mat.metallicRoughnessTextureIndex =
      textureIndex(aiTextureType_GLTF_METALLIC_ROUGHNESS,
                   aiTextureType_GLTF_METALLIC_ROUGHNESS);

  return mat;
}

Texture AssetRegistry::ConvertTexture(const aiTexture &texture) {
  const std::string name = texture.mFilename.C_Str();
  if (!texture.pcData || texture.mWidth == 0) {
    throw std::runtime_error("Empty embedded texture: " + name);
  }
  if (texture.mHeight == 0) {
    if (texture.mWidth >
        static_cast<unsigned int>(std::numeric_limits<int>::max())) {
      throw std::runtime_error("Embedded texture is too large to decode: " +
                               name);
    }
    return DecodeTexture(reinterpret_cast<const stbi_uc *>(texture.pcData),
                         static_cast<int>(texture.mWidth), name);
  }
  Texture tex;
  tex.name = name;
  tex.width = texture.mWidth;
  tex.height = texture.mHeight;
  tex.gpuTextureIndex = static_cast<uint32_t>(m_Textures.size());
  const size_t pixelCount = static_cast<size_t>(tex.width) * tex.height;
  if (pixelCount > tex.data.max_size() / 4) {
    throw std::runtime_error("Embedded texture is too large: " + name);
  }
  tex.data.resize(pixelCount * 4);
  for (size_t i = 0; i < pixelCount; ++i) {
    tex.data[i * 4] = texture.pcData[i].r;
    tex.data[i * 4 + 1] = texture.pcData[i].g;
    tex.data[i * 4 + 2] = texture.pcData[i].b;
    tex.data[i * 4 + 3] = texture.pcData[i].a;
  }
  return tex;
}

ModelNode AssetRegistry::ConvertNode(
    const aiNode &node, const aiScene &scene,
    std::span<const MeshHandle> meshHandles,
    std::span<const MaterialHandle> materialHandles) {
  ModelNode result;
  result.name = node.mName.C_Str();
  result.localTransform = ConvertMatrix(node.mTransformation);
  result.meshInstances.reserve(node.mNumMeshes);

  for (unsigned int i = 0; i < node.mNumMeshes; ++i) {
    const uint32_t meshIndex = node.mMeshes[i];
    const uint32_t materialIndex = scene.mMeshes[meshIndex]->mMaterialIndex;
    result.meshInstances.push_back(
        {.mesh = meshHandles[meshIndex],
         .material = materialHandles[materialIndex]});
  }

  result.children.reserve(node.mNumChildren);
  for (unsigned int i = 0; i < node.mNumChildren; ++i) {
    result.children.push_back(
        ConvertNode(*node.mChildren[i], scene, meshHandles, materialHandles));
  }

  return result;
}

void AssetRegistry::UploadMaterials() {
  materialBuffer_ = m_Device->createBuffer({
      .byteSize = static_cast<uint32_t>(materials_.size() * sizeof(Material)),
      .structStride = sizeof(Material),
      .debugName = "Material Buffer",
      .isConstantBuffer = true,
      .initialState = nvrhi::ResourceStates::ShaderResource,
      .keepInitialState = true,
  });

  auto upload = m_Device->createCommandList();
  upload->open();
  upload->writeBuffer(materialBuffer_, materials_.data(),
                      materials_.size() * sizeof(Material));
  upload->close();

  m_Device->executeCommandList(upload);
}

void AssetRegistry::UploadTextures() {
  for (Texture &texture : m_Textures) {
    if (!texture.gpuTexture) {
      nvrhi::TextureDesc desc;
      desc.width = texture.width;
      desc.height = texture.height;
      desc.format = nvrhi::Format::RGBA8_UNORM;
      desc.debugName = texture.name.c_str();
      desc.initialState = nvrhi::ResourceStates::ShaderResource;
      desc.keepInitialState = true;
      texture.gpuTexture = m_Device->createTexture(desc);
      auto upload = m_Device->createCommandList();
      upload->open();
      upload->writeTexture(texture.gpuTexture, 0, 0, texture.data.data(), 0);
      upload->close();

      m_Device->executeCommandList(upload);

      m_Device->writeDescriptorTable(
          m_DescriptorTable, nvrhi::BindingSetItem::Texture_SRV(
                                 texture.gpuTextureIndex, texture.gpuTexture));
    }
  }
}

void AssetRegistry::UploadMeshes() {
  auto commandList = m_Device->createCommandList();
  commandList->open();

  for (MeshAsset &mesh : meshes_) {
    if (!mesh.gpuDirty) {
      continue;
    }
    mesh.gpu = UploadMesh(m_Device, commandList, mesh.data);
    mesh.gpuDirty = false;
  }

  commandList->close();
  m_Device->executeCommandList(commandList);
}
}  // namespace Iskra
