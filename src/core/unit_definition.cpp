#include "deimos/unit_definition.hpp"

#include <algorithm>

namespace deimos {
namespace {

void fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
}

struct Cursor {
    const std::vector<TaggedRecord>& records;
    std::size_t pos = 0;
    std::string* error = nullptr;

    const TaggedRecord* peek() const { return pos < records.size() ? &records[pos] : nullptr; }

    const TaggedRecord* expect(std::string_view key) {
        if (pos >= records.size()) {
            fail(error, "unexpected end of unit definition; expected " + std::string(key));
            return nullptr;
        }
        const auto& record = records[pos];
        if (record.key != key) {
            fail(error, "line " + std::to_string(record.source_line) + ": expected " +
                        std::string(key) + ", found " + record.key);
            return nullptr;
        }
        ++pos;
        return &record;
    }
};

std::optional<int> record_int(const TaggedRecord* record, std::string* error) {
    if (!record) return std::nullopt;
    if (const auto v = parse_int_value(record->value)) return v;
    fail(error, "line " + std::to_string(record->source_line) + ": invalid integer for " + record->key);
    return std::nullopt;
}

std::optional<bool> record_bool(const TaggedRecord* record, std::string* error) {
    if (!record) return std::nullopt;
    if (const auto v = parse_bool_value(record->value)) return v;
    fail(error, "line " + std::to_string(record->source_line) + ": invalid Boolean for " + record->key);
    return std::nullopt;
}

std::optional<FourCC> record_id(const TaggedRecord* record, std::string* error) {
    if (!record) return std::nullopt;
    if (const auto v = parse_id_value(record->value)) return v;
    fail(error, "line " + std::to_string(record->source_line) + ": invalid FourCC for " + record->key);
    return std::nullopt;
}

std::optional<UnitSpawnSet> parse_spawn_set(Cursor& cursor) {
    UnitSpawnSet out;
    const auto* name = cursor.expect("stateSpawnSetName_STR");
    const auto spawn = record_id(cursor.expect("stateSpawnSetSpawn_ID"), cursor.error);
    const auto x = record_int(cursor.expect("stateSpawnSetXOffset_INT"), cursor.error);
    const auto y = record_int(cursor.expect("stateSpawnSetYOffset_INT"), cursor.error);
    const auto adjust = record_bool(cursor.expect("stateSpawnSetAdjustOffsetForUnitRotation_BOOL"), cursor.error);
    const auto absolute = record_bool(cursor.expect("stateSpawnSet_AbsoluteCoordinates_BOOL"), cursor.error);
    const auto rate_min = record_int(cursor.expect("stateSpawnSetRateMin_INT"), cursor.error);
    const auto rate_max = record_int(cursor.expect("stateSpawnSetRateMax_INT"), cursor.error);
    const auto volley_min = record_int(cursor.expect("stateSpawnSetNumInVolleyMin_INT"), cursor.error);
    const auto volley_max = record_int(cursor.expect("stateSpawnSetNumInVolleyMax_INT"), cursor.error);
    const auto delay_min = record_int(cursor.expect("stateSpawnSetDelayBetweenEntitiesMin_INT"), cursor.error);
    const auto delay_max = record_int(cursor.expect("stateSpawnSetDelayBetweenEntitiesMax_INT"), cursor.error);
    const auto repeat = record_bool(cursor.expect("stateSpawnSetRepeatSpawns_BOOL"), cursor.error);
    const auto offscreen = record_bool(cursor.expect("stateSpawnSetDon'tSpawnOffscreen_BOOL"), cursor.error);
    const auto pause = record_bool(cursor.expect("stateSpawnSetPauseAnyRotationWhileSpawning_BOOL"), cursor.error);
    const auto pause_time = record_int(cursor.expect("stateSpawnSetTimeToPauseRotationAfterSpawning_INT"), cursor.error);
    const auto fleeing = record_bool(cursor.expect("stateSpawnSetSpawnIfFleeing_BOOL"), cursor.error);
    const auto stationary = record_bool(cursor.expect("stateSpawnSet_StationaryOption_BOOL"), cursor.error);
    const auto terrain = record_bool(cursor.expect("stateSpawnSet_TerrainEffectsOption_BOOL"), cursor.error);
    const auto set_heading = record_bool(cursor.expect("stateSpawnSetSetHeading_BOOL"), cursor.error);
    const auto heading = record_int(cursor.expect("stateSpawnSetHeadingDegrees_INT"), cursor.error);
    if (!name || !spawn || !x || !y || !adjust || !absolute || !rate_min || !rate_max ||
        !volley_min || !volley_max || !delay_min || !delay_max || !repeat || !offscreen ||
        !pause || !pause_time || !fleeing || !stationary || !terrain || !set_heading || !heading) {
        return std::nullopt;
    }
    out.name = name->value;
    out.spawn_id = *spawn;
    out.x_offset = *x;
    out.y_offset = *y;
    out.adjust_offset_for_unit_rotation = *adjust;
    out.absolute_coordinates = *absolute;
    out.rate_min = *rate_min;
    out.rate_max = *rate_max;
    out.num_in_volley_min = *volley_min;
    out.num_in_volley_max = *volley_max;
    out.delay_between_entities_min = *delay_min;
    out.delay_between_entities_max = *delay_max;
    out.repeat_spawns = *repeat;
    out.dont_spawn_offscreen = *offscreen;
    out.pause_rotation_while_spawning = *pause;
    out.time_to_pause_rotation_after_spawning = *pause_time;
    out.spawn_if_fleeing = *fleeing;
    out.stationary_option = *stationary;
    out.terrain_effects_option = *terrain;
    out.set_heading = *set_heading;
    out.heading_degrees = *heading;
    return out;
}

std::optional<UnitStateRule> parse_rule(Cursor& cursor) {
    UnitStateRule out;
    const auto* name = cursor.expect("stateRuleName_STR");
    const auto unit = record_id(cursor.expect("stateRuleUnit_ID"), cursor.error);
    const auto range = record_int(cursor.expect("stateRuleRange_INT"), cursor.error);
    const auto* condition = cursor.expect("stateRuleCondition_STR");
    const auto* action = cursor.expect("stateRuleAction_STR");
    if (!name || !unit || !range || !condition || !action) return std::nullopt;
    out.name = name->value;
    out.unit_id = *unit;
    out.range = *range;
    out.condition = condition->value;
    out.action = action->value;
    return out;
}

bool add_field(const TaggedRecord& record, DefinitionFieldSet& fields, std::string* error) {
    auto field = parse_definition_field(record, error);
    if (!field) return false;
    fields.add(std::move(*field));
    return true;
}

} // namespace

std::optional<std::size_t> UnitDefinition::find_state(std::string_view state_name) const {
    for (std::size_t i = 0; i < states.size(); ++i) {
        // PPC helper 0x57820 is a byte-exact strcmp and the state-transition
        // routine uses it directly.  Case-only mismatches in canonical data
        // therefore remain unresolved/no-op, exactly as in 1.0.6.
        if (states[i].name == state_name) return i;
    }
    return std::nullopt;
}

std::optional<UnitDefinition> parse_unit_definition_document(
    const TaggedTextDocument& document, std::string* error) {
    if (!document.bare_lines.empty()) {
        fail(error, "unit definition contains unexpected bare text");
        return std::nullopt;
    }
    Cursor cursor{document.records, 0, error};
    UnitDefinition out;

    // Core block ends at the declared state count. Preserve every field in
    // order so newly understood semantics never require re-decoding evidence.
    while (cursor.peek() && cursor.peek()->key != "numStates_INT") {
        if (!add_field(*cursor.peek(), out.core_fields, error)) return std::nullopt;
        ++cursor.pos;
    }
    const auto state_count = record_int(cursor.expect("numStates_INT"), error);
    // The original 1.0.6 G_UnitDefinitions loader asserts that the decoded
    // state count is in the inclusive range 1..20. Preserve that binary-
    // confirmed contract here instead of accepting structurally impossible
    // unit data.
    constexpr int kMaxUnitStates = 20;
    if (!state_count || *state_count <= 0 || *state_count > kMaxUnitStates) {
        fail(error, "unit definition state count must be in range 1..20");
        return std::nullopt;
    }

    const auto name = out.core_fields.string_value("name_STR");
    const auto family = out.core_fields.string_value("familyName_STR");
    const auto description = out.core_fields.string_value("description_STR");
    if (!name || !family || !description) {
        fail(error, "unit definition is missing core name/family/description fields");
        return std::nullopt;
    }
    out.name = std::string(*name);
    out.family_name = std::string(*family);
    out.description = std::string(*description);
    out.states.reserve(static_cast<std::size_t>(*state_count));

    for (int state_index = 0; state_index < *state_count; ++state_index) {
        UnitStateDefinition state;
        const auto* state_name = cursor.expect("stateName_STR");
        if (!state_name) return std::nullopt;
        state.name = state_name->value;

        while (cursor.peek() && cursor.peek()->key != "stateNumSpawnSets_INT") {
            if (cursor.peek()->key == "stateName_STR") {
                fail(error, "state block ended before stateNumSpawnSets_INT");
                return std::nullopt;
            }
            if (!add_field(*cursor.peek(), state.fields, error)) return std::nullopt;
            ++cursor.pos;
        }

        const auto spawn_count = record_int(cursor.expect("stateNumSpawnSets_INT"), error);
        if (!spawn_count || *spawn_count < 0) return std::nullopt;
        state.spawn_sets.reserve(static_cast<std::size_t>(*spawn_count));
        for (int i = 0; i < *spawn_count; ++i) {
            auto spawn = parse_spawn_set(cursor);
            if (!spawn) return std::nullopt;
            state.spawn_sets.push_back(std::move(*spawn));
        }

        while (cursor.peek() && cursor.peek()->key != "stateNumRules_INT") {
            if (cursor.peek()->key == "stateName_STR") {
                fail(error, "state block ended before stateNumRules_INT");
                return std::nullopt;
            }
            if (!add_field(*cursor.peek(), state.fields, error)) return std::nullopt;
            ++cursor.pos;
        }

        const auto rule_count = record_int(cursor.expect("stateNumRules_INT"), error);
        if (!rule_count || *rule_count < 0) return std::nullopt;
        state.rules.reserve(static_cast<std::size_t>(*rule_count));
        for (int i = 0; i < *rule_count; ++i) {
            auto rule = parse_rule(cursor);
            if (!rule) return std::nullopt;
            state.rules.push_back(std::move(*rule));
        }
        out.states.push_back(std::move(state));
    }

    if (cursor.pos != document.records.size()) {
        fail(error, "unexpected tagged records after declared unit states");
        return std::nullopt;
    }
    return out;
}

std::optional<UnitDefinition> decode_and_parse_unit_definition(
    std::span<const std::uint8_t> encoded, std::string* error) {
    const auto decoded = decode_legacy_text(encoded);
    const auto document = parse_tagged_text(decoded, error);
    if (!document) return std::nullopt;
    return parse_unit_definition_document(*document, error);
}

} // namespace deimos
