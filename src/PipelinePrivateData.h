#pragma once

#include <vector>
#include <unordered_map>
#include <tuple>
#include <shared_mutex>
#include "reshade.hpp"
#include "CDataFile.h"
#include "ToggleGroup.h"

using effect_queue = std::unordered_map<std::string, std::tuple<ShaderToggler::ToggleGroup*, uint64_t, reshade::api::resource>>;

struct __declspec(novtable) EffectData final {
    constexpr EffectData() : rendered(false), enabled_in_screenshot(true), timeout(-1) {}
    constexpr EffectData(reshade::api::effect_technique technique, reshade::api::effect_runtime* runtime)
    {
        if (!runtime->get_annotation_bool_from_technique(technique, "enabled_in_screenshot", &enabled_in_screenshot, 1))
        {
            enabled_in_screenshot = true;
        }

        if (!runtime->get_annotation_int_from_technique(technique, "timeout", &timeout, 1))
        {
            timeout = -1;
        }

        rendered = false;
    }

    bool rendered = false;
    bool enabled_in_screenshot = true;
    int32_t timeout = -1;
};

struct __declspec(novtable) ShaderData final {
    uint32_t activeShaderHash = -1;
    effect_queue bindingsToUpdate;
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

struct __declspec(uuid("C63E95B1-4E2F-46D6-A276-E8B4612C069A")) DeviceDataContainer {
    reshade::api::effect_runtime* current_runtime = nullptr;
    std::atomic_bool rendered_effects = false;
    std::shared_mutex render_mutex;
    std::unordered_map<std::string, EffectData> allEnabledTechniques;
    std::shared_mutex binding_mutex;
    std::unordered_map<std::string, TextureBindingData> bindingMap;
    std::unordered_set<std::string> bindingsUpdated;
    std::unordered_set<const ShaderToggler::ToggleGroup*> constantsUpdated;
    std::unordered_set<const ShaderToggler::ToggleGroup*> srvUpdated;
    bool reload_bindings = false;
    HuntPreview huntPreview;
};