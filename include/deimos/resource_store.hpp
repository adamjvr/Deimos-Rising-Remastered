#pragma once

#include "deimos/pak_archive.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace deimos {

struct ResourceBytes {
    std::vector<std::uint8_t> bytes;
    bool from_local_override = false;
    std::size_t pak_index = 0;
};

// Reconstructs the observed Deimos content lookup model: loose files under
// Data/Local override packaged resources under Data/Paks. PAK order is kept
// explicit and deterministic; later-added PAKs win if two PAKs contain the
// same logical path. The Local-over-PAK priority is evidence-backed, while
// cross-PAK collision priority is an implementation policy until proven.
class ResourceStore {
public:
    void set_local_root(std::filesystem::path root) { local_root_ = std::move(root); }
    void clear_local_root() { local_root_.reset(); }

    void add_pak(PakArchive pak) { paks_.push_back(std::move(pak)); }
    [[nodiscard]] std::size_t pak_count() const { return paks_.size(); }

    [[nodiscard]] std::optional<ResourceBytes> read(
        std::string_view logical_path,
        std::string* error = nullptr) const;

private:
    static bool safe_relative_path(std::string_view logical_path);

    std::optional<std::filesystem::path> local_root_;
    std::vector<PakArchive> paks_;
};

} // namespace deimos
