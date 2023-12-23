#include <d3d12.h>
#include "RenderingPreviewManager.h"
#include "StateTracking.h"
#include "resource.h"
#include "Util.h"

using namespace Rendering;
using namespace ShaderToggler;
using namespace reshade::api;
using namespace std;

RenderingPreviewManager::RenderingPreviewManager(AddonImGui::AddonUIData& data, ResourceManager& rManager, RenderingShaderManager& shManager) : uiData(data), resourceManager(rManager), shaderManager(shManager)
{
}

RenderingPreviewManager::~RenderingPreviewManager()
{
}

const ResourceViewData RenderingPreviewManager::GetCurrentPreviewResourceView(command_list* cmd_list, DeviceDataContainer& deviceData, const ToggleGroup* group, CommandListDataContainer& commandListData, uint32_t descIndex, uint64_t action)
{
    ResourceViewData active_data;

    if (deviceData.current_runtime == nullptr)
    {
        return active_data;
    }

    device* device = deviceData.current_runtime->get_device();

    state_tracking& state = cmd_list->get_private_data<state_tracking>();
    const vector<resource_view>& rtvs = state.render_targets;

    size_t index = group->getRenderTargetIndex();
    index = std::min(index, rtvs.size() - 1);

    if (rtvs.size() > 0 && rtvs[index] != 0)
    {
        resource rs = device->get_resource_from_view(rtvs[index]);

        if (rs == 0)
        {
            // Render targets may not have a resource bound in D3D12, in which case writes to them are discarded
            return active_data;
        }

        // Don't apply effects to non-RGB buffers
        resource_desc desc = device->get_resource_desc(rs);
        resource_view_desc view_desc = device->get_resource_view_desc(rtvs[index]);

        // Make sure our target matches swap buffer dimensions when applying effects or it's explicitly requested
        if (group->getMatchSwapchainResolution() < ShaderToggler::SWAPCHAIN_MATCH_MODE_NONE)
        {
            uint32_t width, height;
            deviceData.current_runtime->get_screenshot_width_and_height(&width, &height);

            if ((group->getMatchSwapchainResolution() >= ShaderToggler::SWAPCHAIN_MATCH_MODE_ASPECT_RATIO &&
                !RenderingManager::check_aspect_ratio(static_cast<float>(desc.texture.width), static_cast<float>(desc.texture.height), width, height, group->getMatchSwapchainResolution())) ||
                (group->getMatchSwapchainResolution() == ShaderToggler::SWAPCHAIN_MATCH_MODE_RESOLUTION && (width != desc.texture.width || height != desc.texture.height)))
            {
                return active_data;
            }
        }

        active_data.resource = rs;
        active_data.format = view_desc.format;
    }

    return active_data;
}

void RenderingPreviewManager::UpdatePreview(command_list* cmd_list, uint64_t callLocation, uint64_t invocation)
{
    if (cmd_list == nullptr || cmd_list->get_device() == nullptr)
    {
        return;
    }

    device* device = cmd_list->get_device();
    CommandListDataContainer& commandListData = cmd_list->get_private_data<CommandListDataContainer>();
    DeviceDataContainer& deviceData = device->get_private_data<DeviceDataContainer>();

    // Remove call location from queue
    commandListData.commandQueue &= ~(invocation << (callLocation * MATCH_DELIMITER));

    if (deviceData.current_runtime == nullptr || uiData.GetToggleGroupIdShaderEditing() < 0) {
        return;
    }

    RuntimeDataContainer& runtimeData = deviceData.current_runtime->get_private_data<RuntimeDataContainer>();

    const ToggleGroup& group = uiData.GetToggleGroups().at(uiData.GetToggleGroupIdShaderEditing());

    // Set views during draw call since we can be sure the correct ones are bound at that point
    if (!callLocation && deviceData.huntPreview.target == 0)
    {
        ResourceViewData active_target;

        if (invocation & MATCH_PREVIEW_PS)
        {
            active_target = GetCurrentPreviewResourceView(cmd_list, deviceData, &group, commandListData, 0, invocation & MATCH_PREVIEW_PS);
        }
        else if (invocation & MATCH_PREVIEW_VS)
        {
            active_target = GetCurrentPreviewResourceView(cmd_list, deviceData, &group, commandListData, 1, invocation & MATCH_PREVIEW_VS);
        }
        else if (invocation & MATCH_PREVIEW_CS)
        {
            active_target = GetCurrentPreviewResourceView(cmd_list, deviceData, &group, commandListData, 2, invocation & MATCH_PREVIEW_CS);
        }

        if (active_target.resource != 0)
        {
            resource_desc desc = device->get_resource_desc(active_target.resource);
            //cmd_list->get_private_data<state_tracking>().start_resource_barrier_tracking(res, resource_usage::render_target);

            deviceData.huntPreview.target = active_target.resource;
            deviceData.huntPreview.target_desc = desc;
            deviceData.huntPreview.format = desc.texture.format;
            deviceData.huntPreview.view_format = active_target.format;
            deviceData.huntPreview.width = desc.texture.width;
            deviceData.huntPreview.height = desc.texture.height;
        }
        else
        {
            return;
        }
    }

    if (deviceData.huntPreview.target == 0 || !(!callLocation && !deviceData.huntPreview.target_invocation_location || callLocation & deviceData.huntPreview.target_invocation_location))
    {
        return;
    }

    if (group.getId() == uiData.GetToggleGroupIdShaderEditing() && !deviceData.huntPreview.matched)
    {
        resource rs = deviceData.huntPreview.target;
        //resource_usage rs_usage = cmd_list->get_private_data<state_tracking>().stop_resource_barrier_tracking(rs);
        //if (rs_usage == resource_usage::undefined)
        //{
        //    return;
        //}

        if (!resourceManager.IsCompatibleWithPreviewFormat(deviceData.current_runtime, rs))
        {
            deviceData.huntPreview.recreate_preview = true;
        }
        else
        {
            bool supportsAlphaClear = device->get_api() == device_api::d3d9 || device->get_api() == device_api::d3d10 || device->get_api() == device_api::d3d11 || device->get_api() == device_api::d3d12;

            resource previewResPing = resource{ 0 };
            resource previewResPong = resource{ 0 };
            resource_view preview_pong_rtv = resource_view{ 0 };
            resource_view preview_ping_srv = resource_view{ 0 };

            resourceManager.SetPingPreviewHandles(&previewResPing, nullptr, &preview_ping_srv);
            resourceManager.SetPongPreviewHandles(&previewResPong, &preview_pong_rtv, nullptr);

            if (previewResPong != 0 && (!group.getClearPreviewAlpha() || !supportsAlphaClear))
            {
                //resource resources[2] = { rs, previewResPong };
                //resource_usage from[2] = { rs_usage, resource_usage::shader_resource };
                //resource_usage to[2] = { resource_usage::copy_source, resource_usage::copy_dest };
                
                //cmd_list->barrier(2, resources, from, to);
                cmd_list->copy_resource(rs, previewResPong);
                //cmd_list->barrier(2, resources, to, from);
            }
            else if (previewResPing != 0 && previewResPong != 0 && preview_ping_srv != 0 && preview_pong_rtv != 0 && supportsAlphaClear)
            {
                //resource resources[2] = { rs, previewResPing };
                //resource_usage from[2] = { rs_usage, resource_usage::shader_resource };
                //resource_usage to[2] = { resource_usage::copy_source, resource_usage::copy_dest };
                
                //cmd_list->barrier(2, resources, from, to);
                cmd_list->copy_resource(rs, previewResPing);
                //cmd_list->barrier(2, resources, to, from);

                //cmd_list->barrier(previewResPong, resource_usage::shader_resource, resource_usage::render_target);
                shaderManager.CopyResource(cmd_list, preview_ping_srv, preview_pong_rtv, deviceData.huntPreview.width, deviceData.huntPreview.height);
                //cmd_list->barrier(previewResPong, resource_usage::render_target, resource_usage::shader_resource);
            }

            if (group.getFlipBuffer() && runtimeData.specialEffects[REST_FLIP].technique != 0)
            {
                Util::Rendering::render_technique(deviceData, runtimeData.specialEffects[REST_FLIP].technique, cmd_list, preview_pong_rtv);
            }

            if (group.getToneMap() && runtimeData.specialEffects[REST_TONEMAP_TO_SDR].technique != 0)
            {
                Util::Rendering::render_technique(deviceData, runtimeData.specialEffects[REST_TONEMAP_TO_SDR].technique, cmd_list, preview_pong_rtv);
            }
        }

        deviceData.huntPreview.matched = true;
    }
}