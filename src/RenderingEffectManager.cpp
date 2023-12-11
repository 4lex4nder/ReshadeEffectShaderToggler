#include "RenderingEffectManager.h"
#include "StateTracking.h"

using namespace Rendering;
using namespace ShaderToggler;
using namespace reshade::api;
using namespace std;

RenderingEffectManager::RenderingEffectManager(AddonImGui::AddonUIData& data, ResourceManager& rManager, RenderingShaderManager& shManager, ToggleGroupResourceManager& tgrManager) : 
    uiData(data), resourceManager(rManager), shaderManager(shManager), groupResourceManager(tgrManager)
{
}

RenderingEffectManager::~RenderingEffectManager()
{

}

bool RenderingEffectManager::RenderRemainingEffects(effect_runtime* runtime)
{
    if (runtime == nullptr || runtime->get_device() == nullptr)
    {
        return false;
    }

    command_list* cmd_list = runtime->get_command_queue()->get_immediate_command_list();
    device* device = runtime->get_device();
    RuntimeDataContainer& runtimeData = runtime->get_private_data<RuntimeDataContainer>();
    CommandListDataContainer& commandListData = cmd_list->get_private_data<CommandListDataContainer>();
    DeviceDataContainer& deviceData = device->get_private_data<DeviceDataContainer>();
    bool rendered = false;

    resource res = runtime->get_current_back_buffer();

    GlobalResourceView& view = resourceManager.GetResourceView(device, res.handle);
    resource_view active_rtv = view.rtv;
    resource_view active_rtv_srgb = view.rtv_srgb;

    if (active_rtv == 0 || !deviceData.rendered_effects) {
        return false;
    }

    RenderingManager::EnumerateTechniques(runtime, [&runtimeData, &cmd_list, &active_rtv, &active_rtv_srgb, &rendered](effect_runtime* runtime, effect_technique technique, string& name) {
        if (runtimeData.allEnabledTechniques.contains(name) && !runtimeData.allEnabledTechniques[name]->rendered)
        {
            runtime->render_technique(technique, cmd_list, active_rtv, active_rtv_srgb);

            runtimeData.allEnabledTechniques[name]->rendered = true;
            rendered = true;
        }
        });

    return rendered;
}

bool RenderingEffectManager::_RenderEffects(
    command_list* cmd_list,
    DeviceDataContainer& deviceData,
    RuntimeDataContainer& runtimeData,
    const unordered_map<string, tuple<ToggleGroup*, uint64_t, resource>>& techniquesToRender,
    vector<string>& removalList,
    const unordered_set<string>& toRenderNames)
{
    bool rendered = false;
    CommandListDataContainer& cmdData = cmd_list->get_private_data<CommandListDataContainer>();
    effect_runtime* runtime = deviceData.current_runtime;

    unordered_map<ToggleGroup*, pair<vector<pair<string, EffectData*>>, resource>> groupTechMap;

    for (const auto& sTech : runtimeData.allSortedTechniques)
    {
        if (sTech.second->enabled && !sTech.second->rendered && toRenderNames.contains(sTech.first))
        {
            const auto& tech = techniquesToRender.find(sTech.first);

            if (tech != techniquesToRender.end())
            {
                const auto& [techName, techData] = *tech;
                const auto& [group, _, active_resource] = techData;

                const auto& curTechEntry = groupTechMap.find(group);

                if (curTechEntry == groupTechMap.end())
                {
                    const auto& nEntry = groupTechMap.emplace(group, make_pair(vector<pair<string, EffectData*>>(), active_resource));
                    nEntry.first->second.first.push_back(make_pair(techName, sTech.second));
                }
                else
                {
                    curTechEntry->second.first.push_back(make_pair(techName, sTech.second));
                }
            }
        }
    }

    for (const auto& tech : groupTechMap)
    {
        const auto& group = tech.first;
        const auto& [effectList, active_resource] = tech.second;

        if (active_resource == 0)
        {
            continue;
        }

        resource_view view_non_srgb = {};
        resource_view view_srgb = {};
        resource_view group_view = {};
        resource_desc desc = cmd_list->get_device()->get_resource_desc(active_resource);
        GroupResource& groupResource = group->GetGroupResource(GroupResourceType::RESOURCE_ALPHA);
        GlobalResourceView& view = resourceManager.GetResourceView(runtime->get_device(), active_resource.handle);
        bool copyPreserveAlpha = false;

        if (group->getPreserveAlpha())
        {
            if (groupResourceManager.IsCompatibleWithGroupFormat(runtime->get_device(), GroupResourceType::RESOURCE_ALPHA, active_resource, group))
            {
                resource group_res = {};
                groupResourceManager.SetGroupBufferHandles(group, GroupResourceType::RESOURCE_ALPHA, &group_res, &view_non_srgb, &view_srgb, &group_view);
                cmd_list->copy_resource(active_resource, group_res);
                copyPreserveAlpha = true;
            }
            else
            {
                view_non_srgb = view.rtv;
                view_srgb = view.rtv_srgb;

                groupResource.state = GroupResourceState::RESOURCE_INVALID;
                groupResource.target_description = desc;
            }
        }
        else
        {
            view_non_srgb = view.rtv;
            view_srgb = view.rtv_srgb;
        }

        if (view_non_srgb == 0)
        {
            continue;
        }

        if (group->getFlipBuffer() && runtimeData.specialEffects[REST_FLIP].technique != 0)
        {
            runtime->render_technique(runtimeData.specialEffects[REST_FLIP].technique, cmd_list, view_non_srgb, view_srgb);
        }

        if (group->getToneMap() && runtimeData.specialEffects[REST_TONEMAP_TO_SDR].technique != 0)
        {
            runtime->render_technique(runtimeData.specialEffects[REST_TONEMAP_TO_SDR].technique, cmd_list, view_non_srgb, view_srgb);
        }

        for (const auto& effectTech : effectList)
        {
            const auto& [name, effectData] = effectTech;

            deviceData.rendered_effects = true;

            runtime->render_technique(effectData->technique, cmd_list, view_non_srgb, view_srgb);
            effectData->rendered = true;

            removalList.push_back(name);

            rendered = true;
        }

        if (group->getToneMap() && runtimeData.specialEffects[REST_TONEMAP_TO_HDR].technique != 0)
        {
            runtime->render_technique(runtimeData.specialEffects[REST_TONEMAP_TO_HDR].technique, cmd_list, view_non_srgb, view_srgb);
        }

        if (group->getFlipBuffer() && runtimeData.specialEffects[REST_FLIP].technique != 0)
        {
            runtime->render_technique(runtimeData.specialEffects[REST_FLIP].technique, cmd_list, view_non_srgb, view_srgb);
        }

        if (copyPreserveAlpha)
        {
            resource_view target_view_non_srgb = view.rtv;
            resource_view target_view_srgb = view.rtv_srgb;

            if (target_view_non_srgb != 0)
                shaderManager.CopyResourceMaskAlpha(cmd_list, group_view, target_view_non_srgb, desc.texture.width, desc.texture.height);
        }
    }

    return rendered;
}

void RenderingEffectManager::RenderEffects(command_list* cmd_list, uint64_t callLocation, uint64_t invocation)
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

    if (deviceData.current_runtime == nullptr || (commandListData.ps.techniquesToRender.size() == 0 && commandListData.vs.techniquesToRender.size() == 0 && commandListData.cs.techniquesToRender.size() == 0)) {
        return;
    }

    RuntimeDataContainer& runtimeData = deviceData.current_runtime->get_private_data<RuntimeDataContainer>();
    bool toRender = false;
    unordered_set<string> psToRenderNames;
    unordered_set<string> vsToRenderNames;
    unordered_set<string> csToRenderNames;

    if (invocation & MATCH_EFFECT_PS)
    {
        RenderingManager::QueueOrDequeue(cmd_list, deviceData, commandListData, commandListData.ps.techniquesToRender, psToRenderNames, callLocation, 0, MATCH_EFFECT_PS);
    }

    if (invocation & MATCH_EFFECT_VS)
    {
        RenderingManager::QueueOrDequeue(cmd_list, deviceData, commandListData, commandListData.vs.techniquesToRender, vsToRenderNames, callLocation, 1, MATCH_EFFECT_VS);
    }

    if (invocation & MATCH_EFFECT_CS)
    {
        RenderingManager::QueueOrDequeue(cmd_list, deviceData, commandListData, commandListData.cs.techniquesToRender, csToRenderNames, callLocation, 2, MATCH_EFFECT_CS);
    }

    bool rendered = false;
    vector<string> psRemovalList;
    vector<string> vsRemovalList;
    vector<string> csRemovalList;

    if (psToRenderNames.size() == 0 && vsToRenderNames.size() == 0)
    {
        return;
    }

    if (!deviceData.rendered_effects)
    {
        deviceData.current_runtime->render_effects(cmd_list, resource_view{ 0 }, resource_view{ 0 });
    }

    unique_lock<shared_mutex> dev_mutex(runtimeData.render_mutex);
    rendered =
        (psToRenderNames.size() > 0) && _RenderEffects(cmd_list, deviceData, runtimeData, commandListData.ps.techniquesToRender, psRemovalList, psToRenderNames) ||
        (vsToRenderNames.size() > 0) && _RenderEffects(cmd_list, deviceData, runtimeData, commandListData.vs.techniquesToRender, vsRemovalList, vsToRenderNames) ||
        (csToRenderNames.size() > 0) && _RenderEffects(cmd_list, deviceData, runtimeData, commandListData.cs.techniquesToRender, csRemovalList, csToRenderNames);
    dev_mutex.unlock();

    for (auto& g : psRemovalList)
    {
        commandListData.ps.techniquesToRender.erase(g);
    }

    for (auto& g : vsRemovalList)
    {
        commandListData.vs.techniquesToRender.erase(g);
    }

    for (auto& g : csRemovalList)
    {
        commandListData.cs.techniquesToRender.erase(g);
    }

    if (rendered)
    {
        cmd_list->get_private_data<state_tracking>().apply(cmd_list);
    }
}


void RenderingEffectManager::PreventRuntimeReload(reshade::api::effect_runtime* runtime, reshade::api::command_list* cmd_list)
{
    if (runtime == nullptr)
        return;

    RuntimeDataContainer& runtimeData = runtime->get_private_data<RuntimeDataContainer>();

    // cringe
    if (runtimeData.specialEffects[REST_NOOP].technique != 0)
    {
        resource res = runtime->get_current_back_buffer();
        GlobalResourceView& view = resourceManager.GetResourceView(runtime->get_device(), res.handle);
        resource_view active_rtv = view.rtv;
        resource_view active_rtv_srgb = view.rtv_srgb;

        if (resourceManager.dummy_rtv != 0)
            runtime->render_technique(runtimeData.specialEffects[REST_NOOP].technique, cmd_list, resourceManager.dummy_rtv, resourceManager.dummy_rtv);

        runtime->render_technique(runtimeData.specialEffects[REST_NOOP].technique, cmd_list, active_rtv, active_rtv_srgb);
    }
}