#include "deimos/original_game_frame_preview.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>

namespace {
std::uint64_t fnv1a64(std::span<const std::uint16_t> pixels) {
    std::uint64_t h = 1469598103934665603ull;
    for (const auto v : pixels) {
        h ^= static_cast<std::uint8_t>(v & 0xffu);
        h *= 1099511628211ull;
        h ^= static_cast<std::uint8_t>((v >> 8u) & 0xffu);
        h *= 1099511628211ull;
    }
    return h;
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: deimos_original_frame_probe /path/to/Paks\n";
        return 2;
    }

    std::string error;
    auto preview = deimos::OriginalGameFramePreview::load(
        std::filesystem::path(argv[1]), {{'l','e','0','1'}}, 0, &error);
    if (!preview) {
        std::cerr << "original frame load failed: " << error << '\n';
        return 3;
    }

    deimos::LegacyRasterSurface frame;
    deimos::LegacyGameplayFrameResult result;
    if (!preview->render(frame, &result, &error)) {
        std::cerr << "original frame render failed: " << error << '\n';
        return 4;
    }

    const auto initial_hash = fnv1a64(frame.pixels);
    if (initial_hash != deimos::kCanonicalOriginalGameInitialFrameFnv64) {
        std::cerr << "original frame oracle mismatch: got 0x" << std::hex << initial_hash
                  << " expected 0x" << deimos::kCanonicalOriginalGameInitialFrameFnv64
                  << std::dec << '\n';
        return 5;
    }

    const auto& info = preview->info();
    std::cout << "Deimos original-data frame PASS\n"
              << "  level: " << info.level_name << " [" << info.level_id.str() << "]\n"
              << "  background: " << info.background_id.str() << '\n'
              << "  player: " << info.player_name << " face=" << info.player_face.str()
              << " frame=" << info.player_frame << '\n'
              << "  loaded sprite groups: " << info.loaded_sprite_groups << '\n'
              << "  FPS max rate: " << info.fps_max_rate << '\n'
              << "  display: " << frame.width << 'x' << frame.height << "x16\n"
              << "  terrain copied: " << (result.world.terrain_viewport_copied ? "yes" : "no") << '\n'
              << "  score bar P1 rasterized: " << (result.score_bar_rasterized[0] ? "yes" : "no") << '\n'
              << "  frame FNV64: 0x" << std::hex << initial_hash << std::dec << '\n';

    const auto tick1 = preview->tick();
    if (!preview->render(frame, &result, &error)) {
        std::cerr << "original frame tick-1 render failed: " << error << '\n';
        return 6;
    }
    const auto tick1_hash = fnv1a64(frame.pixels);

    deimos::OriginalGameFrameTickResult tick30 = tick1;
    for (int i = 1; i < 30; ++i) tick30 = preview->tick();
    if (!preview->render(frame, &result, &error)) {
        std::cerr << "original frame tick-30 render failed: " << error << '\n';
        return 7;
    }
    const auto tick30_hash = fnv1a64(frame.pixels);
    if (tick1_hash != deimos::kCanonicalOriginalGameTick1FrameFnv64 ||
        tick30_hash != deimos::kCanonicalOriginalGameTick30FrameFnv64) {
        std::cerr << "live frame oracle mismatch: tick1=0x" << std::hex << tick1_hash
                  << " expected=0x" << deimos::kCanonicalOriginalGameTick1FrameFnv64
                  << " tick30=0x" << tick30_hash
                  << " expected=0x" << deimos::kCanonicalOriginalGameTick30FrameFnv64
                  << std::dec << '\n';
        return 8;
    }
    std::cout << "  tick 1: sourceTop=" << tick1.terrain_source_top
              << " delta=" << tick1.terrain_applied_vertical_delta
              << " FNV64=0x" << std::hex << tick1_hash << std::dec << '\n'
              << "  tick 30: sourceTop=" << tick30.terrain_source_top
              << " delta=" << tick30.terrain_applied_vertical_delta
              << " FNV64=0x" << std::hex << tick30_hash << std::dec << '\n';
    return 0;
}
