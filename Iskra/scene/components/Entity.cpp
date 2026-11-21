#include <scene/components/Entity.h>
#include <scene/World.h>

namespace Iskra {
Entity::operator bool() const {
  return world_ != nullptr && id_.index != InvalidEntityIndex &&
         world_->IsAlive(*this);
}
}  // namespace Iskra
