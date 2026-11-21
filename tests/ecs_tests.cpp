#include <scene/World.h>

#include <stdexcept>

namespace {
struct Position {
  int value;
};

struct Velocity {
  int value;
};

void Require(bool condition) {
  if (!condition) {
    throw std::runtime_error("ECS test failed");
  }
}

void TestComponentsAndQueries() {
  Iskra::World world;
  Iskra::Entity first = world.CreateEntity();
  Iskra::Entity second = world.CreateEntity();
  Iskra::Entity third = world.CreateEntity();

  first.AddComponent<Position>(1);
  second.AddComponent<Position>(2);
  third.AddComponent<Position>(3);
  second.AddComponent<Velocity>(20);
  third.AddComponent<Velocity>(30);

  bool duplicateRejected = false;
  try {
    second.AddComponent<Velocity>(99);
  } catch (const std::logic_error &) {
    duplicateRejected = true;
  }
  Require(duplicateRejected);

  int matches = 0;
  world.Query<Position, Velocity>().Each(
      [&](Iskra::Entity entity, Position &position, Velocity &velocity) {
        Require(static_cast<bool>(entity));
        position.value += velocity.value;
        ++matches;
      });
  Require(matches == 2);
  Require(second.GetComponent<Position>()->value == 22);
  Require(third.GetComponent<Position>()->value == 33);

  const Iskra::World &constWorld = world;
  int constMatches = 0;
  constWorld.Query<Position, Velocity>().Each(
      [&](Iskra::Entity, const Position &, const Velocity &) {
        ++constMatches;
      });
  Require(constMatches == 2);

  second.RemoveComponent<Velocity>();
  Require(!second.HasComponent<Velocity>());

  matches = 0;
  world.Query<Position, Velocity>().Each(
      [&](Iskra::Entity, Position &, Velocity &) { ++matches; });
  Require(matches == 1);
}

void TestEntityLifetime() {
  Iskra::World world;
  Iskra::Entity invalid;
  Require(!invalid);

  Iskra::Entity entity = world.CreateEntity();
  entity.AddComponent<Position>(42);
  const Iskra::EntityId destroyedId = entity.GetId();

  world.DestroyEntity(entity);
  Require(!entity);
  Require(entity.GetComponent<Position>() == nullptr);

  Iskra::Entity reused = world.CreateEntity();
  Require(reused.GetId().index == destroyedId.index);
  Require(reused.GetId().generation != destroyedId.generation);
  Require(!reused.HasComponent<Position>());

  Iskra::World otherWorld;
  Iskra::Entity foreignEntity = otherWorld.CreateEntity();
  bool foreignEntityRejected = false;
  try {
    world.Add<Position>(foreignEntity, 4);
  } catch (const std::invalid_argument &) {
    foreignEntityRejected = true;
  }
  Require(foreignEntityRejected);
}
}  // namespace

int main() {
  TestComponentsAndQueries();
  TestEntityLifetime();
}
