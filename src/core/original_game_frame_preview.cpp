#include "deimos/original_game_frame_preview.hpp"

#include "deimos/data_tables.hpp"
#include "deimos/game_definitions.hpp"
#include "deimos/frame_timing_runtime.hpp"
#include "deimos/image16_resource.hpp"
#include "deimos/legacy_text.hpp"
#include "deimos/level.hpp"
#include "deimos/pak_archive.hpp"
#include "deimos/player_runtime.hpp"
#include "deimos/resource_id.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
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

    const auto presentation = compile_legacy_presentation_config(*game_floats, error);
    const auto terrain_cfg = compile_legacy_terrain_surface_config(*game_floats, error);
    const auto shadow_cfg = compile_legacy_shadow_runtime_config(*game_floats, error);
    const auto score_cfg = compile_legacy_score_bar_config(*game_floats, *game_rects, error);
    if (!presentation || !terrain_cfg || !shadow_cfg || !score_cfg) return std::nullopt;
    out.presentation_config_ = *presentation;
    out.terrain_config_ = *terrain_cfg;
    out.shadow_config_ = *shadow_cfg;
    out.score_bar_config_ = *score_cfg;

    auto background = load_tga_by_tag(*game_pak, level->background_image, error);
    if (!background) return std::nullopt;
    out.persistent_terrain_ = std::move(*background);
    if (!initialize_legacy_terrain_surface_runtime(
            out.terrain_runtime_, out.persistent_terrain_, out.terrain_config_, error)) {
        return std::nullopt;
    }

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

    out.info_.level_id = level_id;
    out.info_.level_name = level->name;
    out.info_.background_id = level->background_image;
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

    (void)submit_legacy_sprite_render(
        player_visual_, sprite_cache_, shadow_config_, render_context,
        render_queue_, game_surface_, persistent_terrain_, {}, {}, {});

    LegacyGameplayFrameScoreBarInput score_bar;
    score_bar.players[0] = &score_bar_player_;
    score_bar.config = &score_bar_config_;
    score_bar.styles = &score_bar_styles_;
    score_bar.assets = {&score_bar_panel_, &small_text_font_, &sprite_cache_};
    score_bar.seed_base_panel = first_render_;

    LegacyGameplayFrameResult result;
    if (!render_legacy_gameplay_frame(
            render_queue_, game_surface_, persistent_terrain_, terrain_runtime_, horizontal_view_,
            std::span<const LegacyParticleSystem>{}, score_bar, presentation_source_,
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
