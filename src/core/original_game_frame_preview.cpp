#include "deimos/original_game_frame_preview.hpp"

#include "deimos/data_tables.hpp"
#include "deimos/game_definitions.hpp"
#include "deimos/frame_timing_runtime.hpp"
#include "deimos/image16_resource.hpp"
#include "deimos/legacy_text.hpp"
#include "deimos/live_entity_screen_motion.hpp"
#include "deimos/level.hpp"
#include "deimos/pak_archive.hpp"
#include "deimos/player_runtime.hpp"
#include "deimos/resource_id.hpp"
#include "deimos/unit_rule_world_runtime.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace deimos {
namespace {

constexpr FourCC fourcc(char a, char b, char c, char d) {
    return FourCC{{a,b,c,d}};
}

bool fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}


bool tag_equal_ci(FourCC a, FourCC b) {
    for (std::size_t i = 0; i < 4; ++i) {
        const auto ac = static_cast<unsigned char>(a.bytes[i]);
        const auto bc = static_cast<unsigned char>(b.bytes[i]);
        if (std::tolower(ac) != std::tolower(bc)) return false;
    }
    return true;
}

bool absent(FourCC id) {
    return id == FourCC{} || id == fourcc('n','o','n','e') || id == fourcc('N','U','L','L');
}

const PakEntry* find_resource(
    const PakArchive& pak,
    FourCC id,
    ResourceKind kind,
    PlateKind plate = PlateKind::none,
    bool case_insensitive_tag = false) {
    for (const auto& entry : pak.entries()) {
        if (entry.is_directory) continue;
        const auto name = parse_resource_name(entry.path);
        if (!name || name->kind != kind || name->plate != plate) continue;
        const bool match = case_insensitive_tag ? tag_equal_ci(name->tag, id) : name->tag == id;
        if (match) return &entry;
    }
    return nullptr;
}

std::optional<TaggedTextDocument> read_tagged_document(
    const PakArchive& pak,
    const PakEntry& entry,
    std::string* error) {
    auto bytes = pak.read(entry, error);
    if (!bytes) return std::nullopt;
    auto doc = parse_tagged_text(decode_legacy_text(*bytes), error);
    if (!doc && error) *error = entry.path + ": " + *error;
    return doc;
}

template <class Table, class Parser>
std::optional<Table> load_table(
    const PakArchive& pak,
    FourCC id,
    ResourceKind kind,
    Parser parser,
    std::string* error) {
    const auto* entry = find_resource(pak, id, kind);
    if (!entry) {
        fail(error, "required table resource is missing: " + id.str());
        return std::nullopt;
    }
    auto doc = read_tagged_document(pak, *entry, error);
    if (!doc) return std::nullopt;
    auto value = parser(*doc, error);
    if (!value && error && error->find(entry->path) == std::string::npos) {
        *error = entry->path + ": " + *error;
    }
    return value;
}

std::optional<TextFormatDefinition> load_text_style(
    const PakArchive& pak,
    FourCC id,
    std::string* error) {
    const auto* entry = find_resource(pak, id, ResourceKind::text_format);
    if (!entry) {
        fail(error, "required text-format resource is missing: " + id.str());
        return std::nullopt;
    }
    auto doc = read_tagged_document(pak, *entry, error);
    if (!doc) return std::nullopt;
    auto style = parse_text_format(*doc, error);
    if (!style && error) *error = entry->path + ": " + *error;
    return style;
}

std::optional<LegacyRasterSurface> load_tga_by_tag(
    const PakArchive& pak,
    FourCC id,
    std::string* error) {
    const auto* entry = find_resource(pak, id, ResourceKind::image16, PlateKind::none, true);
    if (!entry) {
        fail(error, "required im16 resource is missing: " + id.str());
        return std::nullopt;
    }
    auto bytes = pak.read(*entry, error);
    if (!bytes) return std::nullopt;
    auto surface = decode_legacy_tga16(*bytes, error);
    if (!surface && error) *error = entry->path + ": " + *error;
    return surface;
}

std::optional<LegacySpriteGroupMetadata> load_sprite_group(
    const PakArchive& pak,
    FourCC requested_id,
    std::string* error) {
    if (absent(requested_id)) {
        fail(error, "requested sprite group is absent/none");
        return std::nullopt;
    }

    const PakEntry* alpha_entry = nullptr;
    std::optional<ResourceName> alpha_name;
    for (const auto& entry : pak.entries()) {
        if (entry.is_directory) continue;
        auto name = parse_resource_name(entry.path);
        if (!name || name->kind != ResourceKind::image8 || name->plate != PlateKind::alpha) continue;
        if (!tag_equal_ci(name->tag, requested_id)) continue;
        alpha_entry = &entry;
        alpha_name = std::move(name);
        break;
    }
    if (!alpha_entry || !alpha_name) {
        fail(error, "sprite alpha plate is missing: " + requested_id.str());
        return std::nullopt;
    }

    const PakEntry* color_entry = nullptr;
    for (const auto& entry : pak.entries()) {
        if (entry.is_directory) continue;
        const auto name = parse_resource_name(entry.path);
        if (!name || name->kind != ResourceKind::image8 || name->plate != PlateKind::color) continue;
        if (name->display_name == alpha_name->display_name || tag_equal_ci(name->tag, requested_id)) {
            color_entry = &entry;
            break;
        }
    }
    if (!color_entry) {
        fail(error, "sprite color plate is missing: " + requested_id.str());
        return std::nullopt;
    }

    auto alpha_bytes = pak.read(*alpha_entry, error);
    if (!alpha_bytes) return std::nullopt;
    auto color_bytes = pak.read(*color_entry, error);
    if (!color_bytes) return std::nullopt;
    auto alpha = decode_legacy_gif_indices(*alpha_bytes, error);
    if (!alpha) {
        if (error) *error = alpha_entry->path + ": " + *error;
        return std::nullopt;
    }
    auto color = decode_legacy_gif_indices(*color_bytes, error);
    if (!color) {
        if (error) *error = color_entry->path + ": " + *error;
        return std::nullopt;
    }
    auto group = build_legacy_sprite_group(requested_id, *alpha, *color, error);
    if (!group && error) *error = requested_id.str() + ": " + *error;
    return group;
}

bool ensure_sprite_loaded(
    LegacySpriteCache& cache,
    const PakArchive& pak,
    FourCC id,
    std::string* error) {
    if (absent(id)) return true;
    if (cache.frame_count(id) != 0) return true;
    auto group = load_sprite_group(pak, id, error);
    if (!group) return false;
    if (!cache.publish(std::move(*group))) {
        return fail(error, "failed to publish sprite group: " + id.str());
    }
    return true;
}

std::optional<LevelDefinition> load_level(
    const PakArchive& pak,
    FourCC id,
    std::string* error) {
    const auto* entry = find_resource(pak, id, ResourceKind::level);
    if (!entry) {
        fail(error, "level resource is missing: " + id.str());
        return std::nullopt;
    }
    auto bytes = pak.read(*entry, error);
    if (!bytes) return std::nullopt;
    auto level = decode_and_parse_level(*bytes, error);
    if (!level && error) *error = entry->path + ": " + *error;
    return level;
}

std::optional<LegacySpriteGroupMetadata> load_small_text_font(
    const PakArchive& interface_pak,
    std::string* error) {
    const auto* alpha_entry = find_resource(
        interface_pak, fourcc('T','E','S','M'), ResourceKind::image8, PlateKind::alpha, true);
    const auto* color_entry = find_resource(
        interface_pak, fourcc('t','e','s','m'), ResourceKind::image8, PlateKind::color, true);
    if (!alpha_entry || !color_entry) {
        fail(error, "Interface.pak is missing the canonical TESM/tesm small-text plates");
        return std::nullopt;
    }
    auto alpha_bytes = interface_pak.read(*alpha_entry, error);
    if (!alpha_bytes) return std::nullopt;
    auto color_bytes = interface_pak.read(*color_entry, error);
    if (!color_bytes) return std::nullopt;
    auto alpha = decode_legacy_gif_indices(*alpha_bytes, error);
    auto color = decode_legacy_gif_indices(*color_bytes, error);
    if (!alpha || !color) return std::nullopt;
    return build_legacy_sprite_group(fourcc('t','e','s','m'), *alpha, *color, error);
}

} // namespace

bool original_game_pak_directory_valid(const std::filesystem::path& pak_directory) noexcept {
    std::error_code ec;
    return std::filesystem::is_regular_file(pak_directory / "Game.pak", ec) &&
           std::filesystem::is_regular_file(pak_directory / "Interface.pak", ec);
}

std::optional<OriginalGameFramePreview> OriginalGameFramePreview::load(
    const std::filesystem::path& pak_directory,
    FourCC level_id,
    int player_index,
    std::string* error) {
    if (!original_game_pak_directory_valid(pak_directory)) {
        fail(error, "original PAK directory must contain Game.pak and Interface.pak");
        return std::nullopt;
    }
    if (player_index < 0 || player_index > 1) {
        fail(error, "player preview index must be 0 or 1");
        return std::nullopt;
    }

    auto game_pak = PakArchive::open(pak_directory / "Game.pak", error);
    if (!game_pak) return std::nullopt;
    auto interface_pak = PakArchive::open(pak_directory / "Interface.pak", error);
    if (!interface_pak) return std::nullopt;

    OriginalGameFramePreview out;
    auto level = load_level(*game_pak, level_id, error);
    if (!level) return std::nullopt;

    auto game_floats = load_table<NamedTable<float>>(
        *game_pak, fourcc('g','a','f','l'), ResourceKind::float_list, parse_float_list, error);
    if (!game_floats) return std::nullopt;
    auto game_rects = load_table<NamedTable<RectI>>(
        *game_pak, fourcc('i','n','r','e'), ResourceKind::rect_list, parse_rect_list, error);
    if (!game_rects) return std::nullopt;
    auto game_objects = load_table<NamedTable<FourCC>>(
        *game_pak, fourcc('g','a','o','b'), ResourceKind::id_list, parse_id_list, error);
    if (!game_objects) return std::nullopt;

    const auto presentation = compile_legacy_presentation_config(*game_floats, error);
    const auto terrain_cfg = compile_legacy_terrain_surface_config(*game_floats, error);
    const auto shadow_cfg = compile_legacy_shadow_runtime_config(*game_floats, error);
    const auto score_cfg = compile_legacy_score_bar_config(*game_floats, *game_rects, error);
    if (!presentation || !terrain_cfg || !shadow_cfg || !score_cfg) return std::nullopt;
    out.presentation_config_ = *presentation;
    out.flee_target_config_.visible_width = static_cast<float>(presentation->visible_game_width);
    out.flee_target_config_.visible_height = static_cast<float>(presentation->visible_game_height);
    const auto game_float = [&](std::string_view key, float fallback) {
        const auto it = std::find_if(game_floats->begin(), game_floats->end(), [&](const auto& item) {
            return item.first == key;
        });
        return it == game_floats->end() ? fallback : it->second;
    };
    out.flee_target_config_.north = game_float(
        "Game_EntityFleeNorthLocation", out.flee_target_config_.north);
    out.flee_target_config_.south = game_float(
        "Game_EntityFleeSouthLocation", out.flee_target_config_.south);
    out.flee_target_config_.west = game_float(
        "Game_EntityFleeWestLocation", out.flee_target_config_.west);
    out.flee_target_config_.east = game_float(
        "Game_EntityFleeEastLocation", out.flee_target_config_.east);
    out.terrain_config_ = *terrain_cfg;
    out.shadow_config_ = *shadow_cfg;
    out.score_bar_config_ = *score_cfg;
    const auto player_runtime_globals = compile_legacy_player_runtime_globals(*game_floats, error);
    if (!player_runtime_globals) return std::nullopt;
    out.player_runtime_globals_ = *player_runtime_globals;
    const auto player_runtime_resources = compile_legacy_player_runtime_resources(*game_objects, error);
    if (!player_runtime_resources) return std::nullopt;
    out.player_runtime_resources_ = *player_runtime_resources;
    const auto player_score_globals = compile_legacy_player_score_globals(*game_floats, error);
    if (!player_score_globals) return std::nullopt;
    out.player_score_globals_ = *player_score_globals;
    const auto random_bonus_config = compile_legacy_random_bonus_config(
        *game_floats, *game_objects, error);
    if (!random_bonus_config) return std::nullopt;
    out.random_bonus_config_ = *random_bonus_config;
    const auto water_impact_config = compile_legacy_water_impact_config(*game_objects, error);
    if (!water_impact_config) return std::nullopt;
    out.water_impact_config_ = *water_impact_config;
    const auto particle_tuning = compile_legacy_particle_tuning(*game_floats, error);
    if (!particle_tuning) return std::nullopt;
    out.particle_tuning_ = *particle_tuning;

    auto background = load_tga_by_tag(*game_pak, level->background_image, error);
    if (!background) return std::nullopt;
    out.persistent_terrain_ = std::move(*background);
    if (!initialize_legacy_terrain_surface_runtime(
            out.terrain_runtime_, out.persistent_terrain_, out.terrain_config_, error)) {
        return std::nullopt;
    }

    // Level Media Mask is a first-class original resource, not a synthesized
    // collision bitmap. Canonical Level 1 proves an exact 480x3600 -> 96x720
    // pairing. Derive the cell scale from the level rectangle so the live
    // removal path can bind PPC 0xFEE0's value==31 water classification
    // without hard-coding a Level-1-only 5:1 divisor.
    auto media_mask = load_tga_by_tag(*game_pak, level->media_mask, error);
    if (!media_mask) return std::nullopt;
    const auto media_mask_geometry = compile_legacy_media_mask_geometry(
        *media_mask, level->background, error);
    if (!media_mask_geometry) return std::nullopt;
    out.media_mask_ = std::move(*media_mask);
    out.media_mask_geometry_ = *media_mask_geometry;

    auto panel = load_tga_by_tag(*game_pak, fourcc('s','c','o','r'), error);
    if (!panel) return std::nullopt;
    out.score_bar_panel_ = std::move(*panel);

    auto font = load_small_text_font(*interface_pak, error);
    if (!font || font->frames.size() < 90) {
        if (font && error) *error = "Interface.pak small-text font has fewer than 90 frames";
        return std::nullopt;
    }
    out.small_text_font_ = std::move(*font);

    const auto load_style = [&](FourCC id) { return load_text_style(*game_pak, id, error); };
    auto sbsh = load_style(fourcc('s','b','s','h'));
    auto sbpm = load_style(fourcc('s','b','p','m'));
    auto sbs1 = load_style(fourcc('s','b','s','1'));
    auto sbs2 = load_style(fourcc('s','b','s','2'));
    auto sbl1 = load_style(fourcc('s','b','l','1'));
    auto sbl2 = load_style(fourcc('s','b','l','2'));
    auto sll1 = load_style(fourcc('s','l','l','1'));
    auto sll2 = load_style(fourcc('s','l','l','2'));
    if (!sbsh || !sbpm || !sbs1 || !sbs2 || !sbl1 || !sbl2 || !sll1 || !sll2) return std::nullopt;
    out.score_bar_styles_.shield = *sbsh;
    out.score_bar_styles_.power = *sbpm;
    out.score_bar_styles_.score = {{*sbs1, *sbs2}};
    out.score_bar_styles_.lives = {{*sbl1, *sbl2}};
    out.score_bar_styles_.last_life = {{*sll1, *sll2}};

    auto definitions = GameDefinitions::load_from_game_pak(*game_pak, error);
    if (!definitions) return std::nullopt;
    if (static_cast<std::size_t>(player_index) >= definitions->players().size()) {
        fail(error, "Game.pak does not contain the requested player definition");
        return std::nullopt;
    }
    const auto& player_tagged = definitions->players()[static_cast<std::size_t>(player_index)];
    const auto player_def = compile_player_runtime_definition(player_tagged.definition);

    LegacyScoreBarWeaponInput weapon_input;
    weapon_input.power_percentage = 100.0f;
    for (std::size_t i = 0; i < weapon_input.previews.size(); ++i) {
        if (i < definitions->weapons().size()) {
            weapon_input.previews[i] = compile_legacy_score_bar_weapon_preview(definitions->weapons()[i].definition);
        }
    }

    PlayerRuntimeSlot player;
    player.player_index = static_cast<std::int8_t>(player_index);
    player.enabled = true;
    player.status = static_cast<int>(LegacyPlayerStatus::active);
    initialize_legacy_player_gameplay(player, player_def);
    player.x = static_cast<float>(player_def.entry_solo_start_x);
    player.y = static_cast<float>(player_def.entry_solo_start_y);

    out.score_bar_player_ = initialize_legacy_score_bar_player(player, player_def, weapon_input);
    out.score_bar_weapon_input_ = weapon_input;
    out.player_runtime_ = player;
    out.player_definition_ = player_def;
    const auto player_control = compile_preview_player_control_config(
        player_tagged.definition, *game_floats,
        out.presentation_config_.visible_game_width,
        out.presentation_config_.visible_game_height, error);
    if (!player_control) return std::nullopt;
    out.player_control_config_ = *player_control;

    const auto score_resources = out.score_bar_player_.resources;
    for (const auto id : {score_resources.base_face, score_resources.power_face, score_resources.shield_face}) {
        if (!ensure_sprite_loaded(out.sprite_cache_, *game_pak, id, error)) return std::nullopt;
    }
    for (const auto& preview : out.score_bar_player_.weapon_previews) {
        if (!ensure_sprite_loaded(out.sprite_cache_, *game_pak, preview.face, error)) return std::nullopt;
    }

    // The player score-bar life symbol is the same canonical sprite family the
    // original data exposes for Player 1/2 appearance. Use it as the first
    // native-frame integration sprite while full live player render state is
    // still being wired into the app loop.
    out.player_visual_.sprite_face = score_resources.base_face;
    out.player_visual_.sprite_frame = score_resources.base_frame;
    out.player_visual_.draw_layer = fourcc('p','l','a','y');
    out.player_visual_.air_domain = true;
    out.player_visual_.world_space = true;
    out.player_visual_.casts_shadows = false;
    out.player_visual_.visibility_percent = 100.0f;
    out.player_visual_.required_visibility_percent = 100.0f;
    out.player_visual_.scale = 1.0f;
    out.player_visual_.required_scale = 1.0f;
    out.player_x_ = player.x;
    out.player_y_ = player.y;
    if (!refresh_legacy_sprite_geometry(out.player_visual_, out.sprite_cache_)) {
        fail(error, "failed to resolve Player-1 collision geometry");
        return std::nullopt;
    }
    out.player_runtime_.collision_half_width = out.player_visual_.half_width;
    out.player_runtime_.collision_half_height = out.player_visual_.half_height;

    out.info_.level_id = level_id;
    out.info_.level_name = level->name;
    out.info_.background_id = level->background_image;
    out.info_.media_mask_id = level->media_mask;
    out.info_.media_mask_width = out.media_mask_.width;
    out.info_.media_mask_height = out.media_mask_.height;
    out.info_.media_mask_cell_width = out.media_mask_geometry_.world_pixels_per_cell_x;
    out.info_.media_mask_cell_height = out.media_mask_geometry_.world_pixels_per_cell_y;
    out.info_.player_name = player_tagged.definition.name;
    out.info_.player_face = score_resources.base_face;
    out.info_.player_frame = score_resources.base_frame;
    out.info_.loaded_sprite_groups = out.sprite_cache_.group_count();
    const auto timing = compile_legacy_frame_timing_config(*game_floats, error);
    if (!timing) return std::nullopt;
    out.info_.fps_max_rate = timing->fps_max_rate;

    out.game_surface_ = LegacyRasterSurface(
        out.presentation_config_.visible_game_width,
        out.presentation_config_.visible_game_height, 0);
    out.presentation_source_ = LegacyRasterSurface(
        out.presentation_config_.visible_game_width + out.presentation_config_.score_bar_width,
        out.presentation_config_.min_screen_height, 0);
    out.display_surface_ = LegacyRasterSurface(
        out.presentation_config_.min_screen_width,
        out.presentation_config_.min_screen_height, 0);

    // Keep the parsed original-data corpus available for opt-in live-world
    // construction. Baseline preview/tick behavior remains unchanged until
    // enable_live_world() is called, preserving all established frame oracles.
    out.level_definition_ = std::move(*level);
    out.weapon_catalog_ = compile_live_player_weapon_catalog(*definitions);
    out.game_definitions_ = std::move(*definitions);
    out.game_pak_ = std::move(*game_pak);
    return out;
}

OriginalGameFrameTickResult OriginalGameFramePreview::tick(
    const PreviewPlayerControlInput& input) {
    OriginalGameFrameTickResult result;
    result.player_control = advance_preview_player_control(
        player_runtime_, player_control_config_, input);
    player_x_ = player_runtime_.x;
    player_y_ = player_runtime_.y;
    result.terrain_reached_end = tick_legacy_terrain_scroll(
        terrain_runtime_, persistent_terrain_);
    advance_legacy_score_bar_player(
        score_bar_player_, player_runtime_, score_bar_weapon_input_, score_bar_config_);
    ++ticks_elapsed_;
    result.tick_index = ticks_elapsed_;
    result.terrain_source_top = terrain_runtime_.source_view.top;
    result.terrain_applied_vertical_delta = terrain_runtime_.applied_vertical_delta;
    return result;
}


bool OriginalGameFramePreview::refresh_live_entity_visual(
    EntityRuntime& entity,
    std::string* error) {
    if (!game_definitions_ || !game_pak_) return fail(error, "live world original-data corpus is unavailable");
    const auto* unit = game_definitions_->find_unit(entity.unit_id);
    if (!unit) return fail(error, "live entity Unit Definition is missing: " + entity.unit_id.str());
    if (entity.state.current_state >= entity.behavior.states.size()) {
        return fail(error, "live entity state lies outside compiled visual behavior");
    }

    auto it = std::find_if(live_visuals_.begin(), live_visuals_.end(), [&](const auto& record) {
        return record.handle == entity.handle;
    });
    if (it == live_visuals_.end()) {
        LiveEntityVisualRecord record;
        record.handle = entity.handle;
        record.state_index = entity.state.current_state;
        record.visual = initialise_legacy_sprite_visual(
            entity.behavior, entity.state.current_state, visual_random_,
            entity.sprite_frame);
        live_visuals_.push_back(std::move(record));
        it = std::prev(live_visuals_.end());
    } else if (it->state_index != entity.state.current_state) {
        const auto& visual_state = entity.behavior.states[entity.state.current_state];
        apply_legacy_state_visual_targets(
            it->visual, visual_state, entity.sprite_frame);
        it->state_index = entity.state.current_state;
    } else if (it->visual.sprite_frame != entity.sprite_frame) {
        it->visual.sprite_frame = entity.sprite_frame;
        it->visual.bounds_dirty = true;
    }

    if (!ensure_sprite_loaded(sprite_cache_, *game_pak_, it->visual.sprite_face, error)) return false;
    if (!refresh_legacy_sprite_geometry(it->visual, sprite_cache_)) {
        return fail(error, "failed to resolve live entity sprite geometry: " + it->visual.sprite_face.str());
    }
    entity.collision_half_width = it->visual.half_width;
    entity.collision_half_height = it->visual.half_height;
    return true;
}

bool OriginalGameFramePreview::refresh_live_ground_crosshair(std::string* error) {
    ground_crosshair_enabled_ = false;
    ground_crosshair_locked_ = false;

    if (!live_world_enabled_ || !game_pak_) return true;
    const auto* weapon = selected_live_ground_weapon(weapon_catalog_, weapon_state_);
    if (!weapon || absent(weapon->crosshair_face)) return true;

    // Shipped sprite-base constructor 0x12650 seeds layer 'defa'. The weapon
    // controller at 0x3B3C0 then copies selected-ground +0x168/+0x16C into
    // that persistent sprite. 0x3C4F0 proves the sprite center is controller
    // (player) position plus the serialized +0x178/+0x17C offsets: it computes
    // playerY-crosshairY and divides by abs(+0x17C) for ground launch geometry.
    ground_crosshair_x_ = player_runtime_.x + static_cast<float>(weapon->crosshair_x_offset);
    ground_crosshair_y_ = player_runtime_.y + static_cast<float>(weapon->crosshair_y_offset);

    if (!ensure_sprite_loaded(sprite_cache_, *game_pak_, weapon->crosshair_face, error)) return false;

    LegacySpriteVisualRuntime probe;
    probe.sprite_face = weapon->crosshair_face;
    probe.sprite_frame = weapon->crosshair_frame;
    probe.draw_layer = fourcc('d','e','f','a');
    probe.world_space = true;
    probe.visibility_percent = 100.0f;
    probe.required_visibility_percent = 100.0f;
    probe.scale = 1.0f;
    probe.required_scale = 1.0f;
    probe.bounds_dirty = true;
    if (!refresh_legacy_sprite_geometry(probe, sprite_cache_)) {
        return fail(error, "failed to resolve ground crosshair sprite geometry: " + weapon->crosshair_face.str());
    }

    // PPC target scan calls 0x3BAB0(controller,1) when the reticle rectangle
    // overlaps an eligible ground entity. Mirror the proven AABB decision in
    // the clean stable-handle world; projectiles/effects that are not hittable
    // by player projectiles are intentionally excluded.
    for (const auto& entity : entity_world_.members()) {
        if (entity.lifecycle != EntityLifecycle::active) continue;
        if (entity.behavior.collision_domain != fourcc('g','r','n','d')) continue;
        if (!entity.behavior.can_be_hit_by_player_projectile) continue;
        const float dx = std::abs(entity.x - ground_crosshair_x_);
        const float dy = std::abs(entity.y - ground_crosshair_y_);
        if (dx <= static_cast<float>(entity.collision_half_width + probe.half_width) &&
            dy <= static_cast<float>(entity.collision_half_height + probe.half_height)) {
            ground_crosshair_locked_ = true;
            break;
        }
    }

    const FourCC face = ground_crosshair_locked_ && !absent(weapon->crosshair_locked_face)
        ? weapon->crosshair_locked_face : weapon->crosshair_face;
    const int frame = ground_crosshair_locked_
        ? weapon->crosshair_locked_frame : weapon->crosshair_frame;
    if (!ensure_sprite_loaded(sprite_cache_, *game_pak_, face, error)) return false;

    ground_crosshair_visual_ = probe;
    ground_crosshair_visual_.sprite_face = face;
    ground_crosshair_visual_.sprite_frame = frame;
    ground_crosshair_visual_.bounds_dirty = true;
    if (!refresh_legacy_sprite_geometry(ground_crosshair_visual_, sprite_cache_)) {
        return fail(error, "failed to resolve locked ground crosshair sprite geometry: " + face.str());
    }
    ground_crosshair_enabled_ = true;
    return true;
}

bool OriginalGameFramePreview::construct_live_entity_group(
    const SpawnRequestSeed& request,
    std::size_t* groups_created,
    std::size_t* members_created,
    std::string* error) {
    if (!game_definitions_) return fail(error, "live world definitions are unavailable");
    const auto* unit = game_definitions_->find_unit(request.unit_id);
    if (!unit) return fail(error, "spawn target Unit Definition is missing: " + request.unit_id.str());

    EntityHeadlessConstructionContext context;
    context.preflight.current_tick = static_cast<std::uint32_t>(ticks_elapsed_);
    context.preflight.active_live_member_count = static_cast<int>(entity_world_.active_member_count());
    context.preflight.same_unit_type_already_exists = entity_world_.has_active_unit(request.unit_id);
    context.preflight.player_gate.qualifying_player_present = player_world_.any_active_player();
    context.world_y_origin = terrain_runtime_.source_view.top;
    context.flee_targets = flee_target_config_;
    context.hunt_target_provider = [this](EntityPoint position) -> std::optional<EntityPoint> {
        const auto closest = player_world_.closest_active_player(position.x, position.y);
        return closest ? std::optional<EntityPoint>{closest->position} : std::nullopt;
    };

    auto build = construct_entity_group_headless(
        *unit, request, context, entity_identities_, entity_random_, entity_trig_);
    if (!build.constructed()) {
        // Constructor rejection is a normal legacy outcome. Missing target or
        // unsupported motion, however, means this live integration cannot
        // faithfully continue the request.
        if (build.status == EntityGroupBuildStatus::rejected) return true;
        return fail(error, "live entity constructor could not complete for " + request.unit_id.str());
    }
    if (build.plan.delete_existing_owned_type) {
        (void)entity_world_.mark_owned_unit_deleted(
            request.unit_id, build.plan.delete_existing_owner_index);
    }

    const auto member_count = build.members.size();
    entity_world_.register_group(std::move(build));
    if (groups_created) ++*groups_created;
    if (members_created) *members_created += member_count;

    // Newly appended members retain insertion order; initialize their visual
    // contract and collision extents after the complete group is registered.
    auto& members = entity_world_.members();
    const auto first = members.size() - member_count;
    for (std::size_t i = first; i < members.size(); ++i) {
        if (!refresh_live_entity_visual(members[i], error)) return false;
        (void)initialize_entity_owner_location(
            members[i], *unit,
            resolve_entity_owner_position(
                entity_world_, members[i], [this](std::int8_t index) {
                    return player_world_.position_for_player_index(index);
                }));
    }
    return true;
}


bool OriginalGameFramePreview::activate_live_level_row(
    int world_y,
    std::size_t* groups_created,
    std::size_t* members_created,
    std::string* error) {
    if (!level_definition_) return fail(error, "live level definition is unavailable");

    for (const auto placement_index : level_activation_.activate_row(*level_definition_, world_y)) {
        const auto& object = level_definition_->objects[placement_index];
        SpawnRequestSeed request;
        request.unit_id = object.unit_id;
        request.x = static_cast<float>(object.x);
        request.y = static_cast<float>(object.y);
        request.subtract_world_y_origin = true;
        request.editor_heading_degrees = object.heading_degrees;
        request.stationary = object.stationary;
        request.terrain_effects_enabled = object.terrain_effects;
        request.player_owner_index = -1;
        if (!construct_live_entity_group(request, groups_created, members_created, error)) return false;
    }
    return true;
}

bool OriginalGameFramePreview::refresh_live_score_bar_weapon_previews(
    bool mark_changed,
    std::string* error) {
    std::array<LegacyScoreBarWeaponPreview, 3> previews{};
    std::size_t output = 0;

    auto append = [&](const LivePlayerWeaponSlot& slot) {
        if (output >= previews.size()) return true;
        previews[output++] = {slot.score_bar_preview_face, slot.score_bar_preview_frame};
        return ensure_sprite_loaded(sprite_cache_, *game_pak_, slot.score_bar_preview_face, error);
    };

    // The score-bar renderer treats slot 0 as selected. Keep the currently
    // selected, level-available air weapon first, followed by the remaining
    // available air weapons in definition order. Locked weapons stay blank;
    // this matches original Level-1 screenshots instead of advertising every
    // weapon in the data corpus from the first frame.
    if (const auto* selected = selected_live_air_weapon(weapon_catalog_, weapon_state_)) {
        if (!append(*selected)) return false;
    }
    for (std::size_t i = 0; i < weapon_catalog_.air.size() && output < previews.size(); ++i) {
        if (i == weapon_state_.selected_air) continue;
        const auto& slot = weapon_catalog_.air[i];
        if (live_level_number_ < slot.minimum_level_available ||
            live_level_number_ > slot.maximum_level_available) continue;
        if (!append(slot)) return false;
    }

    score_bar_weapon_input_.previews = previews;
    score_bar_weapon_input_.previews_changed = mark_changed;
    if (!mark_changed) {
        score_bar_player_.weapon_previews = previews;
        score_bar_player_.dirty.weapons = true;
    }
    return true;
}

bool OriginalGameFramePreview::enable_live_world(std::string* error) {
    if (live_world_enabled_) return true;
    if (!game_definitions_ || !game_pak_ || !level_definition_) {
        return fail(error, "original-data live world cannot start without Game.pak definitions and level data");
    }

    entity_world_ = {};
    player_world_ = {};
    live_visuals_.clear();
    pause_vertical_scrolling_latched_ = false;
    entity_identities_ = {};
    entity_random_ = LegacyRandom{1};
    visual_random_ = LegacyRandom{1};
    random_bonus_context_ = {};
    random_bonus_context_.progression_value = 1;
    ground_obstacles_.clear();
    particle_runtime_ = {};
    // 0x44630 is a one-time subsystem bootstrap consuming 302 legacy RNG
    // draws. The exact global startup ordering relative to Level construction
    // is not yet caller-closed, so initialize direction geometry from a copy
    // of the deterministic stream. Actual gameplay particle producers below
    // consume entity_random_ inline at their recovered call sites.
    auto particle_startup_random = entity_random_;
    initialize_legacy_particle_directions(
        particle_runtime_, presentation_config_.visible_game_width,
        presentation_config_.visible_game_height, particle_startup_random);
    player_world_.slots()[0] = player_runtime_;
    initialize_live_player_weapon_state(weapon_state_, weapon_catalog_, live_level_number_);
    // The live weapon-power producer is not yet recovered. A permanently full
    // meter was misleading in the playable host, so live mode begins at the
    // neutral zero state while the static external-data oracle remains
    // untouched. This is explicitly separate from the score multiplier.
    score_bar_weapon_input_.power_percentage = 0.0f;
    score_bar_player_.displayed_power = 0.0f;
    score_bar_player_.dirty.power = true;
    if (!refresh_live_score_bar_weapon_previews(false, error)) return false;

    // PPC 0xFA10 does not instantiate the entire level placement corpus. It
    // primes world routine 0x33090 one world-Y row at a time from the current
    // terrain bottom through sourceTop-64. Release only placements whose yLoc
    // is reached by that recovered row stream.
    level_activation_.reset(level_definition_->objects.size());
    std::size_t groups = 0;
    std::size_t members = 0;
    bool activation_ok = true;
    prime_legacy_terrain_rows(terrain_runtime_, [&](int world_y) {
        if (!activation_ok) return;
        activation_ok = activate_live_level_row(world_y, &groups, &members, error);
    });
    if (!activation_ok) return false;

    // The selected air weapon owns the player's in-game appearance face in
    // canonical Weapon Definitions. Apply it only in opt-in live-world mode so
    // all previously frozen baseline preview hashes remain stable.
    if (const auto* weapon = selected_live_air_weapon(weapon_catalog_, weapon_state_)) {
        if (!absent(weapon->player1_appearance_face)) {
            if (!ensure_sprite_loaded(sprite_cache_, *game_pak_, weapon->player1_appearance_face, error)) return false;
            player_visual_.sprite_face = weapon->player1_appearance_face;
            player_visual_.sprite_frame = 0;
            if (!refresh_legacy_sprite_geometry(player_visual_, sprite_cache_)) {
                return fail(error, "failed to resolve live Player-1 weapon appearance geometry");
            }
            player_runtime_.collision_half_width = player_visual_.half_width;
            player_runtime_.collision_half_height = player_visual_.half_height;
            player_world_.slots()[0] = player_runtime_;
        }
    }

    live_world_enabled_ = true;
    if (!refresh_live_ground_crosshair(error)) {
        live_world_enabled_ = false;
        return false;
    }
    return true;
}

OriginalGameLiveTickResult OriginalGameFramePreview::tick_live(
    const OriginalGameLiveInput& input,
    std::string* error) {
    OriginalGameLiveTickResult result;
    if (!live_world_enabled_ && !enable_live_world(error)) return result;

    // PlayerWorld slot 0 is the authoritative live Player-1 state. Host input,
    // weapon launch geometry, collision damage/pickups, HUD caches, and render
    // coordinates all consume this same object during the tick. player_runtime_
    // is only the public/static-preview mirror at the live boundary.
    auto& live_player = player_world_.slots()[0];

    // PPC 0x12C10 advances the shared sprite-base hit/pickup glow once per
    // game tick. Trigger calls occur later in collision/pickup processing, so
    // a newly requested pulse renders first at amount 32 and begins becoming
    // visible on the following tick, matching the shipped update order.
    tick_legacy_collision_glow(player_visual_);
    for (auto& record : live_visuals_) tick_legacy_collision_glow(record.visual);

    std::vector<SpawnRequestSeed> player_effect_spawns;
    const auto queue_player_effect = [&](FourCC id, const PlayerRuntimeSlot& player, int count = 1) {
        if (absent(id) || count <= 0) return;
        for (int i = 0; i < count; ++i) {
            SpawnRequestSeed request;
            request.unit_id = id;
            request.x = player.x;
            request.y = player.y;
            request.subtract_world_y_origin = false;
            request.player_owner_index = player.player_index;
            player_effect_spawns.push_back(request);
        }
    };

    // The recovered damage path only *enters* status 3. The separate 0x2A150
    // lifecycle owns death timing, life decrement, respawn, invulnerability
    // expiry, entry effect and eventual game-over disable. Run it before input
    // so a dead ship cannot keep moving/firing during its death animation.
    const auto lifecycle = advance_legacy_player_lifecycle(
        live_player, player_definition_, static_cast<std::uint32_t>(ticks_elapsed_), true, false);
    if (lifecycle.entry_spawn_due) queue_player_effect(*lifecycle.entry_spawn_due, live_player);

    if (live_player.status == static_cast<int>(LegacyPlayerStatus::active)) {
        result.frame.player_control = advance_preview_player_control(
            live_player, player_control_config_, input.movement);
    }
    player_runtime_ = live_player;
    player_x_ = live_player.x;
    player_y_ = live_player.y;

    // 0x10000 first advances the terrain source view and then calls world
    // routine 0x33090 for exactly sourceTop-64. Existing screen-space members
    // receive the camera delta; members constructed by the newly activated row
    // already subtract the new world-Y origin and must not be shifted twice.
    const std::size_t pre_scroll_member_count = entity_world_.members().size();
    const std::size_t placements_before_scroll = level_activation_.activated_count();
    bool activation_ok = true;
    if (!pause_vertical_scrolling_latched_) {
        result.frame.terrain_reached_end = tick_legacy_terrain_scroll(
            terrain_runtime_, persistent_terrain_, [&](int world_y) {
                if (!activation_ok) return;
                activation_ok = activate_live_level_row(
                    world_y, &result.constructed_groups, &result.constructed_members, error);
            });
        if (!activation_ok) return result;
    } else {
        // The pause latch lives outside the terrain runtime in the shipped
        // outer loop. Do not consume vertical progress or activate rows while
        // held, and explicitly clear the per-frame camera delta.
        terrain_runtime_.applied_vertical_delta = 0;
        result.frame.terrain_reached_end = terrain_runtime_.reached_end;
    }
    result.level_placements_activated =
        level_activation_.activated_count() - placements_before_scroll;
    const int terrain_delta = terrain_runtime_.applied_vertical_delta;
    if (terrain_delta != 0) {
        // PPC 0x2A770 scrolls the persistent destruct-to-terrain obstacle
        // rectangle list by the same screen-space camera delta as live members.
        ground_obstacles_.shift_vertical(terrain_delta);
        auto& members = entity_world_.members();
        const auto shift_count = std::min(pre_scroll_member_count, members.size());
        for (std::size_t i = 0; i < shift_count; ++i) {
            auto& entity = members[i];
            if (entity.lifecycle == EntityLifecycle::active) {
                shift_live_entity_for_terrain_scroll(entity, terrain_delta);
            }
        }
    }

    const auto weapon_input =
        live_player.status == static_cast<int>(LegacyPlayerStatus::active)
        ? input.weapons
        : LivePlayerWeaponInput{};
    result.weapons = advance_live_player_weapons(
        weapon_catalog_, weapon_state_, weapon_input, live_player,
        static_cast<std::uint32_t>(ticks_elapsed_), live_level_number_);

    // The recovered score-bar power meter is a consumer, not the producer.
    // Feed it from the serialized hold-to-charge Weapon Definition bridge so
    // live mode no longer leaves the second meter permanently neutral.
    score_bar_weapon_input_.power_percentage = result.weapons.air_power_percentage;

    auto construct_launch = [&](const std::optional<LivePlayerWeaponLaunch>& launch) {
        if (!launch) return true;
        for (const auto& request : launch->requests) {
            if (!construct_live_entity_group(
                    request, &result.constructed_groups, &result.constructed_members, error)) {
                return false;
            }
        }
        return true;
    };
    if (!construct_launch(result.weapons.air_launch) || !construct_launch(result.weapons.ground_launch)) {
        return result;
    }
    for (const auto& request : result.weapons.powerup_requests) {
        if (!construct_live_entity_group(
                request, &result.constructed_groups, &result.constructed_members, error)) return result;
    }

    // Weapon power-up activation Units carry a state explicitly marked
    // `stateUseThisStateOnWeaponPowerupRelease_BOOL`. When the held air weapon
    // is released, drive the matching player-owned activation object into that
    // serialized state instead of letting the charge visual hang forever.
    if (result.weapons.air_powerup_released) {
        if (const auto* air = selected_live_air_weapon(weapon_catalog_, weapon_state_)) {
            for (auto& entity : entity_world_.members()) {
                if (entity.lifecycle != EntityLifecycle::active ||
                    entity.player_owner_index != live_player.player_index ||
                    entity.unit_id != air->powerup_air_activation_spawn_id) continue;
                const auto* unit = game_definitions_->find_unit(entity.unit_id);
                if (!unit) continue;
                for (std::size_t state_index = 0; state_index < unit->states.size(); ++state_index) {
                    if (!unit->states[state_index].fields
                             .bool_value("stateUseThisStateOnWeaponPowerupRelease_BOOL")
                             .value_or(false)) continue;
                    enter_entity_state(
                        entity, *unit, state_index,
                        static_cast<std::uint32_t>(ticks_elapsed_), entity_random_);
                    (void)refresh_live_entity_visual(entity, error);
                    break;
                }
            }
        }
    }

    // Entry/respawn effects are produced by the player lifecycle before the
    // world member pass and therefore may participate normally in this tick.
    for (const auto& request : player_effect_spawns) {
        if (!construct_live_entity_group(
                request, &result.constructed_groups, &result.constructed_members, error)) return result;
        ++result.player_effect_spawns;
    }
    player_effect_spawns.clear();

    if (result.weapons.air_switched) {
        if (!refresh_live_score_bar_weapon_previews(true, error)) return result;
        if (const auto* weapon = selected_live_air_weapon(weapon_catalog_, weapon_state_)) {
            if (!absent(weapon->player1_appearance_face)) {
                if (!ensure_sprite_loaded(sprite_cache_, *game_pak_, weapon->player1_appearance_face, error)) return result;
                player_visual_.sprite_face = weapon->player1_appearance_face;
                player_visual_.sprite_frame = 0;
                (void)refresh_legacy_sprite_geometry(player_visual_, sprite_cache_);
                live_player.collision_half_width = player_visual_.half_width;
                live_player.collision_half_height = player_visual_.half_height;
                player_runtime_ = live_player;
            }
        }
    }

    std::vector<SpawnRequestSeed> pending_spawns;
    bool pause_vertical_scrolling_this_frame = false;
    const std::size_t update_count = entity_world_.members().size();
    for (std::size_t i = 0; i < update_count; ++i) {
        auto& entity = entity_world_.members()[i];
        if (entity.lifecycle != EntityLifecycle::active) continue;
        const auto* unit = game_definitions_->find_unit(entity.unit_id);
        if (!unit) continue;
        if (!advance_entity_group_delay_gate(entity)) continue;

        // The air-weapon charge activation object is explicitly player-owned
        // and its current state is Lock-to-owner. The clean EntityReference
        // cannot point at a PlayerRuntimeSlot, so bind that narrowly proven
        // weapon effect to its player here. Do not generalize this to every
        // player-owned Lock-to-owner Unit: other effect families have separate
        // caller semantics and broad binding perturbs established live oracles.
        const auto* selected_air_for_lock =
            selected_live_air_weapon(weapon_catalog_, weapon_state_);
        if (selected_air_for_lock &&
            entity.unit_id == selected_air_for_lock->powerup_air_activation_spawn_id &&
            entity.parent.empty() && entity.player_owner_index >= 0 &&
            static_cast<std::size_t>(entity.player_owner_index) < player_world_.slots().size() &&
            entity.state.current_state < unit->states.size() &&
            unit->states[entity.state.current_state].fields
                .bool_value("stateLockToOwnerLoc_BOOL").value_or(false)) {
            const auto& owner_player =
                player_world_.slots()[static_cast<std::size_t>(entity.player_owner_index)];
            if (owner_player.enabled) {
                entity.x = owner_player.x;
                entity.y = owner_player.y;
                entity.velocity_x = 0.0f;
                entity.velocity_y = 0.0f;
            }
        }

        const auto previous_state = entity.state.current_state;
        const int previous_sprite_frame = entity.sprite_frame;
        const bool previous_fleeing = entity.fleeing;
        const bool players_active_for_motion = player_world_.any_active_player();
        EntityTickContext context;
        context.current_tick = static_cast<std::uint32_t>(ticks_elapsed_);
        context.particle_execution = {&particle_runtime_, &particle_tuning_};

        // PPC 0x15550 runs after animation and before the later 0x15280
        // player-target dispatcher. The core evaluator was already recovered,
        // but the live host previously omitted its world-facts provider and
        // therefore skipped all five rule slots. Supply the original-shaped
        // Unit-ID/range/global queries here. The 2,773 canonical default
        // `Is Tracking Player` slots carry Unit ID `none`, so they remain inert
        // instead of being confused with this member's later target flag.
        const auto visual_for_rules = std::find_if(
            live_visuals_.begin(), live_visuals_.end(), [&](const auto& record) {
                return record.handle == entity.handle;
            });
        UnitRuleWorldContext rule_world;
        rule_world.entities = &entity_world_;
        rule_world.players = &player_world_;
        rule_world.subject_position = {entity.x, entity.y};
        if (visual_for_rules != live_visuals_.end()) {
            rule_world.visibility = visual_for_rules->visual.visibility_percent;
            rule_world.required_visibility = visual_for_rules->visual.required_visibility_percent;
            rule_world.tint = visual_for_rules->visual.tint_percent;
            rule_world.required_tint = visual_for_rules->visual.required_tint_percent;
            rule_world.scale = visual_for_rules->visual.scale;
            rule_world.required_scale = visual_for_rules->visual.required_scale;
        }
        context.facts_for_rule = [rule_world, &entity](
                                     const CompiledStateRule& rule, std::size_t) mutable {
            auto current = rule_world;
            // Animation is advanced inside advance_entity_runtime(), after this
            // host context is assembled, so sample the live stopped bit here.
            current.animation_stopped = entity.animation_stopped;
            return build_unit_rule_world_facts(rule, current);
        };

        context.movement_lifetime_phase = [&](EntityRuntime& live) {
            // Strongly corpus-corroborated screen integration: heading 0 is +Y
            // in velocity space and visible-screen Y subtracts velocity Y.
            advance_live_entity_screen_position(live);
            if (!legacy_live_entity_within_main_tick_bounds(
                    live,
                    presentation_config_.visible_game_width,
                    presentation_config_.visible_game_height,
                    128)) {
                live.lifecycle = EntityLifecycle::deleted;
                live.destroyed_by_owner_index = -1;
                ++result.far_offscreen_culled;
            }
        };

        context.spawn_schedule_context_phase = [&](const EntityRuntime& live) {
            SpawnScheduleContext schedule;
            schedule.parent_is_fleeing = live.fleeing;
            schedule.parent_is_onscreen =
                live.x + live.collision_half_width >= 0.0f &&
                live.x - live.collision_half_width <
                    static_cast<float>(presentation_config_.visible_game_width) &&
                live.y + live.collision_half_height >= 0.0f &&
                live.y - live.collision_half_height <
                    static_cast<float>(presentation_config_.visible_game_height);
            schedule.current_rotation_pause_ticks = live.rotation_pause_ticks;
            return schedule;
        };

        const auto tick_result = advance_entity_runtime_with_players(
            entity_world_, entity, *unit, context, player_world_, entity_random_, entity_trig_);

        if (!previous_fleeing && entity.fleeing) {
            ++result.flee_activations;
            const bool explicit_state_flee =
                entity.state.current_state < entity.behavior.states.size() &&
                !absent(entity.behavior.states[entity.state.current_state].flee_mode);
            if (explicit_state_flee) {
                ++result.explicit_state_flee_activations;
            } else if (!players_active_for_motion &&
                       (entity.behavior.flees_north_on_no_active_players ||
                        entity.behavior.flees_south_on_no_active_players)) {
                ++result.no_player_flee_activations;
            }
        }

        if (entity.lifecycle != EntityLifecycle::active) continue;
        if (previous_state != entity.state.current_state ||
            previous_sprite_frame != entity.sprite_frame) {
            if (!refresh_live_entity_visual(entity, error)) return result;
        } else {
            auto visual = std::find_if(live_visuals_.begin(), live_visuals_.end(), [&](const auto& record) {
                return record.handle == entity.handle;
            });
            if (visual != live_visuals_.end()) (void)tick_legacy_visual_scalars(visual->visual);
        }

        for (const auto& event : tick_result.spawns_due) {
            ++result.spawn_events_due;
            if (event.state_index >= unit->states.size()) continue;
            const auto& state = unit->states[event.state_index];
            if (event.spawn_set_index >= state.spawn_sets.size()) continue;
            const auto& spawn_set = state.spawn_sets[event.spawn_set_index];
            const auto* target = game_definitions_->find_unit(spawn_set.spawn_id);
            if (!target) continue;

            float parent_scale = 1.0f;
            const auto visual = std::find_if(live_visuals_.begin(), live_visuals_.end(), [&](const auto& record) {
                return record.handle == entity.handle;
            });
            if (visual != live_visuals_.end()) parent_scale = visual->visual.scale;

            SpawnRequestContext spawn_context;
            // PPC 0x15F7C calls 0x161C0 for rotation-adjusted child geometry.
            // That helper derives rotation from the current live sprite frame,
            // so a turret that visually tracks a player also launches along
            // that visual bearing even though physical heading is unchanged.
            const int visual_heading = legacy_current_entity_visual_heading(entity);
            if (spawn_set.adjust_offset_for_unit_rotation) {
                ++result.rotation_adjusted_spawn_events;
                if (visual_heading != entity.heading_degrees) {
                    ++result.rotation_heading_differences;
                }
            }
            spawn_context.placement = {entity.x, entity.y, parent_scale, visual_heading,
                                       target->core_fields.bool_value("adjustInitialLocForOwnerScale_BOOL").value_or(false)};
            spawn_context.parent_is_stationary = entity.stationary;
            spawn_context.parent_terrain_effects_enabled = entity.terrain_effects_enabled;
            spawn_context.parent_player_owner_index = entity.player_owner_index;
            spawn_context.parent_reference = {entity.handle, entity.serial};
            if (auto seed = build_spawn_request_seed(spawn_set, *target, spawn_context, entity_trig_)) {
                pending_spawns.push_back(*seed);
            }
        }

        // PPC 0x344F8..0x34578 performs ground-obstacle stopping in the later
        // main-tick slot. Building due spawn requests above deliberately sees
        // the pre-stop stationary/terrain-effect state.
        (void)apply_legacy_ground_obstacle_stop(entity, ground_obstacles_);

        if (entity.lifecycle == EntityLifecycle::active &&
            entity.state.current_state < entity.behavior.states.size()) {
            pause_vertical_scrolling_this_frame =
                pause_vertical_scrolling_this_frame ||
                entity.behavior.states[entity.state.current_state].pause_vertical_scrolling;
        }
    }

    // This frame's state OR controls the next frame's outer terrain boundary.
    pause_vertical_scrolling_latched_ = pause_vertical_scrolling_this_frame;

    for (const auto& request : pending_spawns) {
        if (!construct_live_entity_group(
                request, &result.constructed_groups, &result.constructed_members, error)) return result;
    }

    const auto definition_for_unit = [this](FourCC id) -> const UnitDefinition* {
        return game_definitions_ ? game_definitions_->find_unit(id) : nullptr;
    };

    // The collision core already recovers PPC 0x14F10/0x16300 destruction
    // ordering, but that path is consequence-first and requires the owning host
    // to supply the removal context. Without it, lethal hits merely flipped the
    // lifecycle flag and the outer PPC 0x36610 removal pass never ran. That
    // produced the observed "enemy vanished" failure: no destruction spawn,
    // group accounting, deletion spawn, coin/random-bonus consequence, or
    // destruct-to-terrain obstacle was committed.
    LegacyRemovalContext removal_context;
    removal_context.current_tick = static_cast<std::uint32_t>(ticks_elapsed_);
    removal_context.random_bonus = random_bonus_context_;
    removal_context.random_bonus_config = random_bonus_config_;
    removal_context.world_y_origin = terrain_runtime_.source_view.top;
    removal_context.water_impact_config = water_impact_config_;
    removal_context.ground_obstacles = &ground_obstacles_;
    removal_context.particle_execution = {&particle_runtime_, &particle_tuning_};
    removal_context.water_probe = [this](int world_x, int world_y) {
        return legacy_media_mask_is_water(
            media_mask_, media_mask_geometry_, world_x, world_y);
    };
    // Removal media classification is backed directly by the level's decoded
    // original Media Mask resource. Particle producers now execute through the
    // recovered 0x43340 bridge at their inline collision/destruction sites.

    LegacyRemovalTrace collision_removal_trace;
    std::vector<SpawnRequestSeed> pending_collision_spawns;
    const std::size_t collision_scan_count = entity_world_.members().size();
    for (std::size_t i = 0; i < collision_scan_count; ++i) {
        auto& entity = entity_world_.members()[i];
        if (entity.lifecycle != EntityLifecycle::active) continue;
        const auto collision = scan_legacy_entity_collisions(
            entity_world_, entity, static_cast<std::uint32_t>(ticks_elapsed_),
            definition_for_unit, entity_random_, player_runtime_globals_.entity_hit_delay_ticks,
            &removal_context, &collision_removal_trace);
        result.collisions += collision.collisions_applied;
        for (const auto& pair : collision.pairs) {
            const auto trigger_entity_hit_glow = [&](const CollisionDamageResult& damage, EntityHandle target) {
                if (!damage.collision_glow_due || target == kNoEntityHandle) return;
                const auto visual = std::find_if(
                    live_visuals_.begin(), live_visuals_.end(),
                    [&](const auto& record) { return record.handle == target; });
                if (visual != live_visuals_.end()) {
                    // 0x14F10 calls 0x12BC0(target, 0x7FFF, 6, 0).
                    trigger_legacy_collision_glow(visual->visual, {255,255,255}, 6, false);
                }
            };
            trigger_entity_hit_glow(pair.first_damage, pair.first_damage_target);
            trigger_entity_hit_glow(pair.second_damage, pair.second_damage_target);

            const auto queue_collision_spawn = [&](const CollisionDamageResult& damage) {
                if (!damage.collision_spawn_request) return;
                pending_collision_spawns.push_back(*damage.collision_spawn_request);
                ++result.collision_spawns_due;
            };
            // 0x14F10 executes first-leg then second-leg damage in this order;
            // preserve that constructor-request order while deferring actual
            // vector-growing construction until stable collision traversal ends.
            queue_collision_spawn(pair.first_damage);
            queue_collision_spawn(pair.second_damage);

            // PPC 0x14F10 produces the target Unit score exactly when shields
            // deplete. Route that already-recovered fact to the owning player
            // through 0x29A10 semantics so the live HUD is a gameplay HUD, not
            // a decorative cache. This also preserves multiplier and strict
            // extra-life threshold behavior.
            const auto award_score = [&](const CollisionDamageResult& damage, std::int8_t owner_index) {
                if (damage.score_award == 0 || owner_index != live_player.player_index) return;
                const auto score = apply_legacy_player_score(
                    live_player, player_definition_, player_score_globals_, damage.score_award);
                if (score.life_spawn_due) queue_player_effect(*score.life_spawn_due, live_player);
            };
            award_score(pair.first_damage, pair.first_damage_source_owner_index);
            award_score(pair.second_damage, pair.second_damage_source_owner_index);
        }
        if (entity.lifecycle != EntityLifecycle::active) continue;

        LegacyPlayerCollisionCallbacks callbacks;
        callbacks.try_pickup = [this, &queue_player_effect](PlayerRuntimeSlot& player, const EntityRuntime& pickup) {
            const auto picked = apply_legacy_player_pickup(player, pickup, player_definition_);
            if (picked.spawn_due) queue_player_effect(*picked.spawn_due, player);
            if (picked.feedback_due) {
                // Coin pickup path 0x37674..0x37688 calls the same shared
                // 0x12BC0 pulse as ordinary collision glow: white, rate 6,
                // no restart while an existing pulse is active.
                trigger_legacy_collision_glow(player_visual_, {255,255,255}, 6, false);
            }
            return picked.accepted;
        };
        callbacks.apply_player_damage = [this, &queue_player_effect, &removal_context, &collision_removal_trace](PlayerRuntimeSlot& player, float damage, std::uint32_t tick) {
            const auto damaged = apply_legacy_player_damage(
                player, player_definition_, damage, tick,
                player_runtime_globals_.delay_between_hit_spawns, &player_runtime_resources_);

            // Shipped player-death helper 0x27E50 calls 0x34B90(playerIndex)
            // before constructing the death spawn and money drops. That pass
            // immediately destroys/deletes player-owned effects whose current
            // states opt into owner cleanup (including Shield Warning `nosw`).
            // Preserve those consequences in the same stable collision trace;
            // no vector-growing constructors execute until traversal finishes.
            if (damaged.death_entered) {
                auto owner_cleanup = remove_legacy_player_owned_entities_on_death(
                    entity_world_, player.player_index, removal_context, entity_random_);
                collision_removal_trace.consequences.insert(
                    collision_removal_trace.consequences.end(),
                    std::make_move_iterator(owner_cleanup.consequences.begin()),
                    std::make_move_iterator(owner_cleanup.consequences.end()));
                collision_removal_trace.removals.insert(
                    collision_removal_trace.removals.end(),
                    std::make_move_iterator(owner_cleanup.removals.begin()),
                    std::make_move_iterator(owner_cleanup.removals.end()));
            }

            if (damaged.spawn_on_hit_due) queue_player_effect(*damaged.spawn_on_hit_due, player);
            if (damaged.shield_warning_due) queue_player_effect(*damaged.shield_warning_due, player);
            if (damaged.death_spawn_due) queue_player_effect(*damaged.death_spawn_due, player);
            for (const auto& drop : damaged.money_drops) {
                queue_player_effect(drop.spawn_id, player, drop.count);
            }
        };
        LegacyPlayerCollisionViewport viewport{
            presentation_config_.visible_game_width,
            presentation_config_.visible_game_height};
        const auto player_collision = scan_legacy_player_collisions(
            entity_world_, entity, player_world_, viewport,
            static_cast<std::uint32_t>(ticks_elapsed_), definition_for_unit,
            entity_random_, callbacks,
            player_runtime_globals_.impact_damage_to_entities,
            player_runtime_globals_.entity_hit_delay_ticks,
            &removal_context, &collision_removal_trace);

        // The player-impact path invokes the same 0x14F10 entity damage
        // routine as entity/entity collision. Preserve its returned glow,
        // collision-spawn constructor request and score attribution rather
        // than discarding the aggregate PlayerCollisionScanResult.
        for (const auto& event : player_collision.events) {
            const auto& damage = event.entity_damage;
            if (damage.collision_glow_due && event.entity_damage_target != kNoEntityHandle) {
                const auto visual = std::find_if(
                    live_visuals_.begin(), live_visuals_.end(),
                    [&](const auto& record) { return record.handle == event.entity_damage_target; });
                if (visual != live_visuals_.end()) {
                    trigger_legacy_collision_glow(visual->visual, {255,255,255}, 6, false);
                }
            }
            if (damage.collision_spawn_request) {
                pending_collision_spawns.push_back(*damage.collision_spawn_request);
                ++result.collision_spawns_due;
            }
            if (damage.score_award != 0 && event.player_index == live_player.player_index) {
                const auto score = apply_legacy_player_score(
                    live_player, player_definition_, player_score_globals_, damage.score_award);
                if (score.life_spawn_due) queue_player_effect(*score.life_spawn_due, live_player);
            }
        }
    }

    // Collision Spawn_ID construction is inline inside shipped damage routine
    // 0x14F10, but the clean EntityWorld stores members in a vector. Construct
    // only after all collision/player-collision traversal has released live
    // references, retaining exact pair/leg request order without invalidation.
    for (const auto& request : pending_collision_spawns) {
        if (!construct_live_entity_group(
                request, &result.constructed_groups, &result.constructed_members, error)) return result;
    }

    // PPC 0x36610 is a distinct outer pass over now-inactive members. Keep it
    // after collision traversal so stable vector references cannot be
    // invalidated by consequence-spawn construction. Its one-pass semantics
    // are preserved by the recovered runtime (owner cascades that occur behind
    // the cursor are finalized on a later game tick).
    auto finalized_removals = finalize_legacy_pending_removals(
        entity_world_, removal_context, entity_random_);
    random_bonus_context_ = removal_context.random_bonus;
    result.removals = collision_removal_trace.removals.size() + finalized_removals.removals.size();
    result.removal_consequences =
        collision_removal_trace.consequences.size() + finalized_removals.consequences.size();

    std::vector<SpawnRequestSeed> removal_spawns;
    const auto gather_removal_spawns = [&](const LegacyRemovalTrace& trace) {
        for (const auto& consequence : trace.consequences) {
            if (consequence.spawn_request) removal_spawns.push_back(*consequence.spawn_request);
        }
    };
    gather_removal_spawns(collision_removal_trace);
    gather_removal_spawns(finalized_removals);
    result.removal_spawns = removal_spawns.size();
    for (const auto& request : removal_spawns) {
        if (!construct_live_entity_group(
                request, &result.constructed_groups, &result.constructed_members, error)) return result;
    }

    // Player collision callbacks run during stable world traversal, so their
    // object consequences are deferred until both collision scans and the
    // outer removal pass have finished. This restores plsh/plde/entry/life and
    // death-money objects without invalidating member references mid-scan.
    for (const auto& request : player_effect_spawns) {
        if (!construct_live_entity_group(
                request, &result.constructed_groups, &result.constructed_members, error)) return result;
        ++result.player_effect_spawns;
    }
    player_effect_spawns.clear();

    // Collision damage can cause an immediate on-hit/shield-depleted state
    // transition after the earlier entity-state visual refresh. Synchronize
    // surviving visuals once more at the post-collision boundary.
    for (auto& entity : entity_world_.members()) {
        if (entity.lifecycle != EntityLifecycle::active) continue;
        const auto visual = std::find_if(live_visuals_.begin(), live_visuals_.end(), [&](const auto& record) {
            return record.handle == entity.handle;
        });
        if (visual != live_visuals_.end() && visual->state_index != entity.state.current_state) {
            if (!refresh_live_entity_visual(entity, error)) return result;
        }
    }

    const auto particle_stats = update_legacy_particles(
        particle_runtime_, particle_tuning_,
        presentation_config_.visible_game_width,
        presentation_config_.visible_game_height,
        terrain_delta);
    result.particle_systems = particle_runtime_.systems.size();
    result.active_particles = particle_stats.active_particles_after;

    // The original world unlinks finalized members. The portable host used to
    // retain them forever, causing long sessions to scan an ever-growing dead
    // history and render to perform nested linear handle lookups. Compact only
    // after every consequence/parent/removal consumer has completed this tick.
    const auto pruned = entity_world_.prune_finalized_history();
    result.pruned_members = pruned.members_removed;
    result.pruned_groups = pruned.groups_removed;
    if (pruned.members_removed != 0) {
        live_visuals_.erase(
            std::remove_if(live_visuals_.begin(), live_visuals_.end(), [this](const auto& record) {
                return entity_world_.find_member(record.handle) == nullptr;
            }),
            live_visuals_.end());
    }

    player_runtime_ = live_player;
    player_x_ = live_player.x;
    player_y_ = live_player.y;
    if (!refresh_live_ground_crosshair(error)) return result;
    result.ground_crosshair_enabled = ground_crosshair_enabled_;
    result.ground_crosshair_locked = ground_crosshair_locked_;
    result.ground_crosshair_face = ground_crosshair_visual_.sprite_face;
    result.ground_crosshair_frame = ground_crosshair_visual_.sprite_frame;
    result.ground_crosshair_x = ground_crosshair_x_;
    result.ground_crosshair_y = ground_crosshair_y_;
    advance_legacy_score_bar_player(
        score_bar_player_, player_runtime_, score_bar_weapon_input_, score_bar_config_);
    score_bar_weapon_input_.previews_changed = false;

    ++ticks_elapsed_;
    result.frame.tick_index = ticks_elapsed_;
    result.frame.terrain_source_top = terrain_runtime_.source_view.top;
    result.frame.terrain_applied_vertical_delta = terrain_delta;
    result.active_entities = entity_world_.active_member_count();
    return result;
}

bool OriginalGameFramePreview::render(
    LegacyRasterSurface& canonical_display,
    LegacyGameplayFrameResult* frame_result,
    std::string* error) {
    if (!game_surface_.valid() || !presentation_source_.valid() || !display_surface_.valid()) {
        return fail(error, "original-game preview persistent frame surfaces are invalid");
    }

    // Until persistent entity-owned queue records are integrated, rebuild the
    // fixture's request queue per visible frame. Layers 2..15 are resident in
    // the original queue; blindly appending a new Player-1 request every tick
    // would duplicate the sprite instead of updating its existing record.
    render_queue_.clear();

    LegacyRenderOrchestrationContext render_context;
    render_context.world_x = player_x_;
    render_context.world_y = player_y_;
    render_context.clip = game_surface_.bounds();
    render_context.horizontal_view_offset = horizontal_view_.offset;
    render_context.world_y_origin = terrain_runtime_.source_view.top;
    render_context.render_sequence = render_sequence_++;
    render_context.immediate = false;

    if (live_world_enabled_ && ground_crosshair_enabled_ &&
        player_runtime_.enabled &&
        player_runtime_.status == static_cast<int>(LegacyPlayerStatus::active)) {
        LegacyRenderOrchestrationContext crosshair_context = render_context;
        crosshair_context.world_x = ground_crosshair_x_;
        crosshair_context.world_y = ground_crosshair_y_;
        (void)submit_legacy_sprite_render(
            ground_crosshair_visual_, sprite_cache_, shadow_config_, crosshair_context,
            render_queue_, game_surface_, persistent_terrain_, {}, {}, {});
    }

    if (!live_world_enabled_ ||
        (player_runtime_.enabled &&
         player_runtime_.status == static_cast<int>(LegacyPlayerStatus::active))) {
        (void)submit_legacy_sprite_render(
            player_visual_, sprite_cache_, shadow_config_, render_context,
            render_queue_, game_surface_, persistent_terrain_, {}, {}, {});
    }

    if (live_world_enabled_) {
        for (auto& record : live_visuals_) {
            auto* entity = entity_world_.find_member(record.handle);
            if (!entity || entity->lifecycle != EntityLifecycle::active) continue;
            LegacyRenderOrchestrationContext entity_context = render_context;
            entity_context.world_x = entity->x;
            entity_context.world_y = entity->y;
            (void)submit_legacy_sprite_render(
                record.visual, sprite_cache_, shadow_config_, entity_context,
                render_queue_, game_surface_, persistent_terrain_, {}, {}, {});
        }
    }

    LegacyGameplayFrameScoreBarInput score_bar;
    score_bar.players[0] = &score_bar_player_;
    score_bar.config = &score_bar_config_;
    score_bar.styles = &score_bar_styles_;
    score_bar.assets = {&score_bar_panel_, &small_text_font_, &sprite_cache_};
    score_bar.seed_base_panel = first_render_;

    LegacyGameplayFrameResult result;
    if (!render_legacy_gameplay_frame(
            render_queue_, game_surface_, persistent_terrain_, terrain_runtime_, horizontal_view_,
            particle_runtime_.systems, score_bar, presentation_source_,
            presentation_config_, display_surface_, true, true, result, {}, error)) {
        return false;
    }
    canonical_display = display_surface_;
    first_render_ = false;
    // 0x317E0 owns dirty production. Once a visible frame consumes the dirty
    // classes, later live ticks regenerate only the classes that changed.
    score_bar_player_.dirty = {};
    if (frame_result) *frame_result = std::move(result);
    return true;
}

} // namespace deimos
