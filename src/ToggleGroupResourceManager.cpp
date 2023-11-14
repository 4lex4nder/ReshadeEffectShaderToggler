#include "ToggleGroupResourceManager.h"

using namespace Rendering;
using namespace reshade::api;
using namespace std;

void ToggleGroupResourceManager::DisposeGroupBuffers(reshade::api::effect_runtime* runtime)
{
    runtime->get_command_queue()->wait_idle();

    for (auto& buf : group_buffers)
    {
        auto& [buf_res, buf_rtv, buf_rtv_srgb, buf_srv] = buf.second;

        if (buf_srv != 0)
        {
            runtime->get_device()->destroy_resource_view(buf_srv);
        }

        if (buf_rtv != 0)
        {
            runtime->get_device()->destroy_resource_view(buf_rtv);
        }

        if (buf_rtv_srgb != 0)
        {
            runtime->get_device()->destroy_resource_view(buf_rtv_srgb);
        }

        if (buf_res != 0)
        {
            runtime->get_device()->destroy_resource(buf_res);
        }

        buf_res = resource{ 0 };
        buf_srv = resource_view{ 0 };
        buf_rtv = resource_view{ 0 };
        buf_rtv_srgb = resource_view{ 0 };
    }
}

void ToggleGroupResourceManager::CheckGroupBuffers(reshade::api::command_list* cmd_list, reshade::api::device* device, reshade::api::effect_runtime* runtime)
{
    runtime->get_command_queue()->wait_idle();

    for (auto& groupEntry : group_buffers)
    {
        ShaderToggler::ToggleGroup* group = groupEntry.first;
        auto& [buf_res, buf_rtv, buf_rtv_srgb, buf_srv] = groupEntry.second;

        if (!group->getRecreateBuffer())
        {
            continue;
        }

        if (buf_srv != 0)
        {
            runtime->get_device()->destroy_resource_view(buf_srv);
        }

        if (buf_rtv != 0)
        {
            runtime->get_device()->destroy_resource_view(buf_rtv);
        }

        if (buf_rtv_srgb != 0)
        {
            runtime->get_device()->destroy_resource_view(buf_rtv_srgb);
        }

        if (buf_res != 0)
        {
            runtime->get_device()->destroy_resource(buf_res);
        }

        buf_res = resource{ 0 };
        buf_srv = resource_view{ 0 };
        buf_rtv = resource_view{ 0 };
        buf_rtv_srgb = resource_view{ 0 };

        resource_desc desc = group->getTargetBufferDescription();
        resource_desc group_desc = resource_desc(desc.texture.width, desc.texture.height, 1, 1, desc.texture.format, 1, memory_heap::gpu_only, resource_usage::copy_dest | resource_usage::copy_source | resource_usage::shader_resource | resource_usage::render_target);

        if (!runtime->get_device()->create_resource(group_desc, nullptr, resource_usage::copy_dest, &buf_res))
        {
            reshade::log_message(reshade::log_level::error, "Failed to create group render target!");
        }

        if (buf_res != 0 && !runtime->get_device()->create_resource_view(buf_res, resource_usage::shader_resource, resource_view_desc(group_desc.texture.format), &buf_srv))
        {
            reshade::log_message(reshade::log_level::error, "Failed to create group shader resource view!");
        }

        if (buf_res != 0 && !runtime->get_device()->create_resource_view(buf_res, resource_usage::render_target, resource_view_desc(format_to_default_typed(group_desc.texture.format, 0)), &buf_rtv))
        {
            reshade::log_message(reshade::log_level::error, "Failed to create group render target view!");
        }

        if (buf_res != 0 && !runtime->get_device()->create_resource_view(buf_res, resource_usage::render_target, resource_view_desc(format_to_default_typed(group_desc.texture.format, 1)), &buf_rtv_srgb))
        {
            reshade::log_message(reshade::log_level::error, "Failed to create group SRGB render target view!");
        }

        group->setRecreateBuffer(false);
    }
}

void ToggleGroupResourceManager::SetGroupBufferHandles(ShaderToggler::ToggleGroup* group, reshade::api::resource* res, reshade::api::resource_view* rtv, reshade::api::resource_view* rtv_srgb, reshade::api::resource_view* srv)
{

    const auto& [buf_res, buf_rtv, buf_rtv_srgb, buf_srv] = group_buffers[group];
    
    if (res != nullptr)
        *res = buf_res;
    if (rtv != nullptr)
        *rtv = buf_rtv;
    if (rtv_srgb != nullptr)
        *rtv_srgb = buf_rtv_srgb;
    if (srv != nullptr)
        *srv = buf_srv;
}

bool ToggleGroupResourceManager::IsCompatibleWithGroupFormat(reshade::api::effect_runtime* runtime, reshade::api::resource res, ShaderToggler::ToggleGroup* group)
{
    const auto& [buf_res, buf_rtv, buf_rtv_srgb, buf_srv] = group_buffers[group];
    
    if (res == 0 || buf_res == 0)
        return false;
    
    resource_desc tdesc = runtime->get_device()->get_resource_desc(res);
    resource_desc preview_desc = runtime->get_device()->get_resource_desc(buf_res);
    
    if (tdesc.texture.format == preview_desc.texture.format &&
        tdesc.texture.width == preview_desc.texture.width &&
        tdesc.texture.height == preview_desc.texture.height)
    {
        return true;
    }

    return false;
}