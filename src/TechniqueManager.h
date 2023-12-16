#pragma once

#include <vector>
#include <string>
#include "reshade.hpp"
#include "PipelinePrivateData.h"
#include "RenderingManager.h"
#include "KeyMonitor.h"

namespace ShaderToggler
{
    class __declspec(novtable) TechniqueManager final
    {
    public:
        TechniqueManager(ShaderToggler::KeyMonitor& keyMonitor);

        void OnReshadeReloadedEffects(reshade::api::effect_runtime* runtime);
        bool OnReshadeSetTechniqueState(reshade::api::effect_runtime* runtime, reshade::api::effect_technique technique, bool enabled);
        void OnReshadePresent(reshade::api::effect_runtime* runtime);
        bool OnReshadeReorderTechniques(reshade::api::effect_runtime* runtime, size_t count, reshade::api::effect_technique* techniques);

        void AddEffectsReloadingCallback(std::function<void(reshade::api::effect_runtime*)> callback);
        void SignalEffectsReloading(reshade::api::effect_runtime* runtime);
        void AddEffectsReloadedCallback(std::function<void(reshade::api::effect_runtime*)> callback);
        void SignalEffectsReloaded(reshade::api::effect_runtime* runtime);

    private:
        KeyMonitor& keyMonitor;
        std::vector<std::function<void(reshade::api::effect_runtime*)>> effectsReloadingCallback;
        std::vector<std::function<void(reshade::api::effect_runtime*)>> effectsReloadedCallback;

        static constexpr size_t CHAR_BUFFER_SIZE = 256;
        static char charBuffer[CHAR_BUFFER_SIZE];
        static size_t charBufferSize;
    };
}