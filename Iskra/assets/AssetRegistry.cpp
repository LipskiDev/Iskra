#include <assets/AssetRegistry.h>
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
  Iskra::Texture DecodeTexture(const stbi_uc* encoded, int byteCount, const std::string& name)
  {
    int width = 0, height = 0, channels = 0;
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
      stbi_load_from_memory(encoded, byteCount, &width, &height, &channels, STBI_rgb_alpha),
      stbi_image_free);
    if (!pixels) {
      throw std::runtime_error("Failed to decode texture: " + name);
    }
    Iskra::Texture result;
    result.name = name;
    result.width = static_cast<uint32_t>(width);
    result.height = static_cast<uint32_t>(height);
    result.data.assign(pixels.get(), pixels.get() + static_cast<size_t>(width) * height * 4);
    return result;
  }

  glm::mat4 ConvertMatrix(const aiMatrix4x4& assimpMatrix)
  {
    glm::mat4 glmMatrix;
    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 4; ++j) {
        glmMatrix[j][i] = assimpMatrix[i][j];
      }
    }
    return glmMatrix;
  }
}

namespace Iskra {
  void AssetRegistry::Initialize(nvrhi::IDevice* device, nvrhi::ICommandList* commandList)
  {
    m_Device = device;
    m_CommandList = commandList;

    nvrhi::BindlessLayoutDesc layoutDesc;
    layoutDesc.maxCapacity = k_MaxTextures;
    layoutDesc.visibility = nvrhi::ShaderType::All;
    layoutDesc.registerSpaces = {
      nvrhi::BindingLayoutItem::Texture_SRV(2)
    };

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

  void AssetRegistry::UpdateResourcesIfDirty()
  {
    if (m_MaterialsDirty) {
      UploadMaterials();
      m_MaterialsDirty = false;
    }

    if (m_TexturesDirty) {
      UploadTextures();
      m_TexturesDirty = false;
    }

    if (m_ModelsDirty) {
      UploadModels();
      m_ModelsDirty = false;
    }
  }

  void AssetRegistry::Shutdown()
  {
  }

  ModelData AssetRegistry::LoadModel(const std::filesystem::path& path)
  {
    Assimp::Importer importer;

    const unsigned int flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals | aiProcess_SortByPType | aiProcess_CalcTangentSpace;

    const aiScene* scene = importer.ReadFile(path.string(), flags);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
      throw std::runtime_error("Failed to import model: " + path.string());
    }

    ModelData model;
    model.meshes.reserve(scene->mNumMeshes);

    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
      MeshData mesh = ConvertMesh(*scene->mMeshes[i]);
      model.meshes.push_back(mesh);
    }

    for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
      Material material = ConvertMaterial(*scene->mMaterials[i], *scene, path);
      model.materials.push_back(material);
      m_Materials.push_back(material);
      m_MaterialsDirty = true;
    }

    model.root = ConvertNode(*scene->mRootNode, *scene);

    return model;
  }

  Texture AssetRegistry::LoadTexture(const std::filesystem::path& path)
  {
    int width = 0, height = 0, channels = 0;
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
      stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha),
      stbi_image_free);
    if (!pixels) {
      throw std::runtime_error("Failed to load texture: " + path.string());
    }
    Texture result;
    result.name = path.string();
    result.width = static_cast<uint32_t>(width);
    result.height = static_cast<uint32_t>(height);
    result.data.assign(pixels.get(), pixels.get() + static_cast<size_t>(width) * height * 4);
    return result;
  }

  Material AssetRegistry::LoadMaterial(const std::filesystem::path& path)
  {
    // are there even material file formats?
    return Material();
  }

  nvrhi::BufferHandle AssetRegistry::GetVertexBuffer()
  {
    return m_VertexBuffer;
  }

  nvrhi::BufferHandle AssetRegistry::GetIndexBuffer()
  {
    return m_IndexBuffer;
  }

  nvrhi::BindingLayoutHandle AssetRegistry::GetBindlessTextureBindingLayout()
  {
    return m_TexturesBindingLayout;
  }

  nvrhi::DescriptorTableHandle AssetRegistry::GetBindlessTextureDescriptorTable()
  {
    return m_DescriptorTable;
  }


  MeshData AssetRegistry::ConvertMesh(const aiMesh& mesh)
  {
    MeshData meshData;
    meshData.vertices.reserve(mesh.mNumVertices);
    meshData.indices.reserve(static_cast<size_t>(mesh.mNumFaces) * 3);

    for (unsigned int i = 0; i < mesh.mNumVertices; ++i) {
      Vertex vertex{};
      vertex.position = glm::vec3(mesh.mVertices[i].x, mesh.mVertices[i].y, mesh.mVertices[i].z);

      if (mesh.HasNormals()) {
        vertex.normal = glm::vec3(mesh.mNormals[i].x, mesh.mNormals[i].y, mesh.mNormals[i].z);
      }

      if (mesh.HasTextureCoords(0)) {
        const auto& uv = mesh.mTextureCoords[0][i];
        vertex.uv = { uv.x, 1.0 - uv.y };
      }

	  if (mesh.HasTangentsAndBitangents()) {
		  vertex.tangent = glm::vec3(mesh.mTangents[i].x, mesh.mTangents[i].y, mesh.mTangents[i].z);
		  vertex.bitangent = glm::vec3(mesh.mBitangents[i].x, mesh.mBitangents[i].y, mesh.mBitangents[i].z);
	  }
      
      meshData.vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < mesh.mNumFaces; ++i) {
      const aiFace& face = mesh.mFaces[i];
      if (face.mNumIndices != 3) {
        throw std::runtime_error("Non-triangular face found in mesh");
      }
      meshData.indices.push_back(face.mIndices[0]);
      meshData.indices.push_back(face.mIndices[1]);
      meshData.indices.push_back(face.mIndices[2]);
    }

    return meshData;
  }

  int32_t AssetRegistry::ResolveTexture(const aiString& reference, const aiScene& scene,
    const std::filesystem::path& modelPath)
  {
    const aiTexture* embedded = scene.GetEmbeddedTexture(reference.C_Str());
    const auto path = std::filesystem::absolute(modelPath.parent_path() / reference.C_Str()).lexically_normal();
    const std::string key = embedded
      ? std::filesystem::absolute(modelPath).lexically_normal().string() + "::" + reference.C_Str()
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

  Material AssetRegistry::ConvertMaterial(const aiMaterial& material, const aiScene& scene,
    const std::filesystem::path& modelPath)
  {
    Material mat;
    aiString name;
    aiColor4D color;
    if (material.Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS ||
        material.Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
      mat.baseColorFactor = { color.r, color.g, color.b, color.a };
    }
    float factor;
    if (material.Get(AI_MATKEY_METALLIC_FACTOR, factor) == AI_SUCCESS) {
      mat.metallicFactor = factor;
    }
    if (material.Get(AI_MATKEY_ROUGHNESS_FACTOR, factor) == AI_SUCCESS) {
      mat.roughnessFactor = factor;
    }
    const auto textureIndex = [&](aiTextureType type, aiTextureType fallback) -> int32_t {
      aiString reference;
      if (material.GetTexture(type, 0, &reference) != AI_SUCCESS &&
          material.GetTexture(fallback, 0, &reference) != AI_SUCCESS) {
        return 0;
      }
      return ResolveTexture(reference, scene, modelPath);
    };
    mat.baseColorTextureIndex = textureIndex(aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE);
    mat.normalTextureIndex = textureIndex(aiTextureType_NORMALS, aiTextureType_NORMALS);
    mat.occlusionTextureIndex = textureIndex(aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP);
    mat.emissiveTextureIndex = textureIndex(aiTextureType_EMISSION_COLOR, aiTextureType_EMISSIVE);
    mat.metallicRoughnessTextureIndex = textureIndex(
      aiTextureType_GLTF_METALLIC_ROUGHNESS, aiTextureType_GLTF_METALLIC_ROUGHNESS);

    return mat;
  }

  Texture AssetRegistry::ConvertTexture(const aiTexture& texture)
  {
    const std::string name = texture.mFilename.C_Str();
    if (!texture.pcData || texture.mWidth == 0) {
      throw std::runtime_error("Empty embedded texture: " + name);
    }
    if (texture.mHeight == 0) {
      if (texture.mWidth > static_cast<unsigned int>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("Embedded texture is too large to decode: " + name);
      }
      return DecodeTexture(reinterpret_cast<const stbi_uc*>(texture.pcData),
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

  ModelNode AssetRegistry::ConvertNode(const aiNode& node, const aiScene& scene)
  {
    ModelNode result;
    result.name = node.mName.C_Str();
    result.localTransform = ConvertMatrix(node.mTransformation);
    result.meshInstances.reserve(node.mNumMeshes);

    for (unsigned int i = 0; i < node.mNumMeshes; ++i) {
      MeshInstanceData meshInstance;
      meshInstance.meshIndex = node.mMeshes[i];
      meshInstance.materialIndex = static_cast<int32_t>(scene.mMeshes[meshInstance.meshIndex]->mMaterialIndex);
      result.meshInstances.push_back(meshInstance);
    }

    result.children.reserve(node.mNumChildren);
    for (unsigned int i = 0; i < node.mNumChildren; ++i) {
      result.children.push_back(ConvertNode(*node.mChildren[i], scene));
    }

    return result;
  }

  void AssetRegistry::UploadMaterials()
  {
    m_MaterialBuffer = m_Device->createBuffer({
      .byteSize = static_cast<uint32_t>(m_Materials.size() * sizeof(Material)),
      .structStride = sizeof(Material),
      .debugName = "Material Buffer",
      .isConstantBuffer = true,
      .initialState = nvrhi::ResourceStates::ShaderResource,
      .keepInitialState = true,
    });

    auto upload = m_Device->createCommandList();
    upload->open();
    upload->writeBuffer(m_MaterialBuffer, m_Materials.data(), m_Materials.size() * sizeof(Material));
    upload->close();

    m_Device->executeCommandList(upload);
  }

  void AssetRegistry::UploadTextures()
  {
    for (Texture& texture : m_Textures) {
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

        m_Device->writeDescriptorTable(m_DescriptorTable, nvrhi::BindingSetItem::Texture_SRV(texture.gpuTextureIndex, texture.gpuTexture));
      }
    }
  }

  void AssetRegistry::UploadModels()
  {
    uint32_t totalVertexCount = 0;
    uint32_t totalIndexCount = 0;

    std::vector<Vertex> allVertices;
    std::vector<uint32_t> allIndices;

    for (ModelData& model : m_Models) {
      for (MeshData& mesh : model.meshes) {
        totalVertexCount += static_cast<uint32_t>(mesh.vertices.size());
        totalIndexCount += static_cast<uint32_t>(mesh.indices.size());

        allVertices.insert(allVertices.end(), mesh.vertices.begin(), mesh.vertices.end());
        allIndices.insert(allIndices.end(), mesh.indices.begin(), mesh.indices.end());
      }
    }

    m_VertexBuffer = m_Device->createBuffer({
      .byteSize = totalVertexCount * sizeof(Vertex),
      .structStride = sizeof(Vertex),
      .debugName = "Vertex Buffer",
      .initialState = nvrhi::ResourceStates::ShaderResource,
      .keepInitialState = true,
    });

    m_IndexBuffer = m_Device->createBuffer({
      .byteSize = totalIndexCount * sizeof(uint32_t),
      .structStride = sizeof(uint32_t),
      .debugName = "Index Buffer",
      .initialState = nvrhi::ResourceStates::ShaderResource,
      .keepInitialState = true,
    });

    auto upload = m_Device->createCommandList();
    upload->open();
    upload->writeBuffer(m_VertexBuffer, allVertices.data(), totalVertexCount * sizeof(Vertex));
    upload->writeBuffer(m_IndexBuffer, allIndices.data(), totalIndexCount * sizeof(uint32_t));
    upload->close();

    m_Device->executeCommandList(upload);
  }
}
