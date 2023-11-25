#include "RenderingQueueManager.h"

using namespace Rendering;
using namespace ShaderToggler;
using namespace reshade::api;
using namespace std;

RenderingQueueManager::RenderingQueueManager(AddonImGui::AddonUIData& data, ResourceManager& rManager) : uiData(data), resourceManager(rManager)
{
}

RenderingQueueManager::~RenderingQueueManager()
{

}

void RenderingQueueManager::_CheckCallForCommandList(ShaderData& sData, CommandListDataContainer& commandListData, DeviceDataContainer& deviceData) const
{
    // Masks which checks to perform. Note that we will always schedule a draw call check for binding and effect updates,
    // this serves the purpose of assigning the resource_view to perform the update later on if needed.
    uint64_t queue_mask = MATCH_NONE;

    // Shift in case of VS using data id
    const uint64_t match_effect = MATCH_EFFECT_PS << sData.id;
    const uint64_t match_binding = MATCH_BINDING_PS << sData.id;
    const uint64_t match_const = MATCH_CONST_PS << sData.id;
    const uint64_t match_preview = MATCH_PREVIEW_PS << sData.id;

    if (sData.blockedShaderGroups != nullptr)
    {
        for (auto group : *sData.blockedShaderGroups)
        {
            if (group->isActive())
            {
                if (group->getExtractConstants() && !deviceData.constantsUpdated.contains(group))
                {
                    if (!sData.constantBuffersToUpdate.contains(group))
                    {
                        sData.constantBuffersToUpdate.emplace(group);
                        queue_mask |= match_const;
                    }
                }

                if (group->getId() == uiData.GetToggleGroupIdShaderEditing() && !deviceData.huntPreview.matched)
                {
                    if (uiData.GetCurrentTabType() == AddonImGui::TAB_RENDER_TARGET)
                    {
                        queue_mask |= (match_preview << (group->getInvocationLocation() * MATCH_DELIMITER)) | (match_preview << (CALL_DRAW * MATCH_DELIMITER));
                        deviceData.huntPreview.target_invocation_location = group->getInvocationLocation();
                    }
                }

                if (group->isProvidingTextureBinding() && !deviceData.bindingsUpdated.contains(group))
                {
                    if (!sData.bindingsToUpdate.contains(group))
                    {
                        if (!group->getCopyTextureBinding() || group->getExtractResourceViews())
                        {
                            sData.bindingsToUpdate.emplace(group, std::make_tuple(CALL_DRAW, resource{ 0 }));
                            queue_mask |= (match_binding << CALL_DRAW * MATCH_DELIMITER);
                        }
                        else
                        {
                            sData.bindingsToUpdate.emplace(group, std::make_tuple(group->getBindingInvocationLocation(), resource{ 0 }));
                            queue_mask |= (match_binding << (group->getBindingInvocationLocation() * MATCH_DELIMITER)) | (match_binding << (CALL_DRAW * MATCH_DELIMITER));
                        }
                    }
                }

                if (group->getAllowAllTechniques())
                {
                    for (const auto& [techName, techData] : deviceData.allEnabledTechniques)
                    {
                        if (group->getHasTechniqueExceptions() && group->preferredTechniques().contains(techName))
                        {
                            continue;
                        }

                        if (!techData->rendered)
                        {
                            if (!sData.techniquesToRender.contains(techName))
                            {
                                sData.techniquesToRender.emplace(techName, std::make_tuple(group, group->getInvocationLocation(), resource{ 0 }));
                                queue_mask |= (match_effect << (group->getInvocationLocation() * MATCH_DELIMITER)) | (match_effect << (CALL_DRAW * MATCH_DELIMITER));
                            }
                        }
                    }
                }
                else if (group->preferredTechniques().size() > 0) {
                    for (auto& techName : group->preferredTechniques())
                    {
                        const auto& techData = deviceData.allEnabledTechniques.find(techName);
                        if (techData != deviceData.allEnabledTechniques.end() && !techData->second->rendered)
                        {
                            if (!sData.techniquesToRender.contains(techName))
                            {
                                sData.techniquesToRender.emplace(techName, std::make_tuple(group, group->getInvocationLocation(), resource{ 0 }));
                                queue_mask |= (match_effect << (group->getInvocationLocation() * MATCH_DELIMITER)) | (match_effect << (CALL_DRAW * MATCH_DELIMITER));
                            }
                        }
                    }
                }
            }
        }
    }

    commandListData.commandQueue |= queue_mask;
}

void RenderingQueueManager::CheckCallForCommandList(reshade::api::command_list* commandList)
{
    if (nullptr == commandList)
    {
        return;
    }

    CommandListDataContainer& commandListData = commandList->get_private_data<CommandListDataContainer>();
    DeviceDataContainer& deviceData = commandList->get_device()->get_private_data<DeviceDataContainer>();

    shared_lock<shared_mutex> r_mutex(deviceData.render_mutex);
    shared_lock<shared_mutex> b_mutex(deviceData.binding_mutex);

    _CheckCallForCommandList(commandListData.ps, commandListData, deviceData);
    _CheckCallForCommandList(commandListData.vs, commandListData, deviceData);
    _CheckCallForCommandList(commandListData.cs, commandListData, deviceData);

    b_mutex.unlock();
    r_mutex.unlock();
}

void RenderingQueueManager::_RescheduleGroups(ShaderData& sData, CommandListDataContainer& commandListData, DeviceDataContainer& deviceData)
{
    const uint32_t match_effect = MATCH_EFFECT_PS << sData.id;
    const uint32_t match_binding = MATCH_BINDING_PS << sData.id;
    const uint32_t match_const = MATCH_CONST_PS << sData.id;
    const uint32_t match_preview = MATCH_PREVIEW_PS << sData.id;

    uint32_t queue_mask = MATCH_NONE;

    for (const auto& tech : sData.techniquesToRender)
    {
        const ToggleGroup* group = std::get<0>(tech.second);
        const resource res = std::get<2>(tech.second);

        if (res == 0 && group->getRequeueAfterRTMatchingFailure())
        {
            queue_mask |= (match_effect << (group->getInvocationLocation() * MATCH_DELIMITER)) | (match_effect << (CALL_DRAW * MATCH_DELIMITER));

            if (group->getId() == uiData.GetToggleGroupIdShaderEditing() && !deviceData.huntPreview.matched && deviceData.huntPreview.target == 0)
            {
                if (uiData.GetCurrentTabType() == AddonImGui::TAB_RENDER_TARGET)
                {
                    queue_mask |= (match_preview << (group->getInvocationLocation() * MATCH_DELIMITER)) | (match_preview << (CALL_DRAW * MATCH_DELIMITER));

                    deviceData.huntPreview.target_invocation_location = group->getInvocationLocation();
                }
            }
        }
    }

    for (const auto& tech : sData.bindingsToUpdate)
    {
        const ToggleGroup* group = tech.first;
        const resource res = std::get<1>(tech.second);

        if (res == 0 && group->getRequeueAfterRTMatchingFailure())
        {
            queue_mask |= (match_binding << (group->getInvocationLocation() * MATCH_DELIMITER)) | (match_binding << (CALL_DRAW * MATCH_DELIMITER));

            if (group->getId() == uiData.GetToggleGroupIdShaderEditing() && !deviceData.huntPreview.matched && deviceData.huntPreview.target == 0)
            {
                if (uiData.GetCurrentTabType() == AddonImGui::TAB_RENDER_TARGET)
                {
                    queue_mask |= (match_preview << (group->getInvocationLocation() * MATCH_DELIMITER)) | (match_preview << (CALL_DRAW * MATCH_DELIMITER));

                    deviceData.huntPreview.target_invocation_location = group->getInvocationLocation();
                }
            }
        }
    }

    commandListData.commandQueue |= queue_mask;
}

void RenderingQueueManager::RescheduleGroups(CommandListDataContainer& commandListData, DeviceDataContainer& deviceData)
{
    std::shared_lock<std::shared_mutex> r_mutex(deviceData.render_mutex);
    std::shared_lock<std::shared_mutex> b_mutex(deviceData.binding_mutex);

    if (commandListData.ps.techniquesToRender.size() > 0 || commandListData.ps.bindingsToUpdate.size() > 0)
    {
        _RescheduleGroups(commandListData.ps, commandListData, deviceData);
    }

    if (commandListData.vs.techniquesToRender.size() > 0 || commandListData.vs.bindingsToUpdate.size() > 0)
    {
        _RescheduleGroups(commandListData.vs, commandListData, deviceData);
    }

    if (commandListData.cs.techniquesToRender.size() > 0 || commandListData.cs.bindingsToUpdate.size() > 0)
    {
        _RescheduleGroups(commandListData.cs, commandListData, deviceData);
    }

    b_mutex.unlock();
    r_mutex.unlock();
}


static void clearStage(CommandListDataContainer& commandListData, effect_queue& queuedTasks, uint64_t pipelineChange, uint64_t clearFlag, uint64_t location)
{
    if (queuedTasks.size() > 0 && (pipelineChange & clearFlag))
    {
        for (auto it = queuedTasks.begin(); it != queuedTasks.end();)
        {
            uint64_t callLocation = std::get<1>(it->second);
            if (callLocation == location)
            {
                it = queuedTasks.erase(it);
                continue;
            }
            it++;
        }
    }
}

static void clearStage(CommandListDataContainer& commandListData, binding_queue& queuedTasks, uint64_t pipelineChange, uint64_t clearFlag, uint64_t location)
{
    if (queuedTasks.size() > 0 && (pipelineChange & clearFlag))
    {
        for (auto it = queuedTasks.begin(); it != queuedTasks.end();)
        {
            uint64_t callLocation = std::get<0>(it->second);
            if (callLocation == location)
            {
                it = queuedTasks.erase(it);
                continue;
            }
            it++;
        }
    }
}

void RenderingQueueManager::ClearQueue(CommandListDataContainer& commandListData, const uint64_t pipelineChange, const uint64_t location) const
{
    const uint64_t qloc = pipelineChange << (location * Rendering::MATCH_DELIMITER);

    if (commandListData.commandQueue & qloc)
    {
        commandListData.commandQueue &= ~qloc;

        clearStage(commandListData, commandListData.ps.techniquesToRender, pipelineChange, MATCH_EFFECT_PS, location);
        clearStage(commandListData, commandListData.vs.techniquesToRender, pipelineChange, MATCH_EFFECT_VS, location);
        clearStage(commandListData, commandListData.cs.techniquesToRender, pipelineChange, MATCH_EFFECT_CS, location);

        clearStage(commandListData, commandListData.ps.bindingsToUpdate, pipelineChange, MATCH_BINDING_PS, location);
        clearStage(commandListData, commandListData.vs.bindingsToUpdate, pipelineChange, MATCH_BINDING_VS, location);
        clearStage(commandListData, commandListData.cs.bindingsToUpdate, pipelineChange, MATCH_BINDING_CS, location);
    }
}

void RenderingQueueManager::ClearQueue(CommandListDataContainer& commandListData, const uint64_t pipelineChange) const
{
    // Make sure we dequeue whatever is left over scheduled for CALL_DRAW/CALL_BIND_PIPELINE in case re-queueing was enabled for some group
    if (commandListData.commandQueue & (Rendering::CHECK_MATCH_BIND_RENDERTARGET_EFFECT | Rendering::CHECK_MATCH_BIND_RENDERTARGET_BINDING))
    {
        uint64_t drawflagmask = (commandListData.commandQueue & (Rendering::CHECK_MATCH_BIND_RENDERTARGET_EFFECT | Rendering::CHECK_MATCH_BIND_RENDERTARGET_BINDING)) >> (Rendering::CALL_BIND_RENDER_TARGET * Rendering::MATCH_DELIMITER);
        drawflagmask &= commandListData.commandQueue;
        drawflagmask &= pipelineChange;

        // Clear RT commands if their draw flags have not been cleared before a pipeline change
        ClearQueue(commandListData, drawflagmask, Rendering::CALL_BIND_RENDER_TARGET);
    }

    ClearQueue(commandListData, pipelineChange, Rendering::CALL_DRAW);
    ClearQueue(commandListData, pipelineChange, Rendering::CALL_BIND_PIPELINE);
}