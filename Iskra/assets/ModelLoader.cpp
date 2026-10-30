#include <assets/ModelLoader.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <stdexcept>
#include <string>

namespace {
  // Converts an Assimp matrix to a glm matrix
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

  Iskra::MeshData ConvertMesh(const aiMesh& source) {
    Iskra::MeshData meshData;
    meshData.vertices.reserve(source.mNumVertices);
    meshData.indices.reserve(static_cast<size_t>(source.mNumFaces) * 3);

    for (unsigned int i = 0; i < source.mNumVertices; ++i) {
      Iskra::Vertex vertex;

      const auto& position = source.mVertices[i];
      vertex.position = { position.x, position.y, position.z };

      if (source.HasNormals()) {
        const auto& normal = source.mNormals[i];
        vertex.normal = { normal.x, normal.y, normal.z };
      }

      if (source.HasTextureCoords(0)) {
        const auto& uv = source.mTextureCoords[0][i];
        vertex.uv = { uv.x, uv.y };
      }
      meshData.vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < source.mNumFaces; ++i) {
      const auto& face = source.mFaces[i];

      if (face.mNumIndices != 3) {
        throw std::runtime_error("Only triangular meshes are supported.");
      }

      meshData.indices.push_back(face.mIndices[0]);
      meshData.indices.push_back(face.mIndices[1]);
      meshData.indices.push_back(face.mIndices[2]);
    }
    return meshData;
  }

  Iskra::ModelNode ConvertNode(const aiNode& source) {
    Iskra::ModelNode node;
    node.name = source.mName.C_Str();
    node.localTransform = ConvertMatrix(source.mTransformation);

    node.meshInstances.reserve(source.mNumMeshes);

    for (unsigned int i = 0; i < source.mNumMeshes; ++i) {
      Iskra::MeshInstanceData meshInstance;
      meshInstance.meshIndex = source.mMeshes[i];
      meshInstance.materialIndex = -1; // Material indices are not imported yet

      node.meshInstances.push_back(meshInstance);
    }

    node.children.reserve(source.mNumChildren);
    for (unsigned int i = 0; i < source.mNumChildren; ++i) {
      node.children.push_back(ConvertNode(*source.mChildren[i]));
    }

    return node;
  }
}

namespace Iskra {
  ModelData ModelLoader::Load(const std::filesystem::path& path)
  {
    Assimp::Importer importer;

    const unsigned int flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_GenSmoothNormals | aiProcess_SortByPType;

    const aiScene* scene = importer.ReadFile(path.string(), flags);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
      throw std::runtime_error("Failed to import model: " + path.string());
    }

    ModelData model;
    model.meshes.reserve(scene->mNumMeshes);

    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
      model.meshes.push_back(ConvertMesh(*scene->mMeshes[i]));
    }

    model.root = ConvertNode(*scene->mRootNode);

    return model;
  }
}