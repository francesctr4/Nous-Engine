#pragma once

#include <string>
#include <string_view>

// Single source of truth for the engine's asset/library directory layout
// and the sidecar/scene extensions. Header-only — these are constexpr
// compile-time values and trivial inline helpers.
//
// Replaces ad-hoc string literals scattered across HotReloader,
// ImportPipeline, ModuleScene, and ModuleRenderer3D. Most importantly,
// IsAssetsRelative / StripAssetsPrefix replace the load-bearing
// `substr(0, 7) == "Assets/"` idiom — the literal `7` was a magic
// number tied to the length of the prefix.
namespace nous::engine::asset_paths
{
    // Project directory roots, no trailing slash. Use these in filesystem
    // operations that take a directory argument (recursive_directory_iterator,
    // create_directories, remove_all).
    inline constexpr std::string_view k_AssetsDir   = "Assets";
    inline constexpr std::string_view k_LibraryDir  = "Library";
    inline constexpr std::string_view k_ScenesDir   = "Library/Scenes";

    // Prefix form, trailing slash included. Use for engine-relative path
    // comparisons and concatenation (e.g. building a path under Assets/).
    inline constexpr std::string_view k_AssetsPrefix = "Assets/";

    // Sidecar / scene file extensions, dot included.
    inline constexpr std::string_view k_MetaExtension  = ".meta";
    inline constexpr std::string_view k_SceneExtension = ".nous";

    // True when `path` begins with the engine-relative "Assets/" prefix.
    [[nodiscard]] inline bool IsAssetsRelative(std::string_view path) noexcept
    {
        return path.size() > k_AssetsPrefix.size()
            && path.compare(0, k_AssetsPrefix.size(), k_AssetsPrefix) == 0;
    }

    // Returns the substring after the "Assets/" prefix when present,
    // or the original path unchanged otherwise.
    [[nodiscard]] inline std::string_view StripAssetsPrefix(std::string_view path) noexcept
    {
        return IsAssetsRelative(path) ? path.substr(k_AssetsPrefix.size()) : path;
    }

    // Returns `assetsPath + ".meta"`. The four call sites previously used
    // raw concatenation; this keeps the extension owned by one place.
    [[nodiscard]] inline std::string MakeMetaFilePath(std::string_view assetsPath)
    {
        std::string out;
        out.reserve(assetsPath.size() + k_MetaExtension.size());
        out.append(assetsPath).append(k_MetaExtension);
        return out;
    }
}
