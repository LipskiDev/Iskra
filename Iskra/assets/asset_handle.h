#pragma once

#include <compare>
#include <cstdint>
#include <limits>

namespace Iskra {
template <typename Tag>
class AssetHandle {
 public:
  static constexpr uint32_t InvalidIndex = std::numeric_limits<uint32_t>::max();

  constexpr AssetHandle() = default;
  explicit constexpr AssetHandle(uint32_t index) : index_(index) {}

  constexpr bool IsValid() const { return index_ != InvalidIndex; }
  constexpr explicit operator bool() const { return IsValid(); }
  constexpr uint32_t GetIndex() const { return index_; }

  auto operator<=>(const AssetHandle &) const = default;

 private:
  uint32_t index_ = InvalidIndex;
};

struct MeshAssetTag;
struct MaterialAssetTag;
struct ModelAssetTag;

using MeshHandle = AssetHandle<MeshAssetTag>;
using MaterialHandle = AssetHandle<MaterialAssetTag>;
using ModelHandle = AssetHandle<ModelAssetTag>;
}  // namespace Iskra
