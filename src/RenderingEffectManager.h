#pragma once

#include "RenderingManager.h"
#include "RenderingShaderManager.h"
#include "ToggleGroupResourceManager.h"

namespace Rendering
{
    class __declspec(novtable) RenderingEffectManager final
    {
    public:
        RenderingEffectManager(AddonImGui::AddonUIData& data, ResourceManager& rManager, RenderingShaderManager& shManager, ToggleGroupResourceManager& tgrManager);
        ~RenderingEffectManager();

        void RenderEffects(reshade::api::command_list* cmd_list, uint64_t callLocation = CALL_DRAW, uint64_t invocation = MATCH_NONE);
        bool RenderRemainingEffects(reshade::api::effect_runtime* runtime);
        void PreventRuntimeReload(reshade::api::command_list* cmd_list);
    private:
        AddonImGui::AddonUIData& uiData;
        ResourceManager& resourceManager;
        RenderingShaderManager& shaderManager;
        ToggleGroupResourceManager& groupResourceManager;

        bool _RenderEffects(
            reshade::api::command_list* cmd_list,
            DeviceDataContainer& deviceData,
            const std::unordered_map<std::string, std::tuple<ShaderToggler::ToggleGroup*, uint64_t, reshade::api::resource>>& techniquesToRender,
            std::vector<std::string>& removalList,
            const std::unordered_set<std::string>& toRenderNames);
    };
}