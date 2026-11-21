#pragma once

#include <scene/components/Entity.h>
#include <scene/components/component_pool.h>
#include <scene/components/component_query.h>

#include <memory>
#include <stdexcept>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Iskra {
class World {
 public:
  Entity CreateEntity(std::string_view name = {});
  void DestroyEntity(Entity entity);
  bool IsAlive(Entity entity) const;

  template <typename Component, typename... Args>
  Component &Add(Entity entity, Args &&...args) {
    Validate(entity);
    return GetOrCreatePool<Component>().Add(entity.id_,
                                            std::forward<Args>(args)...);
  }

  template <typename Component>
  void Remove(Entity entity) {
    if (!IsAlive(entity)) {
      return;
    }

    if (ComponentPool<Component> *pool = TryGetPool<Component>()) {
      pool->Remove(entity.id_);
    }
  }

  template <typename Component>
  Component *TryGet(Entity entity) {
    if (!IsAlive(entity)) {
      return nullptr;
    }

    ComponentPool<Component> *pool = TryGetPool<Component>();
    return pool != nullptr ? pool->TryGet(entity.id_) : nullptr;
  }

  template <typename Component>
  const Component *TryGet(Entity entity) const {
    if (!IsAlive(entity)) {
      return nullptr;
    }

    const ComponentPool<Component> *pool = TryGetPool<Component>();
    return pool != nullptr ? pool->TryGet(entity.id_) : nullptr;
  }

  template <typename... Components>
  ComponentQuery<false, Components...> Query() {
    return ComponentQuery<false, Components...>(*this,
                                                TryGetPool<Components>()...);
  }

  template <typename... Components>
  ComponentQuery<true, Components...> Query() const {
    return ComponentQuery<true, Components...>(*this,
                                               TryGetPool<Components>()...);
  }

 private:
  struct EntityRecord {
    uint32_t generation = 0;
    bool alive = false;
  };

  void Validate(Entity entity) const;
  void RemoveAllComponents(EntityId entity);

  template <typename Component>
  ComponentPool<Component> *TryGetPool() {
    const auto found = componentPools_.find(std::type_index(typeid(Component)));
    if (found == componentPools_.end()) {
      return nullptr;
    }
    return static_cast<ComponentPool<Component> *>(found->second.get());
  }

  template <typename Component>
  const ComponentPool<Component> *TryGetPool() const {
    const auto found = componentPools_.find(std::type_index(typeid(Component)));
    if (found == componentPools_.end()) {
      return nullptr;
    }
    return static_cast<const ComponentPool<Component> *>(found->second.get());
  }

  template <typename Component>
  ComponentPool<Component> &GetOrCreatePool() {
    if (ComponentPool<Component> *pool = TryGetPool<Component>()) {
      return *pool;
    }

    auto pool = std::make_unique<ComponentPool<Component>>();
    ComponentPool<Component> &result = *pool;
    componentPools_.emplace(std::type_index(typeid(Component)),
                            std::move(pool));
    return result;
  }

  std::vector<EntityRecord> entities_;
  std::vector<uint32_t> freeEntityIndices_;
  std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>>
      componentPools_;
};

template <typename Component, typename... Args>
Component &Entity::AddComponent(Args &&...args) {
  if (world_ == nullptr) {
    throw std::logic_error("Cannot add a component to an invalid entity");
  }
  return world_->Add<Component>(*this, std::forward<Args>(args)...);
}

template <typename Component>
Component *Entity::GetComponent() {
  return world_ != nullptr ? world_->TryGet<Component>(*this) : nullptr;
}

template <typename Component>
const Component *Entity::GetComponent() const {
  return world_ != nullptr
             ? static_cast<const World *>(world_)->TryGet<Component>(*this)
             : nullptr;
}

template <typename Component>
bool Entity::HasComponent() const {
  return GetComponent<Component>() != nullptr;
}

template <typename Component>
void Entity::RemoveComponent() {
  if (world_ != nullptr) {
    world_->Remove<Component>(*this);
  }
}
}  // namespace Iskra
