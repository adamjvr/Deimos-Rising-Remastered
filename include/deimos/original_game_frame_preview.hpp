#pragma once

#include "deimos/gameplay_frame_runtime.hpp"
#include "deimos/collision_runtime.hpp"
#include "deimos/destruction_runtime.hpp"
#include "deimos/entity_world.hpp"
#include "deimos/game_definitions.hpp"
#include "deimos/level.hpp"
#include "deimos/level_activation_runtime.hpp"
#include "deimos/live_player_weapon_runtime.hpp"
#include "deimos/pak_archive.hpp"
#include "deimos/particle_runtime.hpp"
#include "deimos/player_runtime.hpp"
#include "deimos/render_orchestration.hpp"
#include "deimos/preview_player_control.hpp"
#include "deimos/render_runtime.hpp"
#include "deimos/score_bar_runtime.hpp"
#include "deimos/terrain_runtime.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace deimos {

// External-original-data integration fixture used to prove that the recovered
// clean renderer can produce a real Deimos frame without embedding any
// copyrighted game assets in this repository. The caller supplies a directory
// containing the user's original Game.pak and Interface.pak.
struct OriginalGameFramePreviewInfo {
    FourCC level_id{};
    std::string level_name;
    FourCC background_id{};
    FourCC media_mask_id{};
    int media_mask_width = 0;
    int media_mask_height = 0;
    int media_mask_cell_width = 0;
    int media_mask_cell_height = 0;
    std::string player_name;
    FourCC player_face{};
    int player_frame = 0;
    std::size_t loaded_sprite_groups = 0;
    float fps_max_rate = 30.0f;
};

// Canonical user-owned 1.0.6 Level-1 / Player-1 first-frame oracle. This
// freezes the complete 640x480 xRGB1555 result proven on macOS Metal without
// embedding any original data in the source tree.
inline constexpr std::uint64_t kCanonicalOriginalGameInitialFrameFnv64 =
    0x9e8a7ec73b79b254ull;
inline constexpr std::uint64_t kCanonicalOriginalGameTick1FrameFnv64 =
    0x44dede08075273f2ull;
inline constexpr std::uint64_t kCanonicalOriginalGameTick30FrameFnv64 =
    0x51d4a7eec9b0beefull;
inline constexpr std::uint64_t kCanonicalOriginalGameRightTick1FrameFnv64 =
    0x6fd5c94a64dcb0c8ull;

// First full live-world integration oracles. These are regression witnesses for
// the clean Level-1 entity/weapon/collision bridge, not claims of original
// executable screenshot capture. WIP6 restores the shipped persistent pbta
// ground-weapon reticle, so these live-only hashes include that presentation
// layer. They deliberately remain distinct from the canonical static/no-input
// frame hashes above.
inline constexpr std::uint64_t kLiveWorldIntegrationInitialFrameFnv64 =
    0xcd72678207b195b7ull;
inline constexpr std::uint64_t kLiveWorldIntegrationFireTick1FrameFnv64 =
    0x800f06651d29406aull;
inline constexpr std::uint64_t kLiveWorldIntegrationTick120FrameFnv64 =
    0x267609db3ba6dbccull;

struct OriginalGameFrameTickResult {
    std::uint64_t tick_index = 0;
    int terrain_source_top = 0;
    int terrain_applied_vertical_delta = 0;
    bool terrain_reached_end = false;
    PreviewPlayerControlResult player_control{};
};


struct OriginalGameLiveInput {
    PreviewPlayerControlInput movement{};
    LivePlayerWeaponInput weapons{};
};

struct OriginalGameLiveTickResult {
    OriginalGameFrameTickResult frame{};
    LivePlayerWeaponStepResult weapons{};
    std::size_t active_entities = 0;
    std::size_t constructed_groups = 0;
    std::size_t constructed_members = 0;
    std::size_t collisions = 0;
    std::size_t collision_spawns_due = 0;
    std::size_t removals = 0;
    std::size_t removal_consequences = 0;
    std::size_t removal_spawns = 0;
    std::size_t player_effect_spawns = 0;
    std::size_t far_offscreen_culled = 0;
    std::size_t pruned_members = 0;
    std::size_t pruned_groups = 0;
    std::size_t particle_systems = 0;
    std::size_t active_particles = 0;
    std::size_t level_placements_activated = 0;

    // WIP9 differential witnesses for PPC 0x15280/0x17510/0x161C0. These
    // counts expose when authored flee behavior and live-frame-relative spawn
    // rotation actually participate in a host tick without changing gameplay.
    std::size_t flee_activations = 0;
    std::size_t explicit_state_flee_activations = 0;
    std::size_t no_player_flee_activations = 0;
    std::size_t spawn_events_due = 0;
    std::size_t rotation_adjusted_spawn_events = 0;
    std::size_t rotation_heading_differences = 0;

    // Persistent ground-weapon crosshair owned by the player weapon
    // controller in shipped PPC 0x3B3C0/0x3BAB0/0x3BD00. Coordinates are
    // world-space sprite-center coordinates after serialized offsets.
    bool ground_crosshair_enabled = false;
    bool ground_crosshair_locked = false;
    FourCC ground_crosshair_face{};
    int ground_crosshair_frame = 0;
    float ground_crosshair_x = 0.0f;
    float ground_crosshair_y = 0.0f;
};

class OriginalGameFramePreview {
public:
    OriginalGameFramePreview() = default;
    OriginalGameFramePreview(OriginalGameFramePreview&&) noexcept = default;
    OriginalGameFramePreview& operator=(OriginalGameFramePreview&&) noexcept = default;
    OriginalGameFramePreview(const OriginalGameFramePreview&) = delete;
    OriginalGameFramePreview& operator=(const OriginalGameFramePreview&) = delete;

    // Load only the resources required for a deterministic Level-1 preview:
    // level background, score-bar panel/font/styles, player score-bar sprites,
    // three canonical weapon previews, and the Player-1 life/main sprite.
    [[nodiscard]] static std::optional<OriginalGameFramePreview> load(
        const std::filesystem::path& pak_directory,
        FourCC level_id = FourCC{{'l','e','0','1'}},
        int player_index = 0,
        std::string* error = nullptr);

    // Execute the already-recovered gameplay-frame boundary and return the
    // canonical 640x480 xRGB1555 display frame. This is deliberately a static
    // integration preview, not yet the live simulation/game loop.
    [[nodiscard]] bool render(
        LegacyRasterSurface& canonical_display,
        LegacyGameplayFrameResult* frame_result = nullptr,
        std::string* error = nullptr);

    // Advance one deterministic gameplay-preview tick. Terrain scroll and
    // score-bar convergence remain recovered/oracle-backed. The optional
    // directional input passes through the explicitly bounded preview control
    // bridge, whose tuning values are canonical but whose original
    // InputSprocket/film-bit dispatcher is still being instruction-closed.
    [[nodiscard]] OriginalGameFrameTickResult tick(
        const PreviewPlayerControlInput& input = {});

    // Upgrade the external-data preview into the first live world integration:
    // level objects are constructed through the recovered group/member path,
    // canonical weapon definitions create normal spawn requests, entity motion
    // and state machines advance at the 30 Hz game tick, and the recovered
    // entity/entity collision scanner is active. The original film/input bit
    // dispatcher remains separate; host actions arrive as semantic controls.
    [[nodiscard]] bool enable_live_world(std::string* error = nullptr);
    [[nodiscard]] OriginalGameLiveTickResult tick_live(
        const OriginalGameLiveInput& input = {},
        std::string* error = nullptr);
    [[nodiscard]] bool live_world_enabled() const noexcept { return live_world_enabled_; }
    [[nodiscard]] const EntityWorld& entity_world() const noexcept { return entity_world_; }
    [[nodiscard]] const LivePlayerWeaponState& weapon_state() const noexcept { return weapon_state_; }
    [[nodiscard]] const LivePlayerWeaponSlot* selected_air_weapon() const noexcept {
        return selected_live_air_weapon(weapon_catalog_, weapon_state_);
    }
    [[nodiscard]] const LivePlayerWeaponSlot* selected_ground_weapon() const noexcept {
        return selected_live_ground_weapon(weapon_catalog_, weapon_state_);
    }
    [[nodiscard]] std::size_t activated_level_placements() const noexcept {
        return level_activation_.activated_count();
    }
    [[nodiscard]] std::size_t pending_level_placements() const noexcept {
        return level_activation_.pending_count();
    }

    [[nodiscard]] std::uint64_t ticks_elapsed() const noexcept { return ticks_elapsed_; }
    [[nodiscard]] const OriginalGameFramePreviewInfo& info() const noexcept { return info_; }
    [[nodiscard]] const LegacyPresentationConfig& presentation_config() const noexcept {
        return presentation_config_;
    }
    [[nodiscard]] const PlayerRuntimeSlot& player_runtime() const noexcept { return player_runtime_; }
    [[nodiscard]] const PreviewPlayerControlConfig& player_control_config() const noexcept {
        return player_control_config_;
    }

private:
    OriginalGameFramePreviewInfo info_{};
    LegacyPresentationConfig presentation_config_{};
    EntityHeadlessConstructionContext::FleeTargetConfig flee_target_config_{};
    LegacyTerrainSurfaceConfig terrain_config_{};
    LegacyTerrainSurfaceRuntime terrain_runtime_{};
    LegacyHorizontalViewRuntime horizontal_view_{};
    LegacyShadowRuntimeConfig shadow_config_{};
    LegacyScoreBarConfig score_bar_config_{};
    LegacyScoreBarTextStyles score_bar_styles_{};

    LegacyRasterSurface persistent_terrain_{};
    LegacyRasterSurface media_mask_{};
    LegacyMediaMaskGeometry media_mask_geometry_{};
    LegacyRasterSurface score_bar_panel_{};
    LegacySpriteGroupMetadata small_text_font_{};
    LegacySpriteCache sprite_cache_{};
    LegacyScoreBarPlayerState score_bar_player_{};
    LegacyScoreBarWeaponInput score_bar_weapon_input_{};
    PlayerRuntimeSlot player_runtime_{};
    CompiledPlayerRuntimeDefinition player_definition_{};
    PreviewPlayerControlConfig player_control_config_{};


    struct LiveEntityVisualRecord {
        EntityHandle handle = kNoEntityHandle;
        std::size_t state_index = 0;
        LegacySpriteVisualRuntime visual{};
    };

    std::optional<PakArchive> game_pak_;
    std::optional<GameDefinitions> game_definitions_;
    std::optional<LevelDefinition> level_definition_;
    LevelPlacementActivationRuntime level_activation_{};
    PlayerWorld player_world_{};
    EntityWorld entity_world_{};
    EntityIdentityCounters entity_identities_{};
    LegacyRandom entity_random_{1};
    LegacyRandom visual_random_{1};
    LegacyTrigTables entity_trig_{};
    LegacyPlayerRuntimeGlobals player_runtime_globals_{};
    LegacyPlayerRuntimeResources player_runtime_resources_{};
    LegacyPlayerScoreGlobals player_score_globals_{};
    LegacyRandomBonusConfig random_bonus_config_{};
    LegacyRandomBonusContext random_bonus_context_{};
    LegacyWaterImpactConfig water_impact_config_{};
    LegacyGroundObstacleRects ground_obstacles_{};
    LegacyParticleTuning particle_tuning_{};
    LegacyParticleRuntime particle_runtime_{};
    LivePlayerWeaponCatalog weapon_catalog_{};
    LivePlayerWeaponState weapon_state_{};
    std::vector<LiveEntityVisualRecord> live_visuals_{};
    int live_level_number_ = 1;
    bool live_world_enabled_ = false;

    // statePauseVerticalScrolling_BOOL is OR'd from processed live members and
    // suppresses the outer terrain-scroll boundary on the following frame.
    bool pause_vertical_scrolling_latched_ = false;

    [[nodiscard]] bool construct_live_entity_group(
        const SpawnRequestSeed& request,
        std::size_t* groups_created,
        std::size_t* members_created,
        std::string* error);
    [[nodiscard]] bool refresh_live_entity_visual(
        EntityRuntime& entity,
        std::string* error);
    [[nodiscard]] bool activate_live_level_row(
        int world_y,
        std::size_t* groups_created,
        std::size_t* members_created,
        std::string* error);
    [[nodiscard]] bool refresh_live_score_bar_weapon_previews(
        bool mark_changed,
        std::string* error);
    [[nodiscard]] bool refresh_live_ground_crosshair(std::string* error);

    LegacySpriteVisualRuntime player_visual_{};
    LegacySpriteVisualRuntime ground_crosshair_visual_{};
    bool ground_crosshair_enabled_ = false;
    bool ground_crosshair_locked_ = false;
    float ground_crosshair_x_ = 0.0f;
    float ground_crosshair_y_ = 0.0f;
    float player_x_ = 0.0f;
    float player_y_ = 0.0f;

    // Persistent visible-frame objects for the live host path. The first
    // render seeds the static score-bar panel; later frames preserve the
    // presentation source and redraw only dirty HUD classes.
    LegacyRasterSurface game_surface_{};
    LegacyRasterSurface presentation_source_{};
    LegacyRasterSurface display_surface_{};
    LegacyRenderQueue render_queue_{};
    std::uint64_t ticks_elapsed_ = 0;
    std::uint32_t render_sequence_ = 1;
    bool first_render_ = true;
};

// Utility shared by the smoke app/tool. A valid directory must contain both
// Game.pak and Interface.pak; no files are copied into the repository/build.
[[nodiscard]] bool original_game_pak_directory_valid(
    const std::filesystem::path& pak_directory) noexcept;

} // namespace deimos
