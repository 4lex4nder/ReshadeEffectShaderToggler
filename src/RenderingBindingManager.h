#pragma once

#include "RenderingManager.h"
#include "ToggleGroupResourceManager.h"

namespace Rendering
{
    class __declspec(novtable) RenderingBindingManager final
    {
    public:
        RenderingBindingManager(AddonImGui::AddonUIData& data, ResourceManager& rManager, ToggleGroupResourceManager& tgResources);
        ~RenderingBindingManager();

        bool CreateTextureBinding(reshade::api::effect_runtime* runtime, reshade::api::resource* res, reshade::api::resource_view* srv, reshade::api::resource_view* rtv, const reshade::api::resource_desc& desc);
        bool CreateTextureBinding(reshade::api::effect_runtime* runtime, reshade::api::resource* res, reshade::api::resource_view* srv, reshade::api::resource_view* rtv, reshade::api::format format);
        uint32_t UpdateTextureBinding(reshade::api::effect_runtime* runtime, ShaderToggler::ToggleGroup* group, reshade::api::resource res, const reshade::api::resource_desc& desc);
        void InitTextureBingings(reshade::api::effect_runtime* runtime);
        void DisposeTextureBindings(reshade::api::effect_runtime* runtime);
        void UpdateTextureBindings(reshade::api::command_list* cmd_list, uint64_t callLocation = CALL_DRAW, uint64_t invocation = MATCH_NONE);
        void ClearUnmatchedTextureBindings(reshade::api::command_list* cmd_list);
    private:
        AddonImGui::AddonUIData& uiData;
        ResourceManager& resourceManager;
        ToggleGroupResourceManager& toggleGroupResources;

        reshade::api::resource empty_res = { 0 };
        reshade::api::resource_view empty_srv = { 0 };
        reshade::api::resource_view empty_rtv = { 0 };

        void _UpdateTextureBindings(reshade::api::command_list* cmd_list,
            DeviceDataContainer& deviceData,
            const binding_queue& bindingsToUpdate,
            std::vector<ShaderToggler::ToggleGroup*>& removalList,
            const std::unordered_set<ShaderToggler::ToggleGroup*>& toUpdateBindings);
        bool _CreateTextureBinding(reshade::api::effect_runtime* runtime,
            reshade::api::resource* res,
            reshade::api::resource_view* srv,
            reshade::api::resource_view* rtv,
            reshade::api::format format,
            uint32_t width,
            uint32_t height,
            uint16_t levels);
        void _QueueOrDequeue(
            reshade::api::command_list* cmd_list,
            DeviceDataContainer& deviceData,
            CommandListDataContainer& commandListData,
            binding_queue& queue,
            std::unordered_set<ShaderToggler::ToggleGroup*>& immediateQueue,
            uint64_t callLocation,
            uint32_t layoutIndex,
            uint64_t action);
    };
}