#pragma once
#include <chrono>
#include "reshade.hpp"

struct __declspec(novtable) EffectData final {
    constexpr EffectData() : rendered(false), enabled_in_screenshot(true), technique({}), timeout(-1) {}
    constexpr EffectData(reshade::api::effect_technique tech, reshade::api::effect_runtime* runtime) : EffectData(tech, runtime, false) {}
    constexpr EffectData(reshade::api::effect_technique tech, reshade::api::effect_runtime* runtime, bool active)
    {
        if (!runtime->get_annotation_bool_from_technique(tech, "enabled_in_screenshot", &enabled_in_screenshot, 1))
        {
            enabled_in_screenshot = true;
        }

        if (!runtime->get_annotation_int_from_technique(tech, "timeout", &timeout, 1))
        {
            timeout = -1;
        }
        else
        {
            timeout_start = std::chrono::steady_clock::now();
        }

        rendered = false;
        technique = tech;
        enabled = active;
    }
    constexpr EffectData(reshade::api::effect_technique tech, reshade::api::effect_runtime* runtime, bool active, std::string& tech_name, std::string& eff_name) : EffectData(tech, runtime, active)
    {
        technique_name = tech_name;
        effect_name = eff_name;
    }

    bool rendered = false;
    bool enabled_in_screenshot = true;
    bool enabled = false;
    reshade::api::effect_technique technique = {};
    std::string technique_name;
    std::string effect_name;
    int32_t timeout = -1;
    std::chrono::steady_clock::time_point timeout_start;
};