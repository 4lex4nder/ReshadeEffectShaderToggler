///////////////////////////////////////////////////////////////////////
//
// Part of ShaderToggler, a shader toggler add on for Reshade 5+ which allows you
// to define groups of shaders to toggle them on/off with one key press
// 
// (c) Frans 'Otis_Inf' Bouma.
//
// All rights reserved.
// https://github.com/FransBouma/ShaderToggler
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met :
//
//  * Redistributions of source code must retain the above copyright notice, this
//	  list of conditions and the following disclaimer.
//
//  * Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and / or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
/////////////////////////////////////////////////////////////////////////
#define IMGUI_DISABLE_INCLUDE_IMCONFIG_H
#define ImTextureID unsigned long long // Change ImGui texture ID type to that of a 'reshade::api::resource_view' handle

#include <imgui.h>
#include <reshade.hpp>
#include <vector>
#include <unordered_map>
#include <set>
#include <functional>
#include <tuple>
#include <chrono>
#include <filesystem>
#include <MinHook.h>
#include "crc32_hash.hpp"
#include "ShaderManager.h"
#include "CDataFile.h"
#include "ToggleGroup.h"
#include "AddonUIData.h"
#include "AddonUIDisplay.h"
#include "ConstantHandler.h"
#include "ConstantHandlerMemcpy.h"
#include "ConstantCopyMethod.h"
#include "ConstantCopyMethodSingularMapping.h"
#include "ConstantCopyMethodNestedMapping.h"
#include "PipelineStateTracker.h"

using namespace reshade::api;
using namespace ShaderToggler;
using namespace AddonImGui;
using namespace ConstantFeedback;
using namespace StateTracker;

extern "C" __declspec(dllexport) const char* NAME = "Reshade Effect Shader Toggler";
extern "C" __declspec(dllexport) const char* DESCRIPTION = "Addon which allows you to define groups of shaders to render Reshade effects on.";

struct __declspec(uuid("222F7169-3C09-40DB-9BC9-EC53842CE537")) CommandListDataContainer {
    uint64_t activePixelShaderPipeline;
    uint64_t activeVertexShaderPipeline;
    unordered_map<string, int32_t> techniquesToRender;
    unordered_map<string, int32_t> bindingsToUpdate;
    vector<vector<resource_view>> active_rtv_history = vector<vector<resource_view>>(MAX_RT_HISTORY);
    unordered_set < pair<string, int32_t>,
        decltype([](const pair<string, int32_t>& v) {
        return std::hash<std::string>{}(v.first); // Don't care about history index, we'll overwrite them in case of collisions
            }),
        decltype([](const pair<string, int32_t>& lhs, const pair<string, int32_t>& rhs) {
                return lhs.first == rhs.first;
            }) > immediateActionSet;
            PipelineStateTracker stateTracker;
};

struct __declspec(uuid("C63E95B1-4E2F-46D6-A276-E8B4612C069A")) DeviceDataContainer {
    effect_runtime* current_runtime = nullptr;
    atomic_bool rendered_effects = false;
    unordered_map<string, bool> allEnabledTechniques;
    unordered_map<string, constant_type> rest_variables;
    unordered_map<string, tuple<resource, reshade::api::format, resource_view, resource_view>> bindingMap;
    unordered_set<string> bindingsUpdated;
    unordered_set<const ToggleGroup*> constantsUpdated;
    unordered_map<uint64_t, vector<bool>> transient_mask;
};

constexpr auto CHAR_BUFFER_SIZE = 256;
constexpr auto MAX_EFFECT_HANDLES = 128;
constexpr auto REST_VAR_ANNOTATION = "source";

static filesystem::path g_dll_path;
static filesystem::path g_base_path;


static ShaderToggler::ShaderManager g_pixelShaderManager;
static ShaderToggler::ShaderManager g_vertexShaderManager;

static ConstantHandler constantHandlerFallback;
static ConstantHandlerMemcpy constantHandlerMemcpy;
static ConstantCopyMethodSingularMapping constantUnnestedMap(&constantHandlerMemcpy);
static ConstantCopyMethodNestedMapping constantNestedMap(&constantHandlerMemcpy);
static ConstantHandlerBase* constantHandler = nullptr;
static ConstantCopyMethod* constantCopyMethod = nullptr;
static bool constantHandlerHooked = false;

static atomic_uint32_t g_activeCollectorFrameCounter = 0;
static vector<string> allTechniques;
static unordered_map<string, tuple<constant_type, vector<effect_uniform_variable>>> g_restVariables;
static AddonUIData g_addonUIData(&g_pixelShaderManager, &g_vertexShaderManager, constantHandler, &g_activeCollectorFrameCounter, &allTechniques, &g_restVariables);
static std::shared_mutex resource_mutex;
static std::shared_mutex resource_view_mutex;
static std::shared_mutex constbuffer_mutex;
static std::shared_mutex pipeline_layout_mutex;
static std::shared_mutex render_mutex;
static std::shared_mutex binding_mutex;
static char g_charBuffer[CHAR_BUFFER_SIZE];
static size_t g_charBufferSize = CHAR_BUFFER_SIZE;
static const float clearColor[] = { 0, 0, 0, 0 };

static unordered_set<uint64_t> s_constantBuffers;
static unordered_set<uint64_t> s_resources;
static unordered_set<uint64_t> s_resourceViews;

static unordered_map<uint64_t, pair<resource_view, resource_view>> s_backBufferView;
static unordered_map<uint64_t, pair<resource_view, resource_view>> s_sRGBResourceViews;
static unordered_map<const resource_desc*, reshade::api::format> s_resourceFormatTransient;
static unordered_map<uint64_t, reshade::api::format> s_resourceFormat;

/// <summary>
/// Calculates a crc32 hash from the passed in shader bytecode. The hash is used to identity the shader in future runs.
/// </summary>
/// <param name="shaderData"></param>
/// <returns></returns>
static uint32_t calculateShaderHash(void* shaderData)
{
    if (nullptr == shaderData)
    {
        return 0;
    }

    const auto shaderDesc = *static_cast<shader_desc*>(shaderData);
    return compute_crc32(static_cast<const uint8_t*>(shaderDesc.code), shaderDesc.code_size);
}


static void enumerateTechniques(effect_runtime* runtime, std::function<void(effect_runtime*, effect_technique, string&)> func)
{
    runtime->enumerate_techniques(nullptr, [func](effect_runtime* rt, effect_technique technique) {
        g_charBufferSize = CHAR_BUFFER_SIZE;
    rt->get_technique_name(technique, g_charBuffer, &g_charBufferSize);
    string name(g_charBuffer);
    func(rt, technique, name);
        });
}


static void enumerateRESTUniformVariables(effect_runtime* runtime, std::function<void(effect_runtime*, effect_uniform_variable, constant_type&, string&)> func)
{
    runtime->enumerate_uniform_variables(nullptr, [func](effect_runtime* rt, effect_uniform_variable variable) {
        g_charBufferSize = CHAR_BUFFER_SIZE;
    if (!rt->get_annotation_string_from_uniform_variable(variable, REST_VAR_ANNOTATION, g_charBuffer))
    {
        return;
    }

    string id(g_charBuffer);

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

    func(rt, variable, type, id);
        });
}


static bool isSRGB(reshade::api::format value)
{
    switch (value)
    {
    case format::r8g8b8a8_unorm_srgb:
    case format::r8g8b8x8_unorm_srgb:
    case format::b8g8r8a8_unorm_srgb:
    case format::b8g8r8x8_unorm_srgb:
    case format::bc1_unorm_srgb:
    case format::bc2_unorm_srgb:
    case format::bc3_unorm_srgb:
    case format::bc7_unorm_srgb:
        return true;
    default:
        return false;
    }

    return false;
}


static bool hasSRGB(reshade::api::format value)
{
    switch (value)
    {
    case format::r8g8b8a8_typeless:
    case format::r8g8b8a8_unorm:
    case format::r8g8b8a8_unorm_srgb:
    case format::r8g8b8x8_typeless:
    case format::r8g8b8x8_unorm:
    case format::r8g8b8x8_unorm_srgb:
    case format::b8g8r8a8_typeless:
    case format::b8g8r8a8_unorm:
    case format::b8g8r8a8_unorm_srgb:
    case format::b8g8r8x8_typeless:
    case format::b8g8r8x8_unorm:
    case format::b8g8r8x8_unorm_srgb:
    case format::bc1_typeless:
    case format::bc1_unorm:
    case format::bc1_unorm_srgb:
    case format::bc2_typeless:
    case format::bc2_unorm:
    case format::bc2_unorm_srgb:
    case format::bc3_typeless:
    case format::bc3_unorm:
    case format::bc3_unorm_srgb:
    case format::bc7_typeless:
    case format::bc7_unorm:
    case format::bc7_unorm_srgb:
        return true;
    default:
        return false;
    }

    return false;
}


static void reloadConstantVariables(effect_runtime* runtime)
{
    g_restVariables.clear();

    enumerateRESTUniformVariables(runtime, [](effect_runtime* runtime, effect_uniform_variable variable, constant_type& type, string& name) {
        if (!g_restVariables.contains(name))
        {
            g_restVariables.emplace(name, make_tuple(type, vector<effect_uniform_variable>()));
        }

    if (type == std::get<0>(g_restVariables.at(name)))
    {
        std::get<1>(g_restVariables.at(name)).push_back(variable);
    }
        });
}


static void initBackbuffer(effect_runtime* runtime)
{
    // Create backbuffer resource views
    device* dev = runtime->get_device();
    uint32_t count = runtime->get_back_buffer_count();

    for (uint32_t i = 0; i < count; ++i)
    {
        resource backBuffer = runtime->get_back_buffer(i);
        resource_desc desc = dev->get_resource_desc(backBuffer);

        resource_view backBufferView = { 0 };
        resource_view backBufferViewSRGB = { 0 };
        reshade::api::format viewFormat = desc.texture.format;
        reshade::api::format viewFormatSRGB = desc.texture.format;

        if (hasSRGB(desc.texture.format))
        {
            if (isSRGB(desc.texture.format))
            {
                viewFormat = format_to_default_typed(desc.texture.format);
            }
            else
            {
                viewFormatSRGB = format_to_default_typed(desc.texture.format, 1);
            }
        }

        dev->create_resource_view(backBuffer, resource_usage::render_target,
            resource_view_desc(viewFormat), &backBufferView);
        dev->create_resource_view(backBuffer, resource_usage::render_target,
            resource_view_desc(viewFormatSRGB), &backBufferViewSRGB);

        s_backBufferView[backBuffer.handle] = make_pair(backBufferView, backBufferViewSRGB);
    }
}


static bool RenderRemainingEffects(effect_runtime* runtime)
{
    if (runtime == nullptr || runtime->get_device() == nullptr)
    {
        return false;
    }

    command_list* cmd_list = runtime->get_command_queue()->get_immediate_command_list();
    device* device = runtime->get_device();
    CommandListDataContainer& commandListData = cmd_list->get_private_data<CommandListDataContainer>();
    DeviceDataContainer& deviceData = device->get_private_data<DeviceDataContainer>();
    resource_view active_rtv = { 0 };
    resource_view active_rtv_srgb = { 0 };

    if (s_backBufferView.contains(runtime->get_current_back_buffer().handle))
    {
        active_rtv = s_backBufferView.at(runtime->get_current_back_buffer().handle).first;
        active_rtv_srgb = s_backBufferView.at(runtime->get_current_back_buffer().handle).second;
    }

    if (deviceData.current_runtime == nullptr || active_rtv == 0 || !deviceData.rendered_effects) {
        return false;
    }

    bool rendered = false;
    enumerateTechniques(deviceData.current_runtime, [&deviceData, &commandListData, &cmd_list, &device, &active_rtv, &active_rtv_srgb, &rendered](effect_runtime* runtime, effect_technique technique, string& name) {
        if (deviceData.allEnabledTechniques.contains(name) && !deviceData.allEnabledTechniques[name])
        {
            resource res = runtime->get_device()->get_resource_from_view(active_rtv);
            resource_desc resDesc = runtime->get_device()->get_resource_desc(res);

            g_addonUIData.cFormat = resDesc.texture.format;

            runtime->render_technique(technique, cmd_list, active_rtv, active_rtv_srgb);

            deviceData.allEnabledTechniques[name] = true;
            rendered = true;
        }
        });

    return rendered;
}


static void onInitDevice(device* device)
{
    device->create_private_data<DeviceDataContainer>();
}


static void onDestroyDevice(device* device)
{
    device->destroy_private_data<DeviceDataContainer>();
}


static void onInitCommandList(command_list* commandList)
{
    commandList->create_private_data<CommandListDataContainer>();
}


static void onDestroyCommandList(command_list* commandList)
{
    commandList->destroy_private_data<CommandListDataContainer>();
}

static void onResetCommandList(command_list* commandList)
{
    CommandListDataContainer& commandListData = commandList->get_private_data<CommandListDataContainer>();

    commandListData.activePixelShaderPipeline = -1;
    commandListData.activeVertexShaderPipeline = -1;
    commandListData.active_rtv_history.clear();
    commandListData.bindingsToUpdate.clear();
    commandListData.techniquesToRender.clear();
    commandListData.stateTracker.Reset();
}


static bool onCreateResource(device* device, resource_desc& desc, subresource_data* initial_data, resource_usage initial_state)
{
    if (static_cast<uint32_t>(desc.usage & resource_usage::render_target) && desc.type == resource_type::texture_2d)
    {
        if (hasSRGB(desc.texture.format)) {
            std::unique_lock<shared_mutex> lock(resource_mutex);

            s_resourceFormatTransient.emplace(&desc, desc.texture.format);

            desc.texture.format = format_to_typeless(desc.texture.format);

            return true;
        }
    }

    return false;
}


static void onInitResource(device* device, const resource_desc& desc, const subresource_data* initData, resource_usage usage, reshade::api::resource handle)
{
    auto& data = device->get_private_data<DeviceDataContainer>();

    std::unique_lock<shared_mutex> lock(resource_mutex);
    s_resources.emplace(handle.handle);

    if (static_cast<uint32_t>(desc.usage & resource_usage::render_target) && desc.type == resource_type::texture_2d)
    {
        if (s_resourceFormatTransient.contains(&desc))
        {
            reshade::api::format orgFormat = s_resourceFormatTransient.at(&desc);
            s_resourceFormat.emplace(handle.handle, orgFormat);
            s_resourceFormatTransient.erase(&desc);

            resource_view view_non_srgb = { 0 };
            resource_view view_srgb = { 0 };

            reshade::api::format format_non_srgb = format_to_default_typed(desc.texture.format);
            reshade::api::format format_srgb = format_to_default_typed(desc.texture.format, 1);

            if (!isSRGB(orgFormat))
            {
                format_non_srgb = orgFormat;
            }

            device->create_resource_view(handle, resource_usage::render_target,
                resource_view_desc(format_non_srgb), &view_non_srgb);

            device->create_resource_view(handle, resource_usage::render_target,
                resource_view_desc(format_srgb), &view_srgb);

            s_sRGBResourceViews.emplace(handle.handle, make_pair(view_non_srgb, view_srgb));
        }
    }
    lock.unlock();

    if (desc.heap == memory_heap::cpu_to_gpu && static_cast<uint32_t>(desc.usage & resource_usage::constant_buffer))
    {
        std::unique_lock<shared_mutex> colock(constbuffer_mutex);
        s_constantBuffers.emplace(handle.handle);

        if (constantHandlerHooked)
        {
            constantHandlerMemcpy.CreateHostConstantBuffer(device, handle);
            if (initData != nullptr && initData->data != nullptr)
            {
                constantHandlerMemcpy.SetHostConstantBuffer(handle.handle, initData->data, desc.buffer.size, 0, desc.buffer.size);
            }
        }
    }
}


static void onDestroyResource(device* device, resource res)
{
    std::unique_lock<shared_mutex> lock(resource_mutex);

    s_resources.erase(res.handle);
    s_resourceFormat.erase(res.handle);

    if (s_sRGBResourceViews.contains(res.handle))
    {
        auto& views = s_sRGBResourceViews.at(res.handle);

        if (views.first != 0)
            device->destroy_resource_view(views.first);
        if (views.second != 0)
            device->destroy_resource_view(views.second);
    }

    s_sRGBResourceViews.erase(res.handle);

    lock.unlock();

    resource_desc desc = device->get_resource_desc(res);
    if (desc.heap == memory_heap::cpu_to_gpu && static_cast<uint32_t>(desc.usage & resource_usage::constant_buffer))
    {
        std::unique_lock<shared_mutex> colock(constbuffer_mutex);
        s_constantBuffers.erase(res.handle);

        if (constantHandlerHooked)
        {
            constantHandlerMemcpy.DeleteHostConstantBuffer(res);
        }
    }
}


static bool onCreateResourceView(device* device, resource resource, resource_usage usage_type, resource_view_desc& desc)
{
    const resource_desc texture_desc = device->get_resource_desc(resource);
    if (!static_cast<uint32_t>(texture_desc.usage & resource_usage::render_target) || texture_desc.type != resource_type::texture_2d)
        return false;

    std::shared_lock<shared_mutex> lock(resource_mutex);
    if (s_resourceFormat.contains(resource.handle))
    {
        desc.format = s_resourceFormat.at(resource.handle);

        if (desc.type == resource_view_type::unknown)
        {
            desc.type = texture_desc.texture.depth_or_layers > 1 ? resource_view_type::texture_2d_array : resource_view_type::texture_2d;
            desc.texture.first_level = 0;
            desc.texture.level_count = (usage_type == resource_usage::shader_resource) ? UINT32_MAX : 1;
            desc.texture.first_layer = 0;
            desc.texture.layer_count = (usage_type == resource_usage::shader_resource) ? UINT32_MAX : 1;
        }

        return true;
    }

    return false;
}


static void onInitResourceView(device* device, resource resource, resource_usage usage_type, const resource_view_desc& desc, resource_view view)
{
    std::unique_lock<shared_mutex> lock(resource_view_mutex);
    s_resourceViews.emplace(view.handle);
}


static void onDestroyResourceView(device* device, resource_view view)
{
    std::unique_lock<shared_mutex> lock(resource_view_mutex);
    s_resourceViews.erase(view.handle);
}


static void onReshadeReloadedEffects(effect_runtime* runtime)
{
    std::unique_lock<shared_mutex> lock(render_mutex);
    DeviceDataContainer& data = runtime->get_device()->get_private_data<DeviceDataContainer>();
    data.allEnabledTechniques.clear();
    allTechniques.clear();

    enumerateTechniques(data.current_runtime, [&data](effect_runtime* runtime, effect_technique technique, string& name) {
        allTechniques.push_back(name);
    bool enabled = runtime->get_technique_state(technique);

    if (enabled)
    {
        data.allEnabledTechniques.emplace(name, false);
    }
        });
}


static bool onReshadeSetTechniqueState(effect_runtime* runtime, effect_technique technique, bool enabled)
{
    DeviceDataContainer& data = runtime->get_device()->get_private_data<DeviceDataContainer>();
    g_charBufferSize = CHAR_BUFFER_SIZE;
    runtime->get_technique_name(technique, g_charBuffer, &g_charBufferSize);
    string techName(g_charBuffer);

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
            data.allEnabledTechniques.emplace(techName, false);
        }
    }

    return false;
}


static bool CreateTextureBinding(effect_runtime* runtime, resource* res, resource_view* srv, resource_view* rtv, reshade::api::format format)
{
    uint32_t frame_width, frame_height;
    runtime->get_screenshot_width_and_height(&frame_width, &frame_height);

    runtime->get_command_queue()->wait_idle();

    if (!runtime->get_device()->create_resource(
        resource_desc(frame_width, frame_height, 1, 1, format_to_typeless(format), 1, memory_heap::gpu_only, resource_usage::copy_dest | resource_usage::shader_resource | resource_usage::render_target),
        nullptr, resource_usage::shader_resource, res))
    {
        reshade::log_message(ERROR, "Failed to create texture binding resource!");
        return false;
    }

    if (!runtime->get_device()->create_resource_view(*res, resource_usage::shader_resource, resource_view_desc(format_to_default_typed(format, 0)), srv))
    {
        reshade::log_message(ERROR, "Failed to create texture binding resource view!");
        return false;
    }

    if (!runtime->get_device()->create_resource_view(*res, resource_usage::render_target, resource_view_desc(format_to_default_typed(format, 0)), rtv))
    {
        reshade::log_message(ERROR, "Failed to create texture binding resource view!");
        return false;
    }

    return true;
}


static void DestroyTextureBinding(effect_runtime* runtime, const string& binding)
{
    DeviceDataContainer& data = runtime->get_device()->get_private_data<DeviceDataContainer>();

    if (data.bindingMap.contains(binding))
    {
        resource res = { 0 };
        resource_view srv = { 0 };
        resource_view rtv = { 0 };
        reshade::api::format rformat = std::get<1>(data.bindingMap[binding]);

        runtime->get_command_queue()->wait_idle();

        res = std::get<0>(data.bindingMap[binding]);
        if (res != 0)
        {
            runtime->get_device()->destroy_resource(res);
        }

        srv = std::get<2>(data.bindingMap[binding]);
        if (srv != 0)
        {
            runtime->get_device()->destroy_resource_view(srv);
        }

        rtv = std::get<3>(data.bindingMap[binding]);
        if (rtv != 0)
        {
            runtime->get_device()->destroy_resource_view(rtv);
        }

        runtime->update_texture_bindings(binding.c_str(), resource_view{ 0 }, resource_view{ 0 });
        data.bindingMap[binding] = std::make_tuple(resource{ 0 }, rformat, resource_view{ 0 }, resource_view{ 0 });
    }
}


static bool UpdateTextureBinding(effect_runtime* runtime, const string& binding, reshade::api::format format)
{
    DeviceDataContainer& data = runtime->get_device()->get_private_data<DeviceDataContainer>();

    if (data.bindingMap.contains(binding))
    {
        reshade::api::format oldFormat = std::get<1>(data.bindingMap[binding]);
        if (format != oldFormat)
        {
            DestroyTextureBinding(runtime, binding);

            resource res = {};
            resource_view srv = {};
            resource_view rtv = {};

            if (CreateTextureBinding(runtime, &res, &srv, &rtv, format))
            {
                data.bindingMap[binding] = std::make_tuple(res, format, srv, rtv);
                runtime->update_texture_bindings(binding.c_str(), srv);
            }
            else
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }

    return true;
}


static void onInitEffectRuntime(effect_runtime* runtime)
{
    DeviceDataContainer& data = runtime->get_device()->get_private_data<DeviceDataContainer>();
    data.current_runtime = runtime;

    initBackbuffer(runtime);

    // Initialize texture bindings with default format
    for (auto& group : g_addonUIData.GetToggleGroups())
    {
        if (group.second.isProvidingTextureBinding() && group.second.getTextureBindingName().length() > 0)
        {
            resource res = {};
            resource_view srv = {};
            resource_view rtv = {};

            if (CreateTextureBinding(runtime, &res, &srv, &rtv, format::r8g8b8a8_unorm))
            {
                std::unique_lock<shared_mutex> lock(binding_mutex);
                data.bindingMap[group.second.getTextureBindingName()] = std::make_tuple(res, format::r8g8b8a8_unorm, srv, rtv);
                runtime->update_texture_bindings(group.second.getTextureBindingName().c_str(), srv);
            }
        }
    }
}


static void onDestroyEffectRuntime(effect_runtime* runtime)
{
    DeviceDataContainer& data = runtime->get_device()->get_private_data<DeviceDataContainer>();
    data.current_runtime = nullptr;

    std::unique_lock<shared_mutex> lock(binding_mutex);

    for (auto& binding : data.bindingMap)
    {
        DestroyTextureBinding(runtime, binding.first);
    }

    data.bindingMap.clear();

    for (auto& view : s_backBufferView) {
        if (view.second.first != 0)
            runtime->get_device()->destroy_resource_view(view.second.first);
        if (view.second.second != 0)
            runtime->get_device()->destroy_resource_view(view.second.second);
    }

    s_backBufferView.clear();
}


static void onInitPipeline(device* device, pipeline_layout, uint32_t subobjectCount, const pipeline_subobject* subobjects, pipeline pipelineHandle)
{
    // shader has been created, we will now create a hash and store it with the handle we got.
    for (uint32_t i = 0; i < subobjectCount; ++i)
    {
        switch (subobjects[i].type)
        {
        case pipeline_subobject_type::vertex_shader:
        {
            g_vertexShaderManager.addHashHandlePair(calculateShaderHash(subobjects[i].data), pipelineHandle.handle);
        }
        break;
        case pipeline_subobject_type::pixel_shader:
        {
            g_pixelShaderManager.addHashHandlePair(calculateShaderHash(subobjects[i].data), pipelineHandle.handle);
        }
        break;
        }
    }
}


static void onDestroyPipeline(device* device, pipeline pipelineHandle)
{
    g_pixelShaderManager.removeHandle(pipelineHandle.handle);
    g_vertexShaderManager.removeHandle(pipelineHandle.handle);
}


/// <summary>
/// This function will return true if the command list specified has one or more shader hashes which are currently marked. Otherwise false.
/// </summary>
/// <param name="commandList"></param>
/// <returns>true if the draw call has to be blocked</returns>
bool checkDrawCallForCommandList(command_list* commandList)
{
    if (nullptr == commandList)
    {
        return false;
    }

    CommandListDataContainer& commandListData = commandList->get_private_data<CommandListDataContainer>();
    DeviceDataContainer& deviceData = commandList->get_device()->get_private_data<DeviceDataContainer>();

    uint32_t psShaderHash = g_pixelShaderManager.getShaderHash(commandListData.activePixelShaderPipeline);
    uint32_t vsShaderHash = g_vertexShaderManager.getShaderHash(commandListData.activeVertexShaderPipeline);

    vector<const ToggleGroup*> tGroups;

    if ((g_pixelShaderManager.isBlockedShader(psShaderHash) || g_vertexShaderManager.isBlockedShader(vsShaderHash)) &&
        (g_pixelShaderManager.isInHuntingMode() || g_vertexShaderManager.isInHuntingMode()))
    {
        tGroups.push_back(&g_addonUIData.GetToggleGroups()[g_addonUIData.GetToggleGroupIdShaderEditing()]);
    }

    for (auto& group : g_addonUIData.GetToggleGroups())
    {
        if ((group.second.isBlockedPixelShader(psShaderHash) || group.second.isBlockedVertexShader(vsShaderHash)) && group.second.isActive())
        {
            tGroups.push_back(&group.second);
        }
    }

    std::shared_lock<std::shared_mutex> dev_mutex(render_mutex);
    for (auto tGroup : tGroups)
    {
        if (tGroup->isProvidingTextureBinding())
        {
            if (commandListData.bindingsToUpdate.contains(tGroup->getTextureBindingName()))
            {
                if (tGroup->getHistoryIndex() < commandListData.bindingsToUpdate[tGroup->getTextureBindingName()])
                    commandListData.bindingsToUpdate[tGroup->getTextureBindingName()] = tGroup->getHistoryIndex();
            }
            else
            {
                commandListData.bindingsToUpdate.emplace(tGroup->getTextureBindingName(), tGroup->getHistoryIndex());
            }
        }

        if (tGroup->getAllowAllTechniques())
        {
            for (const auto& tech : deviceData.allEnabledTechniques)
            {
                if (tGroup->getHasTechniqueExceptions() && tGroup->preferredTechniques().contains(tech.first))
                {
                    continue;
                }

                if (!tech.second)
                {
                    if (commandListData.techniquesToRender.contains(tech.first))
                    {
                        if (tGroup->getHistoryIndex() < commandListData.techniquesToRender[tech.first])
                            commandListData.techniquesToRender[tech.first] = tGroup->getHistoryIndex();
                    }
                    else
                    {
                        commandListData.techniquesToRender.emplace(tech.first, tGroup->getHistoryIndex());
                    }
                }
            }
        }
        else if (tGroup->preferredTechniques().size() > 0) {
            for (auto& techName : tGroup->preferredTechniques())
            {
                if (deviceData.allEnabledTechniques.contains(techName) && !deviceData.allEnabledTechniques.at(techName))
                {
                    if (commandListData.techniquesToRender.contains(techName))
                    {
                        if (tGroup->getHistoryIndex() < commandListData.techniquesToRender[techName])
                            commandListData.techniquesToRender[techName] = tGroup->getHistoryIndex();
                    }
                    else
                    {
                        commandListData.techniquesToRender.emplace(techName, tGroup->getHistoryIndex());
                    }
                }
            }
        }
    }
    dev_mutex.unlock();

    return commandListData.techniquesToRender.size() > 0 || commandListData.bindingsToUpdate.size() > 0;
}


static const ToggleGroup* checkDescriptors(command_list* commandList)
{
    if (nullptr == commandList)
    {
        return nullptr;
    }

    CommandListDataContainer& commandListData = commandList->get_private_data<CommandListDataContainer>();

    uint32_t psShaderHash = g_pixelShaderManager.getShaderHash(commandListData.activePixelShaderPipeline);
    uint32_t vsShaderHash = g_vertexShaderManager.getShaderHash(commandListData.activeVertexShaderPipeline);

    vector<const ToggleGroup*> tGroups;

    if ((g_pixelShaderManager.isBlockedShader(psShaderHash) || g_vertexShaderManager.isBlockedShader(vsShaderHash)) &&
        (g_pixelShaderManager.isInHuntingMode() || g_vertexShaderManager.isInHuntingMode()))
    {
        tGroups.push_back(&g_addonUIData.GetToggleGroups()[g_addonUIData.GetToggleGroupIdShaderEditing()]);
    }

    for (auto& group : g_addonUIData.GetToggleGroups())
    {
        if ((group.second.isBlockedPixelShader(psShaderHash) || group.second.isBlockedVertexShader(vsShaderHash)) && group.second.isActive())
        {
            tGroups.push_back(&group.second);
        }
    }

    for (auto tGroup : tGroups)
    {
        if (tGroup->getExtractConstants())
        {
            return tGroup;
        }
    }

    return nullptr;
}


static const resource_view GetCurrentResourceView(effect_runtime* runtime, const pair<string, int32_t>& matchObject, CommandListDataContainer& commandListData)
{
    uint32_t index;

    if (commandListData.active_rtv_history.size() > abs(matchObject.second) && matchObject.second <= 0)
    {
        index = abs(matchObject.second);
    }
    else if (matchObject.second < commandListData.active_rtv_history.size() - 1)
    {
        index = commandListData.active_rtv_history.size() - 1;
    }
    else
    {
        index = 0;
    }

    device* device = runtime->get_device();
    const vector<resource_view>& rtvs = commandListData.active_rtv_history.at(index);
    resource_view active_rtv = { 0 };

    for (int i = 0; i < rtvs.size(); i++)
    {
        if (runtime != nullptr && rtvs[i] != 0)
        {
            resource rs = device->get_resource_from_view(rtvs[i]);
            if (rs == 0)
            {
                // Render targets may not have a resource bound in D3D12, in which case writes to them are discarded
                continue;
            }

            const resource_desc texture_desc = device->get_resource_desc(rs);

            uint32_t frame_width, frame_height;
            runtime->get_screenshot_width_and_height(&frame_width, &frame_height);

            if (texture_desc.texture.height == frame_height && texture_desc.texture.width == frame_width)
            {
                active_rtv = rtvs[i];
                break;
            }
        }
    }

    return active_rtv;
}


static void UpdateTextureBindings(command_list* cmd_list, bool dec = false)
{
    if (cmd_list == nullptr || cmd_list->get_device() == nullptr)
    {
        return;
    }

    device* device = cmd_list->get_device();
    CommandListDataContainer& commandListData = cmd_list->get_private_data<CommandListDataContainer>();
    DeviceDataContainer& deviceData = device->get_private_data<DeviceDataContainer>();

    if (deviceData.current_runtime == nullptr || commandListData.active_rtv_history.size() == 0 || commandListData.bindingsToUpdate.size() == 0) {
        return;
    }

    for (const auto& binding : commandListData.bindingsToUpdate)
    {
        if (binding.second <= 0) {
            commandListData.immediateActionSet.emplace(binding);
        }
    }

    if (commandListData.immediateActionSet.size() == 0)
    {
        if (dec)
        {
            for (auto& tech : commandListData.bindingsToUpdate)
            {
                if (commandListData.bindingsToUpdate[tech.first] > 0)
                    commandListData.bindingsToUpdate[tech.first]--;
            }
        }

        return;
    }

    std::unique_lock<shared_mutex> mtx(binding_mutex);
    for (const auto& binding : commandListData.immediateActionSet)
    {
        string bindingName = binding.first;
        effect_runtime* runtime = deviceData.current_runtime;

        if (deviceData.bindingsUpdated.contains(bindingName)) {
            continue;
        }

        resource_view active_rtv = GetCurrentResourceView(runtime, binding, commandListData);

        if (active_rtv == 0)
        {
            continue;
        }

        if (deviceData.bindingMap.contains(bindingName))
        {
            resource res = runtime->get_device()->get_resource_from_view(active_rtv);
            resource target_res = std::get<0>(deviceData.bindingMap[bindingName]);

            resource_desc resDesc = runtime->get_device()->get_resource_desc(res);
            if (UpdateTextureBinding(runtime, bindingName, resDesc.texture.format))
            {
                g_addonUIData.cFormat = resDesc.texture.format;
                cmd_list->copy_resource(res, target_res);
                deviceData.bindingsUpdated.emplace(bindingName);
            }
        }
    }
    mtx.unlock();

    for (auto& tech : commandListData.immediateActionSet)
    {
        commandListData.bindingsToUpdate.erase(tech.first);
    }

    if (dec)
    {
        for (auto& tech : commandListData.bindingsToUpdate)
        {
            if (commandListData.bindingsToUpdate[tech.first] > 0)
                commandListData.bindingsToUpdate[tech.first]--;
        }
    }

    commandListData.immediateActionSet.clear();
}


static void RenderEffects(command_list* cmd_list, bool inc = false)
{
    if (cmd_list == nullptr || cmd_list->get_device() == nullptr)
    {
        return;
    }

    device* device = cmd_list->get_device();
    CommandListDataContainer& commandListData = cmd_list->get_private_data<CommandListDataContainer>();
    DeviceDataContainer& deviceData = device->get_private_data<DeviceDataContainer>();
    bool rendered = false;

    if (deviceData.current_runtime == nullptr || commandListData.active_rtv_history.size() == 0 || commandListData.techniquesToRender.size() == 0) {
        return;
    }

    for (const auto& tech : commandListData.techniquesToRender)
    {
        if (tech.second <= 0) {
            commandListData.immediateActionSet.emplace(tech);
        }
    }

    if (commandListData.immediateActionSet.size() == 0)
    {
        if (inc)
        {
            for (auto& tech : commandListData.techniquesToRender)
            {
                if (commandListData.techniquesToRender[tech.first] > 0)
                    commandListData.techniquesToRender[tech.first]--;
            }
        }

        return;
    }

    std::unique_lock<std::shared_mutex> dev_mutex(render_mutex);
    enumerateTechniques(deviceData.current_runtime, [&deviceData, &commandListData, &cmd_list, &device, &rendered](effect_runtime* runtime, effect_technique technique, string& name) {
        auto historic_rtv = commandListData.immediateActionSet.find(pair<string, int32_t>(name, 0));

    if (historic_rtv != commandListData.immediateActionSet.end() && historic_rtv->second <= 0 && !deviceData.allEnabledTechniques[name])
    {
        resource_view active_rtv = GetCurrentResourceView(runtime, *historic_rtv, commandListData);

        if (active_rtv == 0)
        {
            return;
        }

        resource res = runtime->get_device()->get_resource_from_view(active_rtv);

        resource_view view_non_srgb = active_rtv;
        resource_view view_srgb = active_rtv;

        if (s_sRGBResourceViews.contains(res.handle))
        {
            const auto& views = s_sRGBResourceViews.at(res.handle);
            view_non_srgb = views.first;
            view_srgb = views.second;
        }

        runtime->render_effects(cmd_list, view_non_srgb, view_srgb);
        runtime->render_technique(technique, cmd_list, view_non_srgb, view_srgb);

        resource_desc resDesc = runtime->get_device()->get_resource_desc(res);
        g_addonUIData.cFormat = resDesc.texture.format;

        deviceData.allEnabledTechniques[name] = true;
        deviceData.rendered_effects = true;
        rendered = true;
    }
        });
    dev_mutex.unlock();

    for (auto& tech : commandListData.immediateActionSet)
    {
        commandListData.techniquesToRender.erase(tech.first);
    }
    if (inc)
    {
        for (auto& tech : commandListData.techniquesToRender)
        {

            if (commandListData.techniquesToRender[tech.first] > 0)
                commandListData.techniquesToRender[tech.first]--;
        }
    }

    commandListData.immediateActionSet.clear();

    if (rendered && (device->get_api() == device_api::d3d12 || device->get_api() == device_api::vulkan))
    {
        std::shared_lock<std::shared_mutex> dev_mutex(pipeline_layout_mutex);
        commandListData.stateTracker.ReApplyState(cmd_list, deviceData.transient_mask);
    }
}


static void onBindPipeline(command_list* commandList, pipeline_stage stages, pipeline pipelineHandle)
{
    if (nullptr != commandList && pipelineHandle.handle != 0)
    {
        const bool handleHasPixelShaderAttached = g_pixelShaderManager.isKnownHandle(pipelineHandle.handle);
        const bool handleHasVertexShaderAttached = g_vertexShaderManager.isKnownHandle(pipelineHandle.handle);
        if (!handleHasPixelShaderAttached && !handleHasVertexShaderAttached)
        {
            // draw call with unknown handle, don't collect it
            return;
        }
        CommandListDataContainer& commandListData = commandList->get_private_data<CommandListDataContainer>();
        DeviceDataContainer& deviceData = commandList->get_device()->get_private_data<DeviceDataContainer>();

        if (commandList->get_device()->get_api() == device_api::vulkan)
        {
            commandListData.stateTracker.OnBindPipeline(commandList, stages, pipelineHandle);
        }

        if (!deviceData.current_runtime->get_effects_state())
        {
            return;
        }

        if ((uint32_t)(stages & pipeline_stage::pixel_shader) && handleHasPixelShaderAttached)
        {
            if (g_activeCollectorFrameCounter > 0)
            {
                // in collection mode
                g_pixelShaderManager.addActivePipelineHandle(pipelineHandle.handle);
            }
            commandListData.activePixelShaderPipeline = pipelineHandle.handle;
        }
        else if ((uint32_t)(stages & pipeline_stage::vertex_shader) && handleHasVertexShaderAttached)
        {
            if (g_activeCollectorFrameCounter > 0)
            {
                // in collection mode
                g_vertexShaderManager.addActivePipelineHandle(pipelineHandle.handle);
            }
            commandListData.activeVertexShaderPipeline = pipelineHandle.handle;
        }

        (void)checkDrawCallForCommandList(commandList);

        if (!commandListData.stateTracker.IsInRenderPass())
        {
            UpdateTextureBindings(commandList, false);
            RenderEffects(commandList, false);
        }
    }
}


static void onBindRenderTargetsAndDepthStencil(command_list* cmd_list, uint32_t count, const resource_view* rtvs, resource_view dsv)
{
    if (cmd_list == nullptr || cmd_list->get_device() == nullptr)
    {
        return;
    }

    device* device = cmd_list->get_device();
    CommandListDataContainer& commandListData = cmd_list->get_private_data<CommandListDataContainer>();
    DeviceDataContainer& deviceData = device->get_private_data<DeviceDataContainer>();

    commandListData.stateTracker.OnBindRenderTargetsAndDepthStencil(cmd_list, count, rtvs, dsv);

    if (!deviceData.current_runtime->get_effects_state())
    {
        return;
    }

    UpdateTextureBindings(cmd_list, true);
    RenderEffects(cmd_list, true);

    vector<resource_view> new_views = vector<resource_view>(count);

    for (uint32_t i = 0; i < count; i++)
    {
        new_views[i] = rtvs[i];
    }

    if (commandListData.active_rtv_history.size() >= MAX_RT_HISTORY)
    {
        commandListData.active_rtv_history.pop_back();
    }

    commandListData.active_rtv_history.insert(commandListData.active_rtv_history.begin(), new_views);
}


static void onBeginRenderPass(command_list* cmd_list, uint32_t count, const render_pass_render_target_desc* rts, const render_pass_depth_stencil_desc* ds)
{
    if (cmd_list == nullptr || cmd_list->get_device() == nullptr)
    {
        return;
    }

    device* device = cmd_list->get_device();
    CommandListDataContainer& commandListData = cmd_list->get_private_data<CommandListDataContainer>();
    DeviceDataContainer& deviceData = device->get_private_data<DeviceDataContainer>();

    commandListData.stateTracker.OnBeginRenderPass(cmd_list, count, rts, ds);

    if (!deviceData.current_runtime->get_effects_state())
    {
        return;
    }

    UpdateTextureBindings(cmd_list, true);
    RenderEffects(cmd_list, true);

    vector<resource_view> new_views = vector<resource_view>(count);

    for (uint32_t i = 0; i < count; i++)
    {
        new_views[i] = rts[i].view;
    }

    if (new_views.size() > 0)
    {
        if (commandListData.active_rtv_history.size() >= MAX_RT_HISTORY)
        {
            commandListData.active_rtv_history.pop_back();
        }

        commandListData.active_rtv_history.insert(commandListData.active_rtv_history.begin(), new_views);
    }
}


static void onReshadeBeginEffects(effect_runtime* runtime, command_list* cmd_list, resource_view rtv, resource_view rtv_srgb)
{
    DeviceDataContainer& deviceData = runtime->get_device()->get_private_data<DeviceDataContainer>();
    CommandListDataContainer& cmdData = cmd_list->get_private_data<CommandListDataContainer>();

    if (&cmdData != nullptr && cmdData.techniquesToRender.size() > 0)
    {
        enumerateTechniques(deviceData.current_runtime, [&deviceData](effect_runtime* runtime, effect_technique technique, string& name) {
            if (deviceData.allEnabledTechniques.contains(name))
            {
                deviceData.current_runtime->set_technique_state(technique, false);
            }
            });
    }
}


static void onReshadeFinishEffects(effect_runtime* runtime, command_list* cmd_list, resource_view rtv, resource_view rtv_srgb)
{
    DeviceDataContainer& deviceData = runtime->get_device()->get_private_data<DeviceDataContainer>();
    CommandListDataContainer& cmdData = cmd_list->get_private_data<CommandListDataContainer>();

    if (&cmdData != nullptr && cmdData.techniquesToRender.size() > 0)
    {
        enumerateTechniques(deviceData.current_runtime, [&deviceData](effect_runtime* runtime, effect_technique technique, string& name) {
            if (deviceData.allEnabledTechniques.contains(name))
            {
                deviceData.current_runtime->set_technique_state(technique, true);
            }
            });
    }
}


static void onPushDescriptors(command_list* cmd_list, shader_stage stages, pipeline_layout layout, uint32_t layout_param, const descriptor_set_update& update)
{
    const ToggleGroup* group = nullptr;
    if (update.type == descriptor_type::constant_buffer && (group = checkDescriptors(cmd_list)) != nullptr)
    {
        DeviceDataContainer& deviceData = cmd_list->get_device()->get_private_data<DeviceDataContainer>();

        std::unique_lock<shared_mutex> ulock(constbuffer_mutex);
        if (deviceData.constantsUpdated.contains(group))
        {
            return;
        }

        const buffer_range* buffer = static_cast<const reshade::api::buffer_range*>(update.descriptors);

        for (uint32_t i = update.array_offset; i < update.count; ++i)
        {
            if (!s_constantBuffers.contains(buffer[i].buffer.handle))
                continue;

            constantHandler->SetBufferRange(group, buffer[i],
                cmd_list->get_device(), cmd_list, deviceData.current_runtime->get_command_queue());

            constantHandler->ApplyConstantValues(deviceData.current_runtime, group, g_restVariables);
            deviceData.constantsUpdated.insert(group);
            break;
        }
    }
}


static void onBindDescriptorSets(command_list* cmd_list, shader_stage stages, pipeline_layout layout, uint32_t first, uint32_t count, const descriptor_set* sets)
{
    auto& data = cmd_list->get_private_data<CommandListDataContainer>();
    data.stateTracker.OnBindDescriptorSets(cmd_list, stages, layout, first, count, sets);
}


static void onBindViewports(command_list* cmd_list, uint32_t first, uint32_t count, const viewport* viewports)
{
    auto& data = cmd_list->get_private_data<CommandListDataContainer>();
    data.stateTracker.OnBindViewports(cmd_list, first, count, viewports);
}


static void onBindScissorRects(command_list* cmd_list, uint32_t first, uint32_t count, const rect* rects)
{
    auto& data = cmd_list->get_private_data<CommandListDataContainer>();
    data.stateTracker.OnBindScissorRects(cmd_list, first, count, rects);
}


static void onBindPipelineStates(command_list* cmd_list, uint32_t count, const dynamic_state* states, const uint32_t* values)
{
    auto& data = cmd_list->get_private_data<CommandListDataContainer>();
    data.stateTracker.OnBindPipelineStates(cmd_list, count, states, values);
}


static void onInitPipelineLayout(device* device, uint32_t param_count, const pipeline_layout_param* params, pipeline_layout layout)
{
    unique_lock<shared_mutex> lock(pipeline_layout_mutex);

    auto& data = device->get_private_data<DeviceDataContainer>();
    data.transient_mask[layout.handle].resize(param_count);

    for (uint32_t i = 0; i < param_count; i++)
    {
        if (params[i].type == pipeline_layout_param_type::push_constants)
        {
            data.transient_mask[layout.handle][i] = true;
        }
    }
}


static void onDestroyPipelineLayout(device* device, pipeline_layout layout)
{
    unique_lock<shared_mutex> lock(pipeline_layout_mutex);

    auto& data = device->get_private_data<DeviceDataContainer>();
    data.transient_mask.erase(layout.handle);
}


static void onReshadeOverlay(effect_runtime* runtime)
{
    DisplayOverlay(g_addonUIData, runtime);
}


static void onReshadePresent(effect_runtime* runtime)
{
    device* dev = runtime->get_device();
    DeviceDataContainer& deviceData = dev->get_private_data<DeviceDataContainer>();
    command_queue* queue = runtime->get_command_queue();

    if (deviceData.current_runtime->get_effects_state())
    {
        RenderRemainingEffects(deviceData.current_runtime);
    }

    if (dev->get_api() != device_api::d3d12 && dev->get_api() != device_api::vulkan)
        onResetCommandList(queue->get_immediate_command_list());

    deviceData.rendered_effects = false;

    std::for_each(deviceData.allEnabledTechniques.begin(), deviceData.allEnabledTechniques.end(), [](auto& el) {
        el.second = false;
        });

    deviceData.bindingsUpdated.clear();
    deviceData.constantsUpdated.clear();

    reloadConstantVariables(runtime);
    CheckHotkeys(g_addonUIData, runtime);
}


static void onMapBufferRegion(device* device, resource resource, uint64_t offset, uint64_t size, map_access access, void** data)
{
    if (constantCopyMethod != nullptr)
        constantCopyMethod->OnMapBufferRegion(device, resource, offset, size, access, data);
}


static void onUnmapBufferRegion(device* device, resource resource)
{
    if (constantCopyMethod != nullptr)
        constantCopyMethod->OnUnmapBufferRegion(device, resource);
}


static sig_memcpy* org_memcpy = nullptr;

static void* __fastcall detour_memcpy(void* dest, void* src, size_t size)
{
    if (constantCopyMethod != nullptr)
        constantCopyMethod->OnMemcpy(dest, src, size);

    return org_memcpy(dest, src, size);
}


static void displaySettings(effect_runtime* runtime)
{
    DisplaySettings(g_addonUIData, runtime);
}


static bool InitHooks()
{
    // Initialize MinHook.
    if (MH_Initialize() != MH_OK)
    {
        return false;
    }

    if (g_addonUIData.GetAttemptMemcpyHook())
    {
        if (constantHandlerMemcpy.Hook(&org_memcpy, detour_memcpy))
        {
            constantHandler = &constantHandlerMemcpy;
            g_addonUIData.SetConstantHandler(&constantHandlerMemcpy);
            constantHandlerHooked = true;

            if (g_addonUIData.GetMemcpyAssumeUnnested())
            {
                constantCopyMethod = &constantUnnestedMap;
            }
            else
            {
                constantCopyMethod = &constantNestedMap;
            }

            return true;
        }
    }

    constantHandler = &constantHandlerFallback;
    g_addonUIData.SetConstantHandler(&constantHandlerFallback);

    return true;
}


static bool UnInitHooks()
{
    if (MH_Uninitialize() != MH_OK)
    {
        return false;
    }

    return true;
}



/// <summary>
/// copied from Reshade
/// Returns the path to the module file identified by the specified <paramref name="module"/> handle.
/// </summary>
filesystem::path getModulePath(HMODULE module)
{
    WCHAR buf[4096];
    return GetModuleFileNameW(module, buf, ARRAYSIZE(buf)) ? buf : std::filesystem::path();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        if (!reshade::register_addon(hModule))
        {
            return FALSE;
        }

        g_dll_path = getModulePath(hModule);

        g_addonUIData.SetBasePath(g_dll_path.parent_path());
        g_addonUIData.LoadShaderTogglerIniFile();
        InitHooks();
        reshade::register_event<reshade::addon_event::init_resource>(onInitResource);
        reshade::register_event<reshade::addon_event::create_resource>(onCreateResource);
        reshade::register_event<reshade::addon_event::map_buffer_region>(onMapBufferRegion);
        reshade::register_event<reshade::addon_event::unmap_buffer_region>(onUnmapBufferRegion);
        reshade::register_event<reshade::addon_event::destroy_resource>(onDestroyResource);
        reshade::register_event<reshade::addon_event::create_resource_view>(onCreateResourceView);
        reshade::register_event<reshade::addon_event::init_resource_view>(onInitResourceView);
        reshade::register_event<reshade::addon_event::destroy_resource_view>(onDestroyResourceView);
        reshade::register_event<reshade::addon_event::init_pipeline>(onInitPipeline);
        reshade::register_event<reshade::addon_event::bind_viewports>(onBindViewports);
        reshade::register_event<reshade::addon_event::bind_scissor_rects>(onBindScissorRects);
        reshade::register_event<reshade::addon_event::bind_descriptor_sets>(onBindDescriptorSets);
        reshade::register_event<reshade::addon_event::init_pipeline_layout>(onInitPipelineLayout);
        reshade::register_event<reshade::addon_event::destroy_pipeline_layout>(onDestroyPipelineLayout);
        reshade::register_event<reshade::addon_event::bind_pipeline_states>(onBindPipelineStates);
        reshade::register_event<reshade::addon_event::init_command_list>(onInitCommandList);
        reshade::register_event<reshade::addon_event::destroy_command_list>(onDestroyCommandList);
        reshade::register_event<reshade::addon_event::reset_command_list>(onResetCommandList);
        reshade::register_event<reshade::addon_event::destroy_pipeline>(onDestroyPipeline);
        reshade::register_event<reshade::addon_event::reshade_overlay>(onReshadeOverlay);
        reshade::register_event<reshade::addon_event::reshade_present>(onReshadePresent);
        reshade::register_event<reshade::addon_event::reshade_reloaded_effects>(onReshadeReloadedEffects);
        reshade::register_event<reshade::addon_event::reshade_set_technique_state>(onReshadeSetTechniqueState);
        reshade::register_event<reshade::addon_event::bind_pipeline>(onBindPipeline);
        reshade::register_event<reshade::addon_event::init_device>(onInitDevice);
        reshade::register_event<reshade::addon_event::destroy_device>(onDestroyDevice);
        reshade::register_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(onBindRenderTargetsAndDepthStencil);
        reshade::register_event<reshade::addon_event::begin_render_pass>(onBeginRenderPass);
        reshade::register_event<reshade::addon_event::init_effect_runtime>(onInitEffectRuntime);
        reshade::register_event<reshade::addon_event::destroy_effect_runtime>(onDestroyEffectRuntime);
        reshade::register_event<reshade::addon_event::reshade_begin_effects>(onReshadeBeginEffects);
        reshade::register_event<reshade::addon_event::reshade_finish_effects>(onReshadeFinishEffects);
        reshade::register_event<reshade::addon_event::push_descriptors>(onPushDescriptors);
        reshade::register_overlay(nullptr, &displaySettings);
        break;
    case DLL_PROCESS_DETACH:
        UnInitHooks();
        reshade::unregister_event<reshade::addon_event::reshade_present>(onReshadePresent);
        reshade::unregister_event<reshade::addon_event::map_buffer_region>(onMapBufferRegion);
        reshade::unregister_event<reshade::addon_event::unmap_buffer_region>(onUnmapBufferRegion);
        reshade::unregister_event<reshade::addon_event::destroy_pipeline>(onDestroyPipeline);
        reshade::unregister_event<reshade::addon_event::init_pipeline>(onInitPipeline);
        reshade::unregister_event<reshade::addon_event::reshade_overlay>(onReshadeOverlay);
        reshade::unregister_event<reshade::addon_event::reshade_reloaded_effects>(onReshadeReloadedEffects);
        reshade::unregister_event<reshade::addon_event::reshade_set_technique_state>(onReshadeSetTechniqueState);
        reshade::unregister_event<reshade::addon_event::bind_pipeline>(onBindPipeline);
        reshade::unregister_event<reshade::addon_event::bind_viewports>(onBindViewports);
        reshade::unregister_event<reshade::addon_event::bind_scissor_rects>(onBindScissorRects);
        reshade::unregister_event<reshade::addon_event::bind_descriptor_sets>(onBindDescriptorSets);
        reshade::unregister_event<reshade::addon_event::init_pipeline_layout>(onInitPipelineLayout);
        reshade::unregister_event<reshade::addon_event::destroy_pipeline_layout>(onDestroyPipelineLayout);
        reshade::unregister_event<reshade::addon_event::bind_pipeline_states>(onBindPipelineStates);
        reshade::unregister_event<reshade::addon_event::init_command_list>(onInitCommandList);
        reshade::unregister_event<reshade::addon_event::destroy_command_list>(onDestroyCommandList);
        reshade::unregister_event<reshade::addon_event::reset_command_list>(onResetCommandList);
        reshade::unregister_event<reshade::addon_event::init_device>(onInitDevice);
        reshade::unregister_event<reshade::addon_event::destroy_device>(onDestroyDevice);
        reshade::unregister_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(onBindRenderTargetsAndDepthStencil);
        reshade::unregister_event<reshade::addon_event::begin_render_pass>(onBeginRenderPass);
        reshade::unregister_event<reshade::addon_event::init_effect_runtime>(onInitEffectRuntime);
        reshade::unregister_event<reshade::addon_event::destroy_effect_runtime>(onDestroyEffectRuntime);
        reshade::unregister_event<reshade::addon_event::reshade_begin_effects>(onReshadeBeginEffects);
        reshade::unregister_event<reshade::addon_event::reshade_finish_effects>(onReshadeFinishEffects);
        reshade::unregister_event<reshade::addon_event::push_descriptors>(onPushDescriptors);
        reshade::unregister_event<reshade::addon_event::create_resource>(onCreateResource);
        reshade::unregister_event<reshade::addon_event::init_resource>(onInitResource);
        reshade::unregister_event<reshade::addon_event::destroy_resource>(onDestroyResource);
        reshade::unregister_event<reshade::addon_event::create_resource_view>(onCreateResourceView);
        reshade::unregister_event<reshade::addon_event::init_resource_view>(onInitResourceView);
        reshade::unregister_event<reshade::addon_event::destroy_resource_view>(onDestroyResourceView);
        reshade::unregister_overlay(nullptr, &displaySettings);
        reshade::unregister_addon(hModule);
        break;
    }

    return TRUE;
}
