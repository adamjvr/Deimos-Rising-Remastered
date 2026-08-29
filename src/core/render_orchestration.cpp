#include "deimos/render_orchestration.hpp"

#include <cmath>

namespace deimos {
namespace {
constexpr FourCC fourcc(char a, char b, char c, char d) { return FourCC{{a,b,c,d}}; }

bool absent(FourCC id) {
    return id == FourCC{} || id == fourcc('n','o','n','e');
}

const LegacySpriteFrameMetadata* resolve_frame(
    LegacySpriteCache& cache,
    FourCC face,
    int frame,
    const LegacySpriteCache::Loader& loader) {
    if (absent(face)) return nullptr;
    if (const auto* existing = cache.find_loaded_frame(face, frame)) return existing;
    if (loader) {
        // dimensions() is the recovered 0x19CA0 lazy-load/retry entry point.
        (void)cache.dimensions(face, frame, 1.0f, loader);
    }
    return cache.find_loaded_frame(face, frame);
}

LegacyRasterRequest common_request(
    const LegacySpriteVisualRuntime& runtime,
    const LegacyRenderOrchestrationContext& context,
    const LegacySpriteFrameMetadata* frame) {
    LegacyRasterRequest q;
    q.frame = frame;
    q.sprite_face = runtime.sprite_face;
    q.sprite_frame = runtime.sprite_frame;
    q.clip = context.clip;
    q.immediate = context.immediate;
    return q;
}

void set_main_geometry(
    LegacyRasterRequest& q,
    const LegacySpriteVisualRuntime& runtime,
    const LegacyRenderOrchestrationContext& context) {
    // 0x1305C..0x1308C truncates world coordinates independently. Ordinary
    // world-space sprites subtract 0x100A0. Terrain sprites instead add the
    // fixed +32 X basis and 0xFEC0 world/background Y origin.
    const int x = static_cast<int>(std::trunc(context.world_x));
    const int y = static_cast<int>(std::trunc(context.world_y));
    if (runtime.draw_to_terrain) {
        q.center_x = x + 32;
        q.center_y = y + context.world_y_origin;
    } else {
        q.center_x = x - (runtime.world_space ? context.horizontal_view_offset : 0);
        q.center_y = y;
    }
    q.scale = runtime.scale;
}

} // namespace

std::uint16_t legacy_rgb24_to_rgb555(Rgb24 color) {
    return static_cast<std::uint16_t>(
        ((static_cast<std::uint16_t>(color.red) >> 3u) << 10u) |
        ((static_cast<std::uint16_t>(color.green) >> 3u) << 5u) |
         (static_cast<std::uint16_t>(color.blue) >> 3u));
}

LegacyRenderSubmissionBatch build_legacy_raster_requests(
    LegacySpriteVisualRuntime& runtime,
    LegacySpriteCache& cache,
    const LegacyShadowRuntimeConfig& shadow_config,
    const LegacyRenderOrchestrationContext& context,
    LegacyRenderPassSelection selection,
    const LegacySpriteCache::Loader& loader) {
    LegacyRenderSubmissionBatch out;
    if (runtime.visibility_percent <= 0.0f || absent(runtime.sprite_face)) return out;

    const auto* frame = resolve_frame(cache, runtime.sprite_face, runtime.sprite_frame, loader);
    if (!frame) return out;

    // 0x12F20 emits shadow before the main path.
    if (selection.shadow && selection.global_shadows_enabled && runtime.casts_shadows) {
        const auto g = build_legacy_shadow_request_geometry(
            runtime,
            context.world_x,
            context.world_y,
            shadow_config,
            LegacyShadowTransformContext{context.horizontal_view_offset, context.world_y_origin});
        auto q = common_request(runtime, context, frame);
        q.center_x = g.x;
        q.center_y = g.y;
        q.numeric_layer = static_cast<std::uint8_t>(g.numeric_layer);
        q.scale = g.scale;
        q.effect_amount_0_to_32 = g.transparency_0_to_32;
        q.flags = kLegacyRenderShadow | (g.draw_to_terrain ? kLegacyRenderTerrainTarget : 0u);
        out.requests.push_back(q);
    }

    if (!selection.main) return out;

    bool terrain_target = false;
    if (runtime.draw_to_terrain) {
        // 0x13264..0x132A4 stamps +0x90 only on a strictly newer sequence.
        // That same branch supplies layer 1 + flag 0x8 for the terrain main
        // submission. Repeated calls within one sequence therefore do not
        // generate another persistent main-terrain write in clean orchestration.
        terrain_target = legacy_terrain_submission_due(runtime, context.render_sequence);
        out.terrain_main_marked_this_sequence = terrain_target;
        if (!terrain_target) return out;
    }

    const auto intents = build_legacy_render_intents(
        runtime,
        LegacyRenderPassSelection{false, true, selection.global_shadows_enabled});
    for (const auto& intent : intents) {
        auto q = common_request(runtime, context, frame);
        set_main_geometry(q, runtime, context);
        q.numeric_layer = static_cast<std::uint8_t>(terrain_target ? 1 : intent.numeric_layer);
        q.effect_amount_0_to_32 = intent.effect_amount_0_to_32;
        q.effect_color = legacy_rgb24_to_rgb555(intent.effect_color);
        if (terrain_target) q.flags |= kLegacyRenderTerrainTarget;

        switch (intent.kind) {
        case LegacyRenderPassKind::base_sprite:
            // 0x132A8 marks normal base composition as the overall-
            // transparency compositor only when the live visibility float is
            // not exactly 100.0, even if fctiwz later maps it back to amount 0.
            if (runtime.visibility_percent != 100.0f) {
                q.flags |= kLegacyRenderOverallTransparency;
            }
            break;
        case LegacyRenderPassKind::tint:
        case LegacyRenderPassKind::collision_glow:
            q.flags |= kLegacyRenderSolidColor;
            break;
        case LegacyRenderPassKind::shadow:
            // Shadow is emitted above from 0x13460, never from 0x12FA0.
            continue;
        }
        out.requests.push_back(q);
    }
    return out;
}

std::vector<LegacyRasterResult> submit_legacy_sprite_render(
    LegacySpriteVisualRuntime& runtime,
    LegacySpriteCache& cache,
    const LegacyShadowRuntimeConfig& shadow_config,
    const LegacyRenderOrchestrationContext& context,
    LegacyRenderQueue& queue,
    LegacyRasterSurface& main_surface,
    LegacyRasterSurface& terrain_surface,
    LegacyRenderPassSelection selection,
    LegacyRasterConfig raster_config,
    const LegacySpriteCache::Loader& loader) {
    auto batch = build_legacy_raster_requests(
        runtime, cache, shadow_config, context, selection, loader);
    std::vector<LegacyRasterResult> results;
    results.reserve(batch.requests.size());
    for (auto& request : batch.requests) {
        results.push_back(submit_legacy_render_request(
            request, queue, main_surface, terrain_surface, raster_config));
    }
    return results;
}

} // namespace deimos
