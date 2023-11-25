#include "ToggleGroupResourceManager.h"

using namespace Rendering;
using namespace reshade::api;
using namespace std;
using namespace ShaderToggler;

static bool isValidRenderTarget(reshade::api::format format)
{
    switch (format)
    {
    case reshade::api::format::r1_unorm:
    case reshade::api::format::l8_unorm:
    case reshade::api::format::a8_unorm:
    case reshade::api::format::r8_typeless:
    case reshade::api::format::r8_uint:
    case reshade::api::format::r8_sint:
    case reshade::api::format::r8_unorm:
    case reshade::api::format::r8_snorm:
    case reshade::api::format::l8a8_unorm:
    case reshade::api::format::r8g8_typeless:
    case reshade::api::format::r8g8_uint:
    case reshade::api::format::r8g8_sint:
    case reshade::api::format::r8g8_unorm:
    case reshade::api::format::r8g8_snorm:
    case reshade::api::format::r8g8b8a8_typeless:
    case reshade::api::format::r8g8b8a8_uint:
    case reshade::api::format::r8g8b8a8_sint:
    case reshade::api::format::r8g8b8a8_unorm:
    case reshade::api::format::r8g8b8a8_unorm_srgb:
    case reshade::api::format::r8g8b8a8_snorm:
    case reshade::api::format::r8g8b8x8_unorm:
    case reshade::api::format::r8g8b8x8_unorm_srgb:
    case reshade::api::format::b8g8r8a8_typeless:
    case reshade::api::format::b8g8r8a8_unorm:
    case reshade::api::format::b8g8r8a8_unorm_srgb:
    case reshade::api::format::b8g8r8x8_typeless:
    case reshade::api::format::b8g8r8x8_unorm:
    case reshade::api::format::b8g8r8x8_unorm_srgb:
    case reshade::api::format::r10g10b10a2_typeless:
    case reshade::api::format::r10g10b10a2_uint:
    case reshade::api::format::r10g10b10a2_unorm:
    case reshade::api::format::r10g10b10a2_xr_bias:
    case reshade::api::format::b10g10r10a2_typeless:
    case reshade::api::format::b10g10r10a2_uint:
    case reshade::api::format::b10g10r10a2_unorm:
    case reshade::api::format::l16_unorm:
    case reshade::api::format::r16_typeless:
    case reshade::api::format::r16_uint:
    case reshade::api::format::r16_sint:
    case reshade::api::format::r16_unorm:
    case reshade::api::format::r16_snorm:
    case reshade::api::format::r16_float:
    case reshade::api::format::l16a16_unorm:
    case reshade::api::format::r16g16_typeless:
    case reshade::api::format::r16g16_uint:
    case reshade::api::format::r16g16_sint:
    case reshade::api::format::r16g16_unorm:
    case reshade::api::format::r16g16_snorm:
    case reshade::api::format::r16g16_float:
    case reshade::api::format::r16g16b16a16_typeless:
    case reshade::api::format::r16g16b16a16_uint:
    case reshade::api::format::r16g16b16a16_sint:
    case reshade::api::format::r16g16b16a16_unorm:
    case reshade::api::format::r16g16b16a16_snorm:
    case reshade::api::format::r16g16b16a16_float:
    case reshade::api::format::r32_typeless:
    case reshade::api::format::r32_uint:
    case reshade::api::format::r32_sint:
    case reshade::api::format::r32_float:
    case reshade::api::format::r32g32_typeless:
    case reshade::api::format::r32g32_uint:
    case reshade::api::format::r32g32_sint:
    case reshade::api::format::r32g32_float:
    case reshade::api::format::r32g32b32_typeless:
    case reshade::api::format::r32g32b32_uint:
    case reshade::api::format::r32g32b32_sint:
    case reshade::api::format::r32g32b32_float:
    case reshade::api::format::r32g32b32a32_typeless:
    case reshade::api::format::r32g32b32a32_uint:
    case reshade::api::format::r32g32b32a32_sint:
    case reshade::api::format::r32g32b32a32_float:
    case reshade::api::format::r9g9b9e5:
    case reshade::api::format::r11g11b10_float:
    case reshade::api::format::b5g6r5_unorm:
    case reshade::api::format::b5g5r5a1_unorm:
    case reshade::api::format::b5g5r5x1_unorm:
    case reshade::api::format::b4g4r4a4_unorm:
    case reshade::api::format::a4b4g4r4_unorm:
        return true;
    default:
        return false;
    }

    return false;
}

void ToggleGroupResourceManager::ToggleGroupRemoved(reshade::api::effect_runtime* runtime, ShaderToggler::ToggleGroup* group)
{
    runtime->get_command_queue()->wait_idle();

    for (uint32_t i = 0; i < GroupResourceTypeCount; i++)
    {
        GroupResource& resources = group->GetGroupResource(static_cast<GroupResourceType>(i));

        if (resources.owning)
        {
            DisposeGroupResources(runtime, resources.res, resources.rtv, resources.rtv_srgb, resources.srv);
        }
    }
}

void ToggleGroupResourceManager::DisposeGroupResources(effect_runtime* runtime, resource& res, resource_view& rtv, resource_view& rtv_srgb, resource_view& srv)
{
    if (srv != 0)
    {
        runtime->get_device()->destroy_resource_view(srv);
    }

    if (rtv != 0)
    {
        runtime->get_device()->destroy_resource_view(rtv);
    }

    if (rtv_srgb != 0)
    {
        runtime->get_device()->destroy_resource_view(rtv_srgb);
    }

    if (res != 0)
    {
        runtime->get_device()->destroy_resource(res);
    }

    res = resource{ 0 };
    srv = resource_view{ 0 };
    rtv = resource_view{ 0 };
    rtv_srgb = resource_view{ 0 };
}

void ToggleGroupResourceManager::DisposeGroupBuffers(reshade::api::effect_runtime* runtime, std::unordered_map<int, ShaderToggler::ToggleGroup>& groups)
{
    runtime->get_command_queue()->wait_idle();
    
    for (auto& groupEntry : groups)
    {
        ShaderToggler::ToggleGroup& group = groupEntry.second;

        for (uint32_t i = 0; i < GroupResourceTypeCount; i++)
        {
            GroupResource& resources = group.GetGroupResource(static_cast<GroupResourceType>(i));

            if (!resources.owning)
                continue;

            DisposeGroupResources(runtime, resources.res, resources.rtv, resources.rtv_srgb, resources.srv);
        }
    }
}

void ToggleGroupResourceManager::CheckGroupBuffers(reshade::api::effect_runtime* runtime, std::unordered_map<int, ShaderToggler::ToggleGroup>& groups)
{
    runtime->get_command_queue()->wait_idle();

    for (auto& groupEntry : groups)
    {
        ShaderToggler::ToggleGroup& group = groupEntry.second;

        for (uint32_t i = 0; i < GroupResourceTypeCount; i++)
        {
            GroupResource& resources = group.GetGroupResource(static_cast<GroupResourceType>(i));

            if (!resources.owning)
                continue;

            // Dispose of buffers with the alpha preservation option disabled
            if (!resources.enabled())
            {
                DisposeGroupResources(runtime, resources.res, resources.rtv, resources.rtv_srgb, resources.srv);
                continue;
            }

            // Skip buffers not marked for recreation
            if (resources.state != GroupResourceState::RESOURCE_INVALID)
                continue;

            DisposeGroupResources(runtime, resources.res, resources.rtv, resources.rtv_srgb, resources.srv);

            if (static_cast<GroupResourceType>(i) == GroupResourceType::RESOURCE_ALPHA || static_cast<GroupResourceType>(i) == GroupResourceType::RESOURCE_BINDING)
            {
                reshade::api::resource_usage res_usage = resource_usage::copy_dest | resource_usage::copy_source | resource_usage::shader_resource;

                bool validRT = isValidRenderTarget(resources.target_description.texture.format);
                if (validRT)
                {
                    res_usage |= resource_usage::render_target;
                }

                resource_desc desc = resources.target_description;
                resource_desc group_desc = resource_desc(desc.texture.width, desc.texture.height, 1, 1, desc.texture.format, 1, memory_heap::gpu_only, res_usage);

                if (!runtime->get_device()->create_resource(group_desc, nullptr, resource_usage::copy_dest, &resources.res))
                {
                    reshade::log_message(reshade::log_level::error, "Failed to create group render target!");
                }

                if (validRT && resources.res != 0 && !runtime->get_device()->create_resource_view(resources.res, resource_usage::shader_resource, resource_view_desc(format_to_default_typed(group_desc.texture.format, 0)), &resources.srv))
                {
                    reshade::log_message(reshade::log_level::error, "Failed to create group shader resource view!");
                }

                if (validRT && resources.res != 0 && !runtime->get_device()->create_resource_view(resources.res, resource_usage::render_target, resource_view_desc(format_to_default_typed(group_desc.texture.format, 0)), &resources.rtv))
                {
                    reshade::log_message(reshade::log_level::error, "Failed to create group render target view!");
                }

                if (resources.res != 0 && !runtime->get_device()->create_resource_view(resources.res, resource_usage::render_target, resource_view_desc(format_to_default_typed(group_desc.texture.format, 1)), &resources.rtv_srgb))
                {
                    reshade::log_message(reshade::log_level::error, "Failed to create group SRGB render target view!");
                }
            }
            else if (static_cast<GroupResourceType>(i) == GroupResourceType::RESOURCE_CONSTANTS_COPY)
            {
                if (!runtime->get_device()->create_resource(resource_desc(resources.target_description.buffer.size, memory_heap::gpu_to_cpu, resource_usage::copy_dest | resource_usage::copy_source), nullptr, resource_usage::copy_dest, &resources.res))
                {
                    reshade::log_message(reshade::log_level::error, "Failed to create group constant copy buffer!");
                }
            }

            resources.state = GroupResourceState::RESOURCE_RECREATED;
        }
    }
}

void ToggleGroupResourceManager::SetGroupBufferHandles(ShaderToggler::ToggleGroup* group, const GroupResourceType type, reshade::api::resource* res, reshade::api::resource_view* rtv, reshade::api::resource_view* rtv_srgb, reshade::api::resource_view* srv)
{
    const GroupResource& resources = group->GetGroupResource(type);
    
    if (res != nullptr)
        *res = resources.res;
    if (rtv != nullptr)
        *rtv = resources.rtv;
    if (rtv_srgb != nullptr)
        *rtv_srgb = resources.rtv_srgb;
    if (srv != nullptr)
        *srv = resources.srv;
}

bool ToggleGroupResourceManager::IsCompatibleWithGroupFormat(reshade::api::device* device, const GroupResourceType type, reshade::api::resource res, ShaderToggler::ToggleGroup* group)
{
    const GroupResource& resources = group->GetGroupResource(type);
    
    if (res == 0 || resources.res == 0)
        return false;
    
    resource_desc tdesc = device->get_resource_desc(res);
    resource_desc preview_desc = device->get_resource_desc(resources.res);
    
    if (type == GroupResourceType::RESOURCE_ALPHA || type == GroupResourceType::RESOURCE_BINDING)
    {
        if (tdesc.texture.format == preview_desc.texture.format &&
            tdesc.texture.width == preview_desc.texture.width &&
            tdesc.texture.height == preview_desc.texture.height &&
            tdesc.texture.levels == preview_desc.texture.levels)
        {
            return true;
        }
    }
    else if (type == GroupResourceType::RESOURCE_CONSTANTS_COPY)
    {
        if (tdesc.buffer.size == preview_desc.buffer.size)
        {
            return true;
        }
    }

    return false;
}