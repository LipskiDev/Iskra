#pragma once

#include <assets/ModelData.h>
#include <filesystem>

namespace Iskra {
  class ModelLoader {
  public:
    static ModelData Load(const std::filesystem::path& path);
  };
}