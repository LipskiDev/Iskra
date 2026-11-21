#pragma once

#include <scene/components/component_pool.h>

#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace Iskra {
class World;

namespace Detail {
template <typename... Types>
struct AreUnique : std::true_type {};

template <typename First, typename... Rest>
struct AreUnique<First, Rest...>
    : std::bool_constant<(!std::is_same_v<First, Rest> && ...) &&
                         AreUnique<Rest...>::value> {};
}  // namespace Detail

template <bool IsConst, typename... Components>
class ComponentQuery {
 public:
  static_assert(sizeof...(Components) > 0,
                "A component query must contain at least one component");
  static_assert(Detail::AreUnique<Components...>::value,
                "A component query cannot contain duplicate types");

  using WorldType = std::conditional_t<IsConst, const World, World>;

  template <typename Component>
  using PoolPointer =
      std::conditional_t<IsConst, const ComponentPool<Component> *,
                         ComponentPool<Component> *>;

  ComponentQuery(WorldType &world, PoolPointer<Components>... pools)
      : world_(&world), pools_(pools...) {}

  template <typename Function>
  void Each(Function &&function) const {
    const IComponentPool *smallestPool = nullptr;
    bool missingPool = false;

    std::apply(
        [&](auto *...pools) {
          auto consider = [&](const IComponentPool *pool) {
            if (pool == nullptr) {
              missingPool = true;
              return;
            }
            if (smallestPool == nullptr ||
                pool->GetSize() < smallestPool->GetSize()) {
              smallestPool = pool;
            }
          };
          (consider(pools), ...);
        },
        pools_);

    if (missingPool || smallestPool == nullptr) {
      return;
    }

    for (EntityId entity : smallestPool->GetEntities()) {
      if ((GetPool<Components>()->Contains(entity) && ...)) {
        std::invoke(function, Entity(const_cast<World *>(world_), entity),
                    GetPool<Components>()->Get(entity)...);
      }
    }
  }

 private:
  template <typename Component>
  PoolPointer<Component> GetPool() const {
    return std::get<PoolPointer<Component>>(pools_);
  }

  WorldType *world_;
  std::tuple<PoolPointer<Components>...> pools_;
};
}  // namespace Iskra
