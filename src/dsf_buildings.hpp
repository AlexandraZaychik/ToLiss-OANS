#pragma once
#include "core.hpp"

namespace oans {
struct DsfExtraction {
    std::filesystem::path cache;
    std::string note;
    size_t buildings=0,terminals=0,tiles=0;
};
// Called on the building worker only after no usable DSF cache was found.
// Reads enabled scenery and writes a cache for subsequent airport loads.
DsfExtraction extract_runtime_dsf(const std::filesystem::path& root,
                                  const IndexEntry& entry,
                                  const std::filesystem::path& cache_directory,
                                  const std::atomic<bool>& cancel);
std::vector<std::filesystem::path> dsf_scenery_packs(const std::filesystem::path& root);
}
