#include "TechniqueManager.h"

using namespace reshade::api;
using namespace ShaderToggler;
using namespace std;

size_t TechniqueManager::charBufferSize = CHAR_BUFFER_SIZE;
char TechniqueManager::charBuffer[CHAR_BUFFER_SIZE];

TechniqueManager::TechniqueManager(KeyMonitor& kMonitor, vector<string>& techniqueCollection) : keyMonitor(kMonitor), allTechniques(techniqueCollection)
{

}

void TechniqueManager::AddEffectsReloadingCallback(std::function<void(reshade::api::effect_runtime*)> callback)
{
    effectsReloadingCallback.push_back(callback);
}

void TechniqueManager::AddEffectsReloadedCallback(std::function<void(reshade::api::effect_runtime*)> callback)
{
    effectsReloadedCallback.push_back(callback);
}

void TechniqueManager::SignalEffectsReloading(reshade::api::effect_runtime* runtime)
{
    for (auto& func : effectsReloadingCallback)
    {
        func(runtime);
    }
}

void TechniqueManager::SignalEffectsReloaded(reshade::api::effect_runtime* runtime)
{
    for (auto& func : effectsReloadedCallback)
    {
        func(runtime);
    }
}

void TechniqueManager::OnReshadeReloadedEffects(reshade::api::effect_runtime* runtime)
{
    RuntimeDataContainer& data = runtime->get_private_data<RuntimeDataContainer>();
    unique_lock<shared_mutex> lock(data.technique_mutex);

    data.allEnabledTechniques.clear();
    data.allTechniques.clear();
    allTechniques.clear();

    Rendering::RenderingManager::EnumerateTechniques(runtime, [&data, this](effect_runtime* runtime, effect_technique technique, string& name, string& eff_name) {
        allTechniques.push_back(name);
        bool enabled = runtime->get_technique_state(technique);

        // Assign technique handles to REST effects
        bool builtin = false;
        for (uint32_t j = 0; j < REST_EFFECTS_COUNT; j++)
        {
            if (name == data.specialEffects[j].name)
            {
                data.specialEffects[j].technique = technique;
                builtin = true;
                break;
            }
        }

        if (builtin)
        {
            return;
        }

        const auto& it = data.allTechniques.emplace(name, EffectData{ technique, runtime, enabled, name, eff_name });
    
        if (enabled)
        {
            data.allEnabledTechniques.emplace(name, &it.first->second);
        }
        });

    int32_t enabledCount = static_cast<int32_t>(data.allTechniques.size());

    if (enabledCount == 0 || enabledCount - data.previousEnableCount < 0)
    {
        SignalEffectsReloading(runtime);
    }
    else
    {
        SignalEffectsReloaded(runtime);
    }

    data.previousEnableCount = enabledCount;
}

bool TechniqueManager::OnReshadeSetTechniqueState(reshade::api::effect_runtime* runtime, reshade::api::effect_technique technique, bool enabled)
{
    RuntimeDataContainer& data = runtime->get_private_data<RuntimeDataContainer>();
    unique_lock<shared_mutex> lock(data.technique_mutex);

    charBufferSize = CHAR_BUFFER_SIZE;
    runtime->get_technique_name(technique, charBuffer, &charBufferSize);
    string techName(charBuffer);

    // Prevent REST techniques from being manually enabled
    for (uint32_t j = 0; j < REST_EFFECTS_COUNT; j++)
    {
        if (techName == data.specialEffects[j].name)
        {
            return true;
        }
    }

    const auto& it = data.allTechniques.find(techName);

    if (it == data.allTechniques.end())
    {
        return false;
    }

    it->second.enabled = enabled;

    if (!enabled)
    {
        if (data.allEnabledTechniques.contains(techName))
        {
            data.allEnabledTechniques.erase(techName);
        }
    }
    else
    {
        if (data.allEnabledTechniques.find(techName) == data.allEnabledTechniques.end())
        {
            data.allEnabledTechniques.emplace(techName, &it->second);
        }
    }

    return false;
}

bool TechniqueManager::OnReshadeReorderTechniques(reshade::api::effect_runtime* runtime, size_t count, reshade::api::effect_technique* techniques)
{
    RuntimeDataContainer& data = runtime->get_private_data<RuntimeDataContainer>();
    unique_lock<shared_mutex> lock(data.technique_mutex);

    data.allEnabledTechniques.clear();
    data.allTechniques.clear();

    for (uint32_t i = 0; i < count; i++)
    {
        effect_technique technique = techniques[i];

        charBufferSize = CHAR_BUFFER_SIZE;
        runtime->get_technique_name(technique, charBuffer, &charBufferSize);
        string name(charBuffer);

        charBufferSize = CHAR_BUFFER_SIZE;
        runtime->get_technique_effect_name(technique, charBuffer, &charBufferSize);
        string eff_name(charBuffer);

        bool enabled = runtime->get_technique_state(technique);

        // Assign technique handles to REST effects
        bool builtin = false;
        for (uint32_t j = 0; j < REST_EFFECTS_COUNT; j++)
        {
            if (name == data.specialEffects[j].name)
            {
                data.specialEffects[j].technique = technique;
                builtin = true;
                break;
            }
        }

        if (builtin)
        {
            continue;
        }

        const auto& it = data.allTechniques.emplace(name, EffectData{ technique, runtime, enabled, name, eff_name });

        if (enabled)
        {
            data.allEnabledTechniques.emplace(name, &it.first->second);
        }
    }

    return false;
}

void TechniqueManager::OnReshadePresent(reshade::api::effect_runtime* runtime)
{
    RuntimeDataContainer& deviceData = runtime->get_private_data<RuntimeDataContainer>();
    unique_lock<shared_mutex> lock(deviceData.technique_mutex);

    for (auto el = deviceData.allEnabledTechniques.begin(); el != deviceData.allEnabledTechniques.end();)
    {
        // Get rid of techniques with a timeout. We don't actually have a timer, so just get rid of them after they were rendered at least once
        if (el->second->timeout >= 0 &&
            el->second->technique != 0 &&
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - el->second->timeout_start).count() >= el->second->timeout)
        {
            runtime->set_technique_state(el->second->technique, false);
            el = deviceData.allEnabledTechniques.erase(el);
            continue;
        }

        // Prevent effects that are not supposed to be in screenshots from being rendered when ReShade is taking a screenshot
        if (!el->second->enabled_in_screenshot && keyMonitor.GetKeyState(KeyMonitor::KEY_SCREEN_SHOT) == KeyState::KET_STATE_PRESSED)
        {
            el->second->rendered = true;
        }
        else
        {
            el->second->rendered = false;
        }

        el++;
    }
}