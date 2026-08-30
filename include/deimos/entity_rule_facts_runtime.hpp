#pragma once

#include "deimos/entity_world.hpp"
#include "deimos/unit_behavior.hpp"

namespace deimos {

// Visual facts sampled by PPC 0x15550 after the per-member animation update.
// The portable world query layer owns the spatial/count predicates; callers
// that have a sprite runtime may supply these scalar/animation facts without
// coupling EntityWorld to rendering.
struct EntityRuleVisualFacts {
    bool animation_stopped = false;
    float visibility = 0.0f;
    float required_visibility = 0.0f;
    float tint = 0.0f;
    float required_tint = 0.0f;
    float scale = 0.0f;
    float required_scale = 0.0f;
};

// Build one rule slot's world facts using the recovered 0x15550 query
// contracts. FourCC none/NULL/empty intentionally matches no entity. A zero
// spatial range means the Unit-ID query is global; a nonzero range uses the
// same strict-distance convention as the recovered player range transition.
[[nodiscard]] UnitRuleFacts build_entity_rule_facts(
    const EntityWorld& world,
    const PlayerWorld& players,
    const EntityRuntime& self,
    const CompiledStateRule& rule,
    const EntityRuleVisualFacts& visual = {});

} // namespace deimos
