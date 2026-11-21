#include <scene/World.h>

#include <limits>
#include <stdexcept>

namespace Iskra {
Entity World::CreateEntity(std::string_view name) {
  static_cast<void>(name);

  uint32_t index;
  if (!freeEntityIndices_.empty()) {
    index = freeEntityIndices_.back();
    freeEntityIndices_.pop_back();
  } else {
    if (entities_.size() >= InvalidEntityIndex) {
      throw std::runtime_error("Entity capacity exceeded");
    }
    index = static_cast<uint32_t>(entities_.size());
    entities_.push_back({});
  }

  EntityRecord &record = entities_[index];
  record.alive = true;
  return Entity(this, {.index = index, .generation = record.generation});
}

void World::DestroyEntity(Entity entity) {
  if (!IsAlive(entity)) {
    return;
  }

  RemoveAllComponents(entity.id_);

  EntityRecord &record = entities_[entity.id_.index];
  record.alive = false;
  if (record.generation == std::numeric_limits<uint32_t>::max()) {
    return;
  }
  ++record.generation;
  freeEntityIndices_.push_back(entity.id_.index);
}

bool World::IsAlive(Entity entity) const {
  if (entity.world_ != this || entity.id_.index >= entities_.size()) {
    return false;
  }

  const EntityRecord &record = entities_[entity.id_.index];
  return record.alive && record.generation == entity.id_.generation;
}

void World::Validate(Entity entity) const {
  if (!IsAlive(entity)) {
    throw std::invalid_argument("Entity is not alive in this world");
  }
}

void World::RemoveAllComponents(EntityId entity) {
  for (auto &[type, pool] : componentPools_) {
    static_cast<void>(type);
    pool->Remove(entity);
  }
}
}  // namespace Iskra
