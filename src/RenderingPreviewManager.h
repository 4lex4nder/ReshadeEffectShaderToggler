#pragma once

#include "RenderingManager.h"
#include "RenderingShaderManager.h"

namespace Rendering
{
    class __declspec(novtable) RenderingPreviewManager final
    {
    public:
        RenderingPreviewManager(AddonImGui::AddonUIData& data, ResourceManager& rManager, RenderingShaderManager& shManager);
        ~RenderingPreviewManager();

        void UpdatePreview(reshade::api::command_list* cmd_list, uint64_t callLocation, uint64_t invocation);
        const ResourceViewData GetCurrentPreviewResourceView(reshade::api::command_list* cmd_list, DeviceDataContainer& deviceData, const ShaderToggler::ToggleGroup* group, CommandListDataContainer& commandListData, uint32_t descIndex, uint64_t action);

    private:
        AddonImGui::AddonUIData& uiData;
        ResourceManager& resourceManager;
        RenderingShaderManager& shaderManager;
    };
}