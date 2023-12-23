#pragma once

#include <vector>
#include <unordered_map>
#include <tuple>
#include <shared_mutex>
#include <chrono>
#include "reshade.hpp"
#include "CDataFile.h"
#include "ToggleGroup.h"
#include "EffectData.h"

struct __declspec(novtable) ResourceRenderData final {
    constexpr ResourceRenderData() : group(nullptr), invocationLocation(0), resource({0}), format(reshade::api::format::unknown) { }
    constexpr ResourceRenderData(ShaderToggler::ToggleGroup* g, uint64_t i, reshade::api::resource r, reshade::api::format f) : group(g), invocationLocation(i), resource(r), format(f) { }

    ShaderToggler::ToggleGroup* group;
    uint64_t invocationLocation;
    reshade::api::resource resource;
    reshade::api::format format;
};

using effect_queue = std::unordered_map<EffectData*, ResourceRenderData>;
using binding_queue = std::unordered_map<ShaderToggler::ToggleGroup*, ResourceRenderData>;

struct __declspec(novtable) ShaderData final {
    uint32_t activeShaderHash = -1;
    binding_queue bindingsToUpdate;
    std::unordered_set<ShaderToggler::ToggleGroup*> constantBuffersToUpdate;
    effect_queue techniquesToRender;
    std::unordered_set<ShaderToggler::ToggleGroup*> srvToUpdate;
    const std::vector<ShaderToggler::ToggleGroup*>* blockedShaderGroups = nullptr;
    uint32_t id = 0;

    ShaderData(uint32_t _id) : id(_id) { }

    void Reset()
    {
        activeShaderHash = -1;
        bindingsToUpdate.clear();
        constantBuffersToUpdate.clear();
        techniquesToRender.clear();
        srvToUpdate.clear();
        blockedShaderGroups = nullptr;
    }
};

struct __declspec(uuid("222F7169-3C09-40DB-9BC9-EC53842CE537")) CommandListDataContainer {
    uint64_t commandQueue = 0;
    ShaderData ps{ 0 };
    ShaderData vs{ 1 };
    ShaderData cs{ 2 };

    void Reset()
    {
        ps.Reset();
        vs.Reset();
        cs.Reset();

        commandQueue = 0;
    }
};

struct __declspec(novtable) TextureBindingData final
{
    reshade::api::resource res;
    reshade::api::format format;
    reshade::api::resource_view srv;
    reshade::api::resource_view rtv;
    uint32_t width;
    uint32_t height;
    uint16_t levels;
    bool enabled_reset_on_miss;
    bool copy;
    bool reset = false;
};

struct __declspec(novtable) HuntPreview final
{
    reshade::api::resource target = reshade::api::resource{ 0 };
    bool matched = false;
    uint64_t target_invocation_location = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    reshade::api::format format = reshade::api::format::unknown;
    reshade::api::format view_format = reshade::api::format::unknown;
    reshade::api::resource_desc target_desc;
    bool recreate_preview = false;

    void Reset()
    {
        matched = false;
        target = reshade::api::resource{ 0 };
        target_invocation_location = 0;
        width = 0;
        height = 0;
        format = reshade::api::format::unknown;
        recreate_preview = false;
    }
};

struct __declspec(novtable) SpecialEffect final
{
    std::string name;
    reshade::api::effect_technique technique;
};

enum SpecialEffects : uint32_t
{
    REST_TONEMAP_TO_SDR = 0,
    REST_TONEMAP_TO_HDR,
    REST_FLIP,
    REST_NOOP,
    REST_EFFECTS_COUNT
};

struct __declspec(uuid("C63E95B1-4E2F-46D6-A276-E8B4612C069A")) DeviceDataContainer {
    reshade::api::effect_runtime* current_runtime = nullptr;
    std::atomic_bool rendered_effects = false;
    std::shared_mutex binding_mutex;
    std::shared_mutex render_mutex;
    std::unordered_set<const ShaderToggler::ToggleGroup*> bindingsUpdated;
    std::unordered_set<const ShaderToggler::ToggleGroup*> constantsUpdated;
    std::unordered_set<const ShaderToggler::ToggleGroup*> srvUpdated;
    HuntPreview huntPreview;
};

struct __declspec(uuid("838BAF1D-95C0-4A7E-A517-052642879986")) RuntimeDataContainer {
    std::shared_mutex technique_mutex;
    std::unordered_map<std::string, EffectData> allTechniques;
    std::unordered_set<EffectData*> allEnabledTechniques;
    std::vector<EffectData*> allSortedTechniques;

    SpecialEffect specialEffects[4] = {
        SpecialEffect{ "REST_TONEMAP_TO_SDR", reshade::api::effect_technique {0} },
        SpecialEffect{ "REST_TONEMAP_TO_HDR", reshade::api::effect_technique {0} },
        SpecialEffect{ "REST_FLIP", reshade::api::effect_technique {0} },
        SpecialEffect{ "REST_NOOP", reshade::api::effect_technique {0} },
    };
    int32_t previousEnableCount = 0;
};