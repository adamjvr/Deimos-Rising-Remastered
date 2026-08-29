#pragma once

#include "deimos/unit_definition.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace deimos {

// The 1.0.6 PPC rule evaluator at code offset 0x15550 owns a 17-entry
// condition dispatch table.  Keep this list complete even though the shipped
// 1.0.6 data only exercises a subset: older/newer content can still target
// conditions that the executable supports.
enum class UnitRuleConditionKind {
    unused,
    is_tracking_player,
    is_not_tracking_player,
    is_active,
    is_not_active,
    no_destroyable_air_entities_active,
    no_destroyable_ground_entities_active,
    no_destroyable_air_or_ground_entities_active,
    no_players_active,
    entity_within_range_of_player,
    entity_not_within_range_of_player,
    animation_has_stopped,
    visibility_at_required_level,
    tint_at_required_level,
    scale_at_required_level,
    number_of_this_type_active,
    fewer_of_these_entities_active,
    more_of_these_entities_active,
    unknown
};

enum class StateActionKind {
    none,
    change_state,
    delete_entity,
    destroy_entity,
    // The original state-action routine silently returns if a non-empty label
    // is not Delete/Destroy and does not match a local state.  Preserve the
    // label for evidence/debugging, but runtime execution is a no-op.
    unresolved
};

struct ResolvedStateAction {
    StateActionKind kind = StateActionKind::none;
    std::size_t state_index = 0;
    std::string original_label;

    [[nodiscard]] bool is_runtime_noop() const {
        return kind == StateActionKind::none || kind == StateActionKind::unresolved;
    }
};

struct CompiledStateRule {
    UnitRuleConditionKind condition = UnitRuleConditionKind::unused;
    FourCC unit_id{};
    int range = 0;
    ResolvedStateAction action;
};

struct CompiledUnitStateBehavior {
    float range = 0.0f;
    ResolvedStateAction on_range;

    // Visual-state fields reconstructed from the state parser and the
    // 0x146F0 / 0x33E0C state-entry/update paths. Percent values are kept in
    // their serialized integer domain here; render_runtime converts scale
    // percentages to the original floating-point scale domain.
    FourCC sprite_face{};                              // state +0x304
    int sprite_frame_min = 0;                         // state +0x30C
    int sprite_frame_max = 0;                         // state +0x310
    bool use_parent_direction = false;                // state +0x324
    int required_visibility_percent = 0;              // state +0x3C4
    int visibility_delta_percent = 0;                 // state +0x3C8
    int required_scale_percent = 0;                   // state +0x3BC
    int scale_delta_percent = 0;                      // state +0x3C0
    int tint_percent = 0;                             // state +0x3CC
    int tint_delta_percent = 0;                       // state +0x3D0
    Rgb24 tint_color{};                               // state +0x332
    bool do_colorise = false;                         // state +0x34D
    bool draw_to_terrain = false;                     // state +0x353

    // Collision/damage fields mapped directly from the 1.0.6 compiled-state
    // parser and runtime consumers at PPC 0x36CF0 / 0x14F10.
    bool can_be_destroyed_on_owner_destruction = false; // state +0x329
    bool can_be_deleted_on_owner_deletion = false;      // state +0x32A
    bool pass_hits_to_owner = false;                     // state +0x32B
    bool destroy_owner_on_destruction = false;           // state +0x32D
    bool collides = false;                           // state +0x347
    bool invulnerable_on_collision = false;          // state +0x348
    bool collides_with_players = false;              // state +0x34F
    bool do_not_glow_on_collision = false;           // state +0x354
    bool use_on_shield_depletion = false;            // state +0x356

    // Particle producer fields consumed inline by PPC 0x33A7C..0x33B60.
    FourCC state_particles{};                        // state +0x2D0
    Rgb24 state_particle_color{};                    // state +0x2D4 packed xRGB1555
    bool state_particles_repeat = false;             // state +0x2D6
    int state_particle_repeat_delay = 0;             // state +0x2D8
    int state_particle_max_bursts = 0;               // state +0x2DC

    FourCC collision_spawn{};                        // state +0x2E0
    bool collision_repeat_spawns = false;            // state +0x2E4
    int collision_spawn_delay = 0;                   // state +0x2E8

    ResolvedStateAction on_hit;
    int hit_state_delay = 0;
    int timer_min = 0;
    int timer_max = 0;
    ResolvedStateAction on_timer;
    int counter = 0;
    ResolvedStateAction on_counter;
    std::vector<CompiledStateRule> rules;
};

struct CompiledDestructionSoundBehavior {
    FourCC id{};
    int min_volume = 0;
    int max_volume = 0;
    int priority = 0;
    float min_pitch = 0.0f;
    float max_pitch = 0.0f;
};

struct CompiledUnitBehavior {
    // Unit-level visual defaults reconstructed from the Unit Definition
    // parser and sprite-base constructor/state-entry paths.
    int initial_scale_percent = 0;                    // UnitDef +0x1AC
    int initial_scale_tolerance_percent = 0;          // UnitDef +0x1B0
    int initial_visibility_percent = 0;               // UnitDef +0x1B4
    FourCC draw_layer{};                              // UnitDef +0x2E0
    bool adjust_shadow_location_for_scaling = false;  // UnitDef +0x12C

    // Unit-level collision contract. The original loader derives collision
    // domain +0x08 from isGroundBased_BOOL: FourCC 'grnd' or 'air '.
    FourCC collision_domain{};
    bool harmless_to_players = false;                // UnitDef +0x11A
    bool player_projectile = false;                  // UnitDef +0x11B
    bool can_be_hit_by_player_projectile = false;    // UnitDef +0x11C
    bool hittable_when_invisible = false;            // UnitDef +0x121
    bool casts_shadows = false;                       // UnitDef +0x11E
    bool collides_with_ground_obstacles = false;      // UnitDef +0x128
    bool death_spawn_on_any_media = false;            // UnitDef +0x12B
    FourCC media_impact_size{};                       // UnitDef +0x2E4
    float collision_damage = 0.0f;                   // UnitDef +0x274
    float shields_base = 0.0f;                       // UnitDef +0x43C
    float shields_level_increment = 0.0f;            // UnitDef +0x440
    float shields_max = 0.0f;                        // UnitDef +0x444
    FourCC hit_particles{};                          // UnitDef +0x2D8
    bool hit_particle_circular_burst = false;        // source loader field; canonical data false
    Rgb24 hit_particle_color{};                      // compiled packed color read at UnitDef +0x17E
    FourCC deletion_spawn{};                         // UnitDef +0x2DC

    // Destruction/removal fields consumed by PPC 0x16300 / 0x36120 / 0x36610.
    FourCC destruction_spawn{};                      // UnitDef +0x478
    FourCC destruction_particles{};                  // UnitDef +0x47C
    Rgb24 destruction_particle_color{};              // UnitDef +0x480
    std::string destruction_notice;                  // UnitDef +0x482 fixed string
    int destruction_coin_count = 0;                  // UnitDef +0x4A4
    FourCC destruction_coin{};                       // UnitDef +0x4A8
    FourCC destruction_group_kill_coin{};            // UnitDef +0x4AC
    bool destruction_destroy_children = false;       // UnitDef +0x4B0
    bool destruction_delete_children = false;        // UnitDef +0x4B1
    bool destruction_create_obstacle = false;        // UnitDef +0x4B2
    bool destruction_draw_to_terrain = false;        // UnitDef +0x4B3
    bool destruction_release_random_bonus = false;   // UnitDef +0x4B4
    int score = 0;                                   // UnitDef +0x4B8
    CompiledDestructionSoundBehavior destruction_sound; // UnitDef +0x4BC..+0x4D0
    FourCC pickup_type{};                            // UnitDef +0x4D4
    int pickup_value = 0;                            // UnitDef +0x4DC

    // PPC member constructor 0x35DAC..0x35DF0 scans state +0x356 and caches
    // whether any state carries stateUseThisStateOnShieldDepletion_BOOL in
    // live +0xCD. Keep the same derived fact with the compiled behavior.
    bool has_shield_depletion_state = false;

    std::vector<CompiledUnitStateBehavior> states;
    std::size_t unresolved_active_actions = 0;
    std::size_t unresolved_inert_actions = 0;
};

// World/query results consumed by one rule predicate.  The original evaluator
// calls world functions using the rule's Unit ID/range; the clean simulation
// will populate these facts from its world model.  Keeping predicate logic
// pure makes the binary-confirmed comparison semantics independently testable.
struct UnitRuleFacts {
    bool tracking_player = false;
    bool active = false;
    bool destroyable_air_entities_active = false;
    bool destroyable_ground_entities_active = false;
    bool players_active = false;
    bool entity_within_player_range = false;
    bool animation_stopped = false;

    float visibility = 0.0f;
    float required_visibility = 0.0f;
    float tint = 0.0f;
    float required_tint = 0.0f;
    float scale = 0.0f;
    float required_scale = 0.0f;

    int matching_unit_active_count = 0;
};

struct RuleEvaluationResult {
    bool matched = false;
    std::size_t rule_index = 0;
    ResolvedStateAction action;
};

using UnitRuleFactsProvider =
    std::function<UnitRuleFacts(const CompiledStateRule&, std::size_t rule_index)>;

UnitRuleConditionKind classify_unit_rule_condition(std::string_view condition);
ResolvedStateAction resolve_state_action(const UnitDefinition& unit, std::string_view action);
CompiledUnitBehavior compile_unit_behavior(const UnitDefinition& unit);

// PPC-confirmed predicate semantics from the 17-way dispatch in 0x15550.
[[nodiscard]] bool evaluate_unit_rule_condition(
    UnitRuleConditionKind condition,
    const UnitRuleFacts& facts,
    int rule_range);

// The original scans the five slots in file order and stops after the first
// true condition even when that rule's action label resolves to a no-op.
[[nodiscard]] RuleEvaluationResult evaluate_first_matching_rule(
    const CompiledUnitStateBehavior& state,
    const UnitRuleFactsProvider& facts_for_rule);

} // namespace deimos
