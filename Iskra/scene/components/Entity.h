#pragma once

#include <cstdint>
#include <compare>
#include <limits>

namespace Iskra {

inline constexpr uint32_t InvalidEntityIndex =
    std::numeric_limits<uint32_t>::max();

struct EntityId {
  uint32_t index = InvalidEntityIndex;
  uint32_t generation = 0;

  auto operator<=>(const EntityId &) const = default;
};

class World;
template <bool IsConst, typename... Components>
class ComponentQuery;

class Entity {
 public:
  Entity() = default;

  template <typename Component, typename... Args>
  Component &AddComponent(Args &&...args);

  template <typename Component>
  Component *GetComponent();

  template <typename Component>
  const Component *GetComponent() const;

  template <typename Component>
  bool HasComponent() const;

  template <typename Component>
  void RemoveComponent();

  explicit operator bool() const;

  EntityId GetId() const { return id_; }

 private:
  friend class World;
  template <bool IsConst, typename... Components>
  friend class ComponentQuery;

  Entity(World *world, EntityId id) : world_(world), id_(id) {}

  World *world_ = nullptr;
  EntityId id_;
};
}  // namespace Iskra
