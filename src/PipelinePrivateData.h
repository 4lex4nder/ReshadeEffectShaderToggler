#pragma once

#include <vector>
#include <unordered_map>
#include <tuple>
#include "CDataFile.h"
#include "ToggleGroup.h"
#include "PipelineStateTracker.h"

struct __declspec(uuid("222F7169-3C09-40DB-9BC9-EC53842CE537")) CommandListDataContainer {
    uint32_t activePixelShaderHash;
    uint32_t activeVertexShaderHash;
    unordered_map<string, int32_t> techniquesToRender;
    unordered_map<string, int32_t> bindingsToUpdate;
    vector<vector<resource_view>> active_rtv_history = vector<vector<resource_view>>(10);
    unordered_set < pair<string, int32_t>,
        decltype([](const pair<string, int32_t>& v) {
        return std::hash<std::string>{}(v.first); // Don't care about history index, we'll overwrite them in case of collisions
            }),
        decltype([](const pair<string, int32_t>& lhs, const pair<string, int32_t>& rhs) {
                return lhs.first == rhs.first;
            }) > immediateActionSet;
    StateTracker::PipelineStateTracker stateTracker;
    const vector<ToggleGroup*>* blockedPixelShaderGroups;
    const vector<ToggleGroup*>* blockedVertexShaderGroups;
};

struct __declspec(uuid("C63E95B1-4E2F-46D6-A276-E8B4612C069A")) DeviceDataContainer {
    effect_runtime* current_runtime = nullptr;
    atomic_bool rendered_effects = false;
    unordered_map<string, bool> allEnabledTechniques;
    unordered_map<string, tuple<resource, reshade::api::format, resource_view, resource_view>> bindingMap;
    unordered_set<string> bindingsUpdated;
    unordered_set<const ToggleGroup*> constantsUpdated;
    unordered_map<uint64_t, vector<bool>> transient_mask;
    ToggleGroup* huntedGroup = nullptr;
    std::unordered_map<int, ToggleGroup>* groups = nullptr;
};