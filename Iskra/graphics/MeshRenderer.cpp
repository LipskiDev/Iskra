#include <graphics/MeshRenderer.h>

namespace Iskra {
  void MeshRenderer::Draw(nvrhi::ICommandList* commandList,
    const GpuMesh& mesh,
    nvrhi::GraphicsState state) const
  {
    nvrhi::VertexBufferBinding vertices{};
    vertices.buffer = mesh.vertexBuffer;
    vertices.slot = 0;
    vertices.offset = 0;

    // The caller supplies pipeline, shader bindings, framebuffer and viewport.
    state.vertexBuffers = {};
    state.addVertexBuffer(vertices);

    nvrhi::IndexBufferBinding indices{};
    indices.buffer = mesh.indexBuffer;
    indices.format = nvrhi::Format::R32_UINT;
    indices.offset = 0;
    state.setIndexBuffer(indices);

    commandList->setGraphicsState(state);

    nvrhi::DrawArguments args{};
    args.vertexCount = mesh.indexCount;
    commandList->drawIndexed(args);
  }
}
