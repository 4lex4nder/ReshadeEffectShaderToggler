#include <format>
#include "ResourceManager.h"

using namespace Rendering;
using namespace reshade::api;
using namespace Shim::Resources;
using namespace std;

ResourceShimType ResourceManager::ResolveResourceShimType(const string& stype)
{
    if (stype == "none")
        return ResourceShimType::Resource_Shim_None;
    else if (stype == "srgb")
        return ResourceShimType::Resource_Shim_SRGB;
    else if (stype == "ffxiv")
        return ResourceShimType::Resource_Shim_FFXIV;

    return ResourceShimType::Resource_Shim_None;
}

void ResourceManager::Init()
{

    switch (_shimType)
    {
    case Resource_Shim_None:
        rShim = nullptr;
        break;
    case Resource_Shim_SRGB:
    {
        static ResourceShimSRGB srgbShim;
        rShim = &srgbShim;
    }
        break;
    case Resource_Shim_FFXIV:
    {
        static ResourceShimFFXIV ffxivShim;
        rShim = &ffxivShim;
    }
        break;
    default:
        rShim = nullptr;
        break;
    }

    if (rShim != nullptr && rShim->Init())
    {
        reshade::log_message(reshade::log_level::info, std::format("Resource shim initialized").c_str());
    }
    else
    {
        reshade::log_message(reshade::log_level::info, std::format("No resource shim initialized").c_str());
    }
}

void ResourceManager::InitBackbuffer(swapchain* runtime)
{
    // Create backbuffer resource views
    device* dev = runtime->get_device();
    uint32_t count = runtime->get_back_buffer_count();

    resource_desc desc = dev->get_resource_desc(runtime->get_back_buffer(0));

    for (uint32_t i = 0; i < count; ++i)
    {
        resource backBuffer = runtime->get_back_buffer(i);

        resource_view backBufferView = { 0 };
        resource_view backBufferViewSRGB = { 0 };

        resource_view srv_non_srgb = { 0 };
        resource_view srv_srgb = { 0 };

        reshade::api::format viewFormat = format_to_default_typed(desc.texture.format, 0);
        reshade::api::format viewFormatSRGB = format_to_default_typed(desc.texture.format, 1);

        dev->create_resource_view(backBuffer, resource_usage::render_target,
            resource_view_desc(viewFormat), &backBufferView);
        dev->create_resource_view(backBuffer, resource_usage::render_target,
            resource_view_desc(viewFormatSRGB), &backBufferViewSRGB);

        dev->create_resource_view(backBuffer, resource_usage::shader_resource,
            resource_view_desc(viewFormat), &srv_non_srgb);
        dev->create_resource_view(backBuffer, resource_usage::shader_resource,
            resource_view_desc(viewFormatSRGB), &srv_srgb);

        _resourceViewRefCount.emplace(backBuffer.handle, 1);
        s_SRVs.emplace(backBuffer.handle, make_pair(srv_non_srgb, srv_srgb));
        s_sRGBResourceViews.emplace(backBuffer.handle, make_pair(backBufferView, backBufferViewSRGB));
    }
}

void ResourceManager::ClearBackbuffer(reshade::api::swapchain* runtime)
{
    device* dev = runtime->get_device();

    uint32_t count = runtime->get_back_buffer_count();

    for (uint32_t i = 0; i < count; ++i)
    {
        resource backBuffer = runtime->get_back_buffer(i);

        const auto& entry = s_sRGBResourceViews.find(backBuffer.handle);

        // Back buffer resource got probably resized, clear old views and reinitialize
        if (entry != s_sRGBResourceViews.end())
        {
            const auto& [oldbackBufferView, oldbackBufferViewSRGB] = entry->second;

            if (oldbackBufferView != 0)
            {
                runtime->get_device()->destroy_resource_view(oldbackBufferView);
            }

            if (oldbackBufferViewSRGB != 0)
            {
                runtime->get_device()->destroy_resource_view(oldbackBufferViewSRGB);
            }
        }

        const auto& entry_views = s_SRVs.find(backBuffer.handle);

        if (entry_views != s_SRVs.end())
        {
            const auto& [oldbackBufferView, oldbackBufferViewSRGB] = entry_views->second;

            if (oldbackBufferView != 0)
            {
                runtime->get_device()->destroy_resource_view(oldbackBufferView);
            }

            if (oldbackBufferViewSRGB != 0)
            {
                runtime->get_device()->destroy_resource_view(oldbackBufferViewSRGB);
            }
        }

        _resourceViewRefCount.erase(backBuffer.handle);
        s_SRVs.erase(backBuffer.handle);
        s_sRGBResourceViews.erase(backBuffer.handle);
    }
}

static inline bool isValidShaderResource(reshade::api::format format)
{
    return format != reshade::api::format::intz;
}

void ResourceManager::CreateViews(reshade::api::device* device, reshade::api::resource resource)
{
    resource_desc rdesc = device->get_resource_desc(resource);

    if ((static_cast<uint32_t>(rdesc.usage & resource_usage::render_target) || static_cast<uint32_t>(rdesc.usage & resource_usage::shader_resource)) &&
        rdesc.type == resource_type::texture_2d)
    {
        unique_lock<shared_mutex> vlock(view_mutex);

        const auto& cRef = _resourceViewRefCount.find(resource.handle);
        if (cRef == _resourceViewRefCount.end())
        {
            unique_lock<shared_mutex> lock(resource_mutex);

            resource_view view_non_srgb = { 0 };
            resource_view view_srgb = { 0 };

            resource_view srv_non_srgb = { 0 };
            resource_view srv_srgb = { 0 };

            reshade::api::format format_non_srgb = format_to_default_typed(rdesc.texture.format, 0);
            reshade::api::format format_srgb = format_to_default_typed(rdesc.texture.format, 1);

            if (static_cast<uint32_t>(rdesc.usage & resource_usage::render_target))
            {
                device->create_resource_view(resource, resource_usage::render_target,
                    resource_view_desc(format_non_srgb), &view_non_srgb);
                device->create_resource_view(resource, resource_usage::render_target,
                    resource_view_desc(format_srgb), &view_srgb);

                s_sRGBResourceViews.emplace(resource.handle, make_pair(view_non_srgb, view_srgb));
            }

            if (static_cast<uint32_t>(rdesc.usage & resource_usage::shader_resource) && isValidShaderResource(rdesc.texture.format))
            {
                device->create_resource_view(resource, resource_usage::shader_resource,
                    resource_view_desc(format_non_srgb), &srv_non_srgb);
                device->create_resource_view(resource, resource_usage::shader_resource,
                    resource_view_desc(format_srgb), &srv_srgb);

                s_SRVs.emplace(resource.handle, make_pair(srv_non_srgb, srv_srgb));
            }
        }

        _resourceViewRefCount[resource.handle]++;
    }
}

void ResourceManager::DisposeView(device* device, uint64_t handle)
{
    const auto it = s_sRGBResourceViews.find(handle);

    if (it != s_sRGBResourceViews.end())
    {
        auto& [view, srgbView] = it->second;

        if (view != 0)
            device->destroy_resource_view(view);
        if (srgbView != 0)
            device->destroy_resource_view(srgbView);

        s_sRGBResourceViews.erase(it);
    }

    const auto sit = s_SRVs.find(handle);

    if (sit != s_SRVs.end())
    {
        auto& [srv1, srv2] = sit->second;

        if (srv1 != 0)
            device->destroy_resource_view(srv1);
        if (srv2 != 0)
            device->destroy_resource_view(srv2);

        s_SRVs.erase(sit);
    }

    const auto rIt = _resourceViewRefCount.find(handle);
    if (rIt != _resourceViewRefCount.end())
    {
        _resourceViewRefCount.erase(rIt);
    }
}

bool ResourceManager::OnCreateSwapchain(reshade::api::swapchain_desc& desc, void* hwnd)
{
    return false;
}

void ResourceManager::OnInitSwapchain(reshade::api::swapchain* swapchain)
{
    InitBackbuffer(swapchain);
}

void ResourceManager::OnDestroySwapchain(reshade::api::swapchain* swapchain)
{
    ClearBackbuffer(swapchain);
}

bool ResourceManager::OnCreateResource(device* device, resource_desc& desc, subresource_data* initial_data, resource_usage initial_state)
{
    bool ret = false;

    if (static_cast<uint32_t>(desc.usage & resource_usage::render_target) &&
        !static_cast<uint32_t>(desc.usage & resource_usage::shader_resource) &&
        desc.type == resource_type::texture_2d)
    {
        desc.usage |= resource_usage::shader_resource;
        ret = true;
    }

    if (rShim != nullptr)
    {
        ret |= rShim->OnCreateResource(device, desc, initial_data, initial_state);
    }
    
    return ret;
}

void ResourceManager::OnInitResource(device* device, const resource_desc& desc, const subresource_data* initData, resource_usage usage, reshade::api::resource handle)
{
    auto& data = device->get_private_data<DeviceDataContainer>();

    if (rShim != nullptr)
    {
        rShim->OnInitResource(device, desc, initData, usage, handle);
    }

    if (device->get_api() > device_api::d3d11)
    {
        CreateViews(device, handle);
    }
}

void ResourceManager::OnDestroyResource(device* device, resource res)
{
    if (rShim != nullptr)
    {
        rShim->OnDestroyResource(device, res);
    }

    if (view_mutex.try_lock())
    {
        if (resource_mutex.try_lock())
        {
            DisposeView(device, res.handle);
            resource_mutex.unlock();
        }
    
        view_mutex.unlock();
    }
}

void ResourceManager::OnDestroyDevice(device* device)
{
    //unique_lock<shared_mutex> lock(resource_mutex);
    //
    //for (auto it = s_sRGBResourceViews.begin(); it != s_sRGBResourceViews.end();)
    //{
    //    auto& views = it->second;
    //
    //    if (views.first != 0)
    //        device->destroy_resource_view(views.first);
    //    if (views.second != 0)
    //        device->destroy_resource_view(views.second);
    //
    //    it = s_sRGBResourceViews.erase(it);
    //}
    //
    //unique_lock<shared_mutex> vlock(view_mutex);
    //_resourceViewRefCount.clear();
    //_resourceViewRef.clear();
}


bool ResourceManager::OnCreateResourceView(device* device, resource resource, resource_usage usage_type, resource_view_desc& desc)
{
    if (rShim != nullptr)
    {
        return rShim->OnCreateResourceView(device, resource, usage_type, desc);
    }

    return false;
}

void ResourceManager::OnInitResourceView(device* device, resource resource, resource_usage usage_type, const resource_view_desc& desc, resource_view view)
{
    if (resource == 0 || device->get_api() > device_api::d3d11)
        return;

    CreateViews(device, resource);

    unique_lock<shared_mutex> vlock(view_mutex);
    _resourceViewToResource.emplace(view.handle, resource.handle);
}

void ResourceManager::OnDestroyResourceView(device* device, resource_view view)
{
    if (device->get_api() > device_api::d3d11)
        return;

    unique_lock<shared_mutex> lock(view_mutex);

    const auto& res = _resourceViewToResource.find(view.handle);
    auto curCount = _resourceViewRefCount.find(res->second);

    if (curCount != _resourceViewRefCount.end())
    {
        if (curCount->second > 1)
        {
            curCount->second--;
        }
        else
        {
            DisposeView(device, res->second);
        }
    }

    _resourceViewToResource.erase(res);
}

void ResourceManager::SetResourceViewHandles(uint64_t handle, reshade::api::resource_view* non_srgb_view, reshade::api::resource_view* srgb_view)
{
    const auto& it = s_sRGBResourceViews.find(handle);
    if (it != s_sRGBResourceViews.end())
    {
        std::tie(*non_srgb_view, *srgb_view) = it->second;
    }
}

void ResourceManager::SetShaderResourceViewHandles(uint64_t handle, reshade::api::resource_view* non_srgb_view, reshade::api::resource_view* srgb_view)
{
    const auto& it = s_SRVs.find(handle);
    if (it != s_SRVs.end())
    {
        std::tie(*non_srgb_view, *srgb_view) = it->second;
    }
}

void ResourceManager::DisposePreview(reshade::api::effect_runtime* runtime)
{
    if (preview_res[0] == 0 && preview_res[1] == 0)
        return;

    runtime->get_command_queue()->wait_idle();

    for (uint32_t i = 0; i < 2; i++)
    {
        if (preview_srv[i] != 0)
        {
            runtime->get_device()->destroy_resource_view(preview_srv[i]);
        }

        if (preview_rtv[i] != 0)
        {
            runtime->get_device()->destroy_resource_view(preview_rtv[i]);
        }

        if (preview_res[i] != 0)
        {
            runtime->get_device()->destroy_resource(preview_res[i]);
        }

        preview_res[i] = resource{ 0 };
        preview_srv[i] = resource_view{ 0 };
        preview_rtv[i] = resource_view{ 0 };
    }
}

void ResourceManager::CheckPreview(reshade::api::command_list* cmd_list, reshade::api::device* device, reshade::api::effect_runtime* runtime)
{
    DeviceDataContainer& deviceData = device->get_private_data<DeviceDataContainer>();

    if (deviceData.huntPreview.recreate_preview)
    {
        DisposePreview(runtime);
        resource_desc desc = deviceData.huntPreview.target_desc;
        resource_desc preview_desc[2] = {
            resource_desc(desc.texture.width, desc.texture.height, 1, 1, format_to_typeless(desc.texture.format), 1, memory_heap::gpu_only, resource_usage::copy_dest | resource_usage::copy_source | resource_usage::shader_resource | resource_usage::render_target),
            resource_desc(desc.texture.width, desc.texture.height, 1, 1, format_to_typeless(desc.texture.format), 1, memory_heap::gpu_only, resource_usage::copy_dest | resource_usage::shader_resource | resource_usage::render_target)
        };

        for (uint32_t i = 0; i < 2; i++)
        {
            if (!runtime->get_device()->create_resource(preview_desc[i], nullptr, resource_usage::shader_resource, &preview_res[i]))
            {
                reshade::log_message(reshade::log_level::error, "Failed to create preview render target!");
            }

            if (preview_res[i] != 0 && !runtime->get_device()->create_resource_view(preview_res[i], resource_usage::shader_resource, resource_view_desc(format_to_default_typed(preview_desc[i].texture.format, 0)), &preview_srv[i]))
            {
                reshade::log_message(reshade::log_level::error, "Failed to create preview shader resource view!");
            }

            if (preview_res[i] != 0 && !runtime->get_device()->create_resource_view(preview_res[i], resource_usage::render_target, resource_view_desc(format_to_default_typed(preview_desc[i].texture.format, 0)), &preview_rtv[i]))
            {
                reshade::log_message(reshade::log_level::error, "Failed to create preview render target view!");
            }
        }
    }
}

void ResourceManager::SetPingPreviewHandles(reshade::api::resource* res, reshade::api::resource_view* rtv, reshade::api::resource_view* srv)
{
    if (preview_res[0] != 0)
    {
        if(res != nullptr)
            *res = preview_res[0];
        if(rtv != nullptr)
            *rtv = preview_rtv[0];
        if(srv != nullptr)
            *srv = preview_srv[0];
    }
}

void ResourceManager::SetPongPreviewHandles(reshade::api::resource* res, reshade::api::resource_view* rtv, reshade::api::resource_view* srv)
{
    if (preview_res[1] != 0)
    {
        if (res != nullptr)
            *res = preview_res[1];
        if (rtv != nullptr)
            *rtv = preview_rtv[1];
        if (srv != nullptr)
            *srv = preview_srv[1];
    }
}

bool ResourceManager::IsCompatibleWithPreviewFormat(reshade::api::effect_runtime* runtime, reshade::api::resource res)
{
    if (res == 0 || preview_res[0] == 0)
        return false;

    resource_desc tdesc = runtime->get_device()->get_resource_desc(res);
    resource_desc preview_desc = runtime->get_device()->get_resource_desc(preview_res[0]);

    if ((format_to_typeless(tdesc.texture.format) == preview_desc.texture.format || tdesc.texture.format == preview_desc.texture.format) &&
        tdesc.texture.width == preview_desc.texture.width &&
        tdesc.texture.height == preview_desc.texture.height)
    {
        return true;
    }

    return false;
}

EmbeddedResourceData ResourceManager::GetResourceData(uint16_t id) {
    HMODULE hModule = NULL;
    GetModuleHandleEx(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        (LPCTSTR)GetResourceData,
        &hModule);

    HRSRC myResource = ::FindResource(hModule, MAKEINTRESOURCE(id), RT_RCDATA);

    if (myResource != 0)
    {
        DWORD myResourceSize = SizeofResource(hModule, myResource);
        HGLOBAL myResourceData = LoadResource(hModule, myResource);

        if (myResourceData != 0)
        {
            const char* pMyBinaryData = static_cast<const char*>(LockResource(myResourceData));
            return EmbeddedResourceData{ pMyBinaryData, myResourceSize };
        }
    }

    return EmbeddedResourceData{ nullptr, 0 };
}