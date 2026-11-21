#pragma once

#include <scene/components/Entity.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Iskra {
class IComponentPool {
 public:
  virtual ~IComponentPool() = default;

  virtual bool Contains(EntityId entity) const = 0;
  virtual void Remove(EntityId entity) = 0;
  virtual size_t GetSize() const = 0;
  virtual std::span<const EntityId> GetEntities() const = 0;
};

template <typename Component>
class ComponentPool final : public IComponentPool {
 public:
  template <typename... Args>
  Component &Add(EntityId entity, Args &&...args) {
    if (Contains(entity)) {
      throw std::logic_error("Entity already has component");
    }

    if (sparse_.size() <= entity.index) {
      sparse_.resize(static_cast<size_t>(entity.index) + 1, InvalidDenseIndex);
    }

    const uint32_t denseIndex = static_cast<uint32_t>(denseComponents_.size());
    sparse_[entity.index] = denseIndex;
    denseEntities_.push_back(entity);

    try {
      return denseComponents_.emplace_back(std::forward<Args>(args)...);
    } catch (...) {
      denseEntities_.pop_back();
      sparse_[entity.index] = InvalidDenseIndex;
      throw;
    }
  }

  Component *TryGet(EntityId entity) {
    if (!Contains(entity)) {
      return nullptr;
    }
    return &denseComponents_[sparse_[entity.index]];
  }

  const Component *TryGet(EntityId entity) const {
    if (!Contains(entity)) {
      return nullptr;
    }
    return &denseComponents_[sparse_[entity.index]];
  }

  Component &Get(EntityId entity) {
    return denseComponents_[sparse_[entity.index]];
  }

  const Component &Get(EntityId entity) const {
    return denseComponents_[sparse_[entity.index]];
  }

  bool Contains(EntityId entity) const override {
    if (entity.index >= sparse_.size()) {
      return false;
    }

    const uint32_t denseIndex = sparse_[entity.index];
    return denseIndex != InvalidDenseIndex &&
           denseIndex < denseEntities_.size() &&
           denseEntities_[denseIndex] == entity;
  }

  void Remove(EntityId entity) override {
    if (!Contains(entity)) {
      return;
    }

    const uint32_t removedIndex = sparse_[entity.index];
    const uint32_t lastIndex = static_cast<uint32_t>(denseEntities_.size() - 1);

    if (removedIndex != lastIndex) {
      denseEntities_[removedIndex] = std::move(denseEntities_[lastIndex]);
      denseComponents_[removedIndex] = std::move(denseComponents_[lastIndex]);
      sparse_[denseEntities_[removedIndex].index] = removedIndex;
    }

    denseEntities_.pop_back();
    denseComponents_.pop_back();
    sparse_[entity.index] = InvalidDenseIndex;
  }

  size_t GetSize() const override { return denseComponents_.size(); }

  std::span<const EntityId> GetEntities() const override {
    return denseEntities_;
  }

 private:
  static constexpr uint32_t InvalidDenseIndex =
      std::numeric_limits<uint32_t>::max();

  std::vector<uint32_t> sparse_;
  std::vector<EntityId> denseEntities_;
  std::vector<Component> denseComponents_;
};
}  // namespace Iskra
