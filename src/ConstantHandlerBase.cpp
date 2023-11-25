#include <cstring>
#include "ConstantHandlerBase.h"
#include "PipelinePrivateData.h"
#include "StateTracking.h"

using namespace Shim::Constants;
using namespace reshade::api;
using namespace ShaderToggler;
using namespace std;

unordered_map<string, tuple<constant_type, vector<effect_uniform_variable>>> ConstantHandlerBase::restVariables;
char ConstantHandlerBase::charBuffer[CHAR_BUFFER_SIZE];
ConstantCopyBase* ConstantHandlerBase::_constCopy;
std::shared_mutex ConstantHandlerBase::groupBufferMutex;

ConstantHandlerBase::ConstantHandlerBase()
{
}

ConstantHandlerBase::~ConstantHandlerBase()
{
}


void ConstantHandlerBase::SetConstantCopy(ConstantCopyBase* constantCopy)
{
    _constCopy = constantCopy;
}

size_t ConstantHandlerBase::GetConstantBufferSize(const ToggleGroup* group)
{
    if (groupBufferSize.contains(group))
    {
        return groupBufferSize.at(group);
    }

    return 0;
}

const uint8_t* ConstantHandlerBase::GetConstantBuffer(const ToggleGroup* group)
{
    if (groupBufferContent.contains(group))
    {
        return groupBufferContent.at(group).data();
    }

    return nullptr;
}

unordered_map<string, tuple<constant_type, vector<effect_uniform_variable>>>* ConstantHandlerBase::GetRESTVariables()
{
    return &restVariables;
}

void ConstantHandlerBase::ReloadConstantVariables(effect_runtime* runtime)
{
    restVariables.clear();

    runtime->enumerate_uniform_variables(nullptr, [](effect_runtime* rt, effect_uniform_variable variable) {
        if (!rt->get_annotation_string_from_uniform_variable<CHAR_BUFFER_SIZE>(variable, "source", charBuffer))
        {
            return;
        }

        reshade::api::format format;
        uint32_t rows;
        uint32_t columns;
        uint32_t array_length;

        rt->get_uniform_variable_type(variable, &format, &rows, &columns, &array_length);
        constant_type type = constant_type::type_unknown;
        switch (format)
        {
        case reshade::api::format::r32_float:
            if (array_length > 0)
                type = constant_type::type_unknown;
            else
            {
                if (rows == 4 && columns == 4)
                    type = constant_type::type_float4x4;
                else if (rows == 3 && columns == 4)
                    type = constant_type::type_float4x3;
                else if (rows == 3 && columns == 3)
                    type = constant_type::type_float3x3;
                else if (rows == 3 && columns == 1)
                    type = constant_type::type_float3;
                else if (rows == 2 && columns == 1)
                    type = constant_type::type_float2;
                else if (rows == 1 && columns == 1)
                    type = constant_type::type_float;
                else
                    type = constant_type::type_unknown;
            }
            break;
        case reshade::api::format::r32_sint:
            if (array_length > 0 || rows > 1 || columns > 1)
                type = constant_type::type_unknown;
            else
                type = constant_type::type_int;
            break;
        case reshade::api::format::r32_uint:
            if (array_length > 0 || rows > 1 || columns > 1)
                type = constant_type::type_unknown;
            else
                type = constant_type::type_uint;
            break;
        }

        if (type == constant_type::type_unknown)
        {
            return;
        }

        string id(charBuffer);
        const auto& vars = restVariables.find(id);

        if (vars == restVariables.end())
        {
            restVariables.emplace(id, make_tuple(type, vector<effect_uniform_variable>{variable}));
        }
        else {
            auto& [varType, varVec] = vars->second;
            if (varType == type)
            {
                varVec.push_back(variable);
            }
        }
        });
}

void ConstantHandlerBase::ClearConstantVariables()
{
    restVariables.clear();
}

void ConstantHandlerBase::OnReshadeSetTechniqueState(effect_runtime* runtime, int32_t enabledCount)
{
    previousEnableCount = enabledCount;
}

void ConstantHandlerBase::OnReshadeReloadedEffects(effect_runtime* runtime, int32_t enabledCount)
{
    unique_lock<shared_mutex> lock(varMutex);

    if (enabledCount == 0 || enabledCount - previousEnableCount < 0)
    {
        ClearConstantVariables();
    }
    else
    {
        ReloadConstantVariables(runtime);
    }

    previousEnableCount = enabledCount;
}

bool ConstantHandlerBase::UpdateConstantBufferEntries(command_list* cmd_list, CommandListDataContainer& cmdData, DeviceDataContainer& devData, ToggleGroup* group, uint32_t index)
{
    state_tracking& state = cmd_list->get_private_data<state_tracking>();
    index = std::min(static_cast<uint32_t>(2), index);

    int32_t slot_size = static_cast<int32_t>(state.get_root_table_size_at(index));
    int32_t slot = std::min(static_cast<int32_t>(group->getCBSlotIndex()), slot_size - 1);

    if (slot_size <= 0)
        return false;

    int32_t desc_size = static_cast<int32_t>(state.get_root_table_entry_size_at(index, slot));
    int32_t desc = std::min(static_cast<int32_t>(group->getCBDescriptorIndex()), desc_size - 1);

    if (desc_size <= 0)
        return false;

    const descriptor_tracking::descriptor_data* buf = state.get_descriptor_at(index, slot, desc);

    DescriptorCycle cycle = group->consumeCBCycle();
    if (cycle != CYCLE_NONE)
    {
        if (cycle == CYCLE_UP)
        {
            desc = std::min(++desc, desc_size - 1);
            buf = state.get_descriptor_at(index, slot, desc);

            while ((buf == nullptr || buf->constant.buffer == 0) && desc < desc_size - 2)
            {
                buf = state.get_descriptor_at(index, slot, ++desc);
            }
        }
        else
        {
            desc = desc > 0 ? --desc : 0;
            buf = state.get_descriptor_at(index, slot, desc);

            while ((buf == nullptr || buf->constant.buffer == 0) && desc > 0)
            {
                buf = state.get_descriptor_at(index, slot, --desc);
            }
        }

        if (buf != nullptr && buf->constant.buffer != 0)
        {
            group->setCBDescriptorIndex(desc);
        }
    }

    if (buf != nullptr && buf->constant.buffer != 0)
    {
        unique_lock<shared_mutex>(groupBufferMutex);

        SetBufferRange(group, buf->constant, cmd_list->get_device(), cmd_list);
        ApplyConstantValues(devData.current_runtime, group, restVariables);
        devData.constantsUpdated.insert(group);

        return true;
    }

    return false;
}

bool ConstantHandlerBase::UpdateConstantEntries(command_list* cmd_list, CommandListDataContainer& cmdData, DeviceDataContainer& devData, ToggleGroup* group, uint32_t index)
{
    state_tracking& state = cmd_list->get_private_data<state_tracking>();
    index = std::min(static_cast<uint32_t>(2), index);

    //const auto& [_, current_constants] = state.push_constants[index];

    int32_t slot_size = static_cast<int32_t>(state.get_root_table_size_at(index));
    int32_t slot = std::min(static_cast<int32_t>(group->getCBSlotIndex()), slot_size - 1);
    
    if (slot <= 0)
        return false;
    
    int32_t const_buffer_size = static_cast<int32_t>(state.get_root_table_entry_size_at(index, slot));
    
    if (const_buffer_size == 0)
        return false;
    
    const vector<uint32_t>* buf = state.get_constants_at(index, slot);//current_constants.at(slot);

    if (buf != nullptr)
    {
        unique_lock<shared_mutex>(groupBufferMutex);

        SetConstants(group, *buf, cmd_list->get_device(), cmd_list);
        ApplyConstantValues(devData.current_runtime, group, restVariables);
        devData.constantsUpdated.insert(group);
    }

    return true;
}

void ConstantHandlerBase::UpdateConstants(command_list* cmd_list)
{
    if (cmd_list == nullptr || cmd_list->get_device() == nullptr)
    {
        return;
    }

    device* device = cmd_list->get_device();
    CommandListDataContainer& commandListData = cmd_list->get_private_data<CommandListDataContainer>();
    DeviceDataContainer& deviceData = device->get_private_data<DeviceDataContainer>();

    if (deviceData.current_runtime == nullptr ||
        (commandListData.ps.constantBuffersToUpdate.size() == 0 && commandListData.vs.constantBuffersToUpdate.size() == 0 && commandListData.cs.constantBuffersToUpdate.size() == 0)) {
        return;
    }

    vector<ToggleGroup*> psRemovalList;
    vector<ToggleGroup*> vsRemovalList;
    vector<ToggleGroup*> csRemovalList;

    for (const auto& cb : commandListData.ps.constantBuffersToUpdate)
    {
        if (!deviceData.constantsUpdated.contains(cb))
        {
            if (!cb->getCBIsPushMode() && UpdateConstantBufferEntries(cmd_list, commandListData, deviceData, cb, cb->getCBShaderStage()) ||
                cb->getCBIsPushMode() && UpdateConstantEntries(cmd_list, commandListData, deviceData, cb, cb->getCBShaderStage()))
            {
                psRemovalList.push_back(cb);
            }
        }
    }

    for (const auto& cb : commandListData.vs.constantBuffersToUpdate)
    {
        if (!deviceData.constantsUpdated.contains(cb))
        {
            if (!cb->getCBIsPushMode() && UpdateConstantBufferEntries(cmd_list, commandListData, deviceData, cb, cb->getCBShaderStage()) ||
                cb->getCBIsPushMode() && UpdateConstantEntries(cmd_list, commandListData, deviceData, cb, cb->getCBShaderStage()))
            {
                vsRemovalList.push_back(cb);
            }
        }
    }

    for (const auto& cb : commandListData.cs.constantBuffersToUpdate)
    {
        if (!deviceData.constantsUpdated.contains(cb))
        {
            if (!cb->getCBIsPushMode() && UpdateConstantBufferEntries(cmd_list, commandListData, deviceData, cb, cb->getCBShaderStage()) ||
                cb->getCBIsPushMode() && UpdateConstantEntries(cmd_list, commandListData, deviceData, cb, cb->getCBShaderStage()))
            {
                csRemovalList.push_back(cb);
            }
        }
    }

    for (const auto& g : psRemovalList)
    {
        commandListData.ps.constantBuffersToUpdate.erase(g);
    }

    for (const auto& g : vsRemovalList)
    {
        commandListData.vs.constantBuffersToUpdate.erase(g);
    }

    for (const auto& g : csRemovalList)
    {
        commandListData.cs.constantBuffersToUpdate.erase(g);
    }
}

void ConstantHandlerBase::ApplyConstantValues(effect_runtime* runtime, const ToggleGroup* group,
    const unordered_map<string, tuple<constant_type, vector<effect_uniform_variable>>>& constants)
{
    unique_lock<shared_mutex> lock(varMutex);

    if (!groupBufferContent.contains(group) || runtime == nullptr)
    {
        return;
    }

    const uint8_t* buffer = groupBufferContent.at(group).data();
    const uint8_t* prevBuffer = groupPrevBufferContent.at(group).data();

    for (const auto& [varName,varData] : group->GetVarOffsetMapping())
    {
        const auto& [offset, prevValue] = varData;

        const uint8_t* bufferInUse = prevValue ? prevBuffer : buffer;

        if (!constants.contains(varName))
        {
            continue;
        }

        const auto& [type, effect_variables] = constants.at(varName);
        uint32_t typeIndex = static_cast<uint32_t>(type);
        size_t bufferSize = groupBufferSize.at(group);

        if (offset + type_size[typeIndex] * type_length[typeIndex] >= bufferSize)
        {
            continue;
        }


        for (const auto& effect_var : effect_variables)
        {
            if (type <= constant_type::type_float4x4)
            {
                runtime->set_uniform_value_float(effect_var, reinterpret_cast<const float*>(bufferInUse + offset), type_length[typeIndex], 0);
            }
            else if (type == constant_type::type_int)
            {
                runtime->set_uniform_value_int(effect_var, reinterpret_cast<const int32_t*>(bufferInUse + offset), type_length[typeIndex], 0);
            }
            else
            {
                runtime->set_uniform_value_uint(effect_var, reinterpret_cast<const uint32_t*>(bufferInUse + offset), type_length[typeIndex], 0);
            }
        }
    }
}


void ConstantHandlerBase::SetConstants(const ToggleGroup* group, const vector<uint32_t>& buf, device* dev, command_list* cmd_list)
{
    if (dev == nullptr || cmd_list == nullptr || buf.size() == 0)
    {
        return;
    }

    InitBuffers(group, buf.size());

    vector<uint8_t>& bufferContent = groupBufferContent.at(group);
    vector<uint8_t>& prevBufferContent = groupPrevBufferContent.at(group);

    std::memcpy(prevBufferContent.data(), bufferContent.data(), buf.size());
    std::memcpy(bufferContent.data(), reinterpret_cast<const uint8_t*>(buf.data()), buf.size() * 4);
}

void ConstantHandlerBase::SetBufferRange(ToggleGroup* group, buffer_range range, device* dev, command_list* cmd_list)
{
    if (dev == nullptr || cmd_list == nullptr || range.buffer == 0)
    {
        return;
    }

    resource_desc targetBufferDesc = dev->get_resource_desc(range.buffer);
    size_t size = static_cast<size_t>(targetBufferDesc.buffer.size);

    InitBuffers(group, size);

    vector<uint8_t>& bufferContent = groupBufferContent.at(group);
    vector<uint8_t>& prevBufferContent = groupPrevBufferContent.at(group);

    std::memcpy(prevBufferContent.data(), bufferContent.data(), size);
    _constCopy->GetHostConstantBuffer(cmd_list, group, bufferContent, size, range.buffer.handle);
}

void ConstantHandlerBase::InitBuffers(const ToggleGroup* group, size_t size)
{
    const auto& content = groupBufferContent.find(group);

    if (content != groupBufferContent.end() && size != content->second.size())
    {
        groupBufferContent[group].resize(size, 0);
        groupPrevBufferContent[group].resize(size, 0);
        groupBufferSize[group] = size;
    }
    else if(content == groupBufferContent.end())
    {
        groupBufferContent.emplace(group, vector<uint8_t>(size, 0));
        groupPrevBufferContent.emplace(group, vector<uint8_t>(size, 0));
        groupBufferSize.emplace(group, size);
    }
}

void ConstantHandlerBase::RemoveGroup(const ToggleGroup* group, device* dev)
{
    if (!groupBufferContent.contains(group))
    {
        return;
    }

    groupBufferContent.erase(group);
    groupPrevBufferContent.erase(group);
    groupBufferSize.erase(group);
}