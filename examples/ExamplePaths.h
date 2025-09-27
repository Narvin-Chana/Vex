#pragma once

#include <filesystem>

static inline const std::filesystem::path ExamplesDir =
    std::filesystem::current_path().parent_path().parent_path().parent_path().parent_path().parent_path() / "examples";