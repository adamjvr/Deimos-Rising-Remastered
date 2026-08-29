#include "deimos/original_game_frame_preview.hpp"

#include <cassert>
#include <filesystem>
#include <string>
#include <type_traits>

int main() {
    static_assert(std::is_move_constructible_v<deimos::OriginalGameFramePreview>);
    static_assert(!std::is_copy_constructible_v<deimos::OriginalGameFramePreview>);
    static_assert(deimos::kCanonicalOriginalGameInitialFrameFnv64 == 0x9e8a7ec73b79b254ull);
    static_assert(deimos::kCanonicalOriginalGameTick1FrameFnv64 == 0x44dede08075273f2ull);
    static_assert(deimos::kCanonicalOriginalGameTick30FrameFnv64 == 0x51d4a7eec9b0beefull);

    const auto impossible = std::filesystem::temp_directory_path() /
        "deimos-original-game-frame-preview-definitely-missing";
    assert(!deimos::original_game_pak_directory_valid(impossible));

    std::string error;
    const auto preview = deimos::OriginalGameFramePreview::load(impossible, {{'l','e','0','1'}}, 0, &error);
    assert(!preview);
    assert(error.find("Game.pak and Interface.pak") != std::string::npos);
    return 0;
}
