#pragma once

#include <reshade.hpp>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <functional>
#include "PipelinePrivateData.h"
#include "ResourceShim.h"
#include "ResourceShimSRGB.h"
#include "ResourceShimFFXIV.h"

namespace Rendering
{
    enum ResourceShimType
    {
        Resource_Shim_None = 0,
        Resource_Shim_SRGB,
        Resource_Shim_FFXIV
    };

    static const std::vector<std::string> ResourceShimNames = {
        "none",
        "srgb",
        "ffxiv"
    };

    struct EmbeddedResourceData
    {
        const void* data;
        size_t size;
    };

    enum class GlobalResourceState : uint32_t
    {
        RESOURCE_UNINITIALIZED = 0,
        RESOURCE_VALID = 1,
        RESOURCE_USED = 2,
        RESOURCE_INVALID = 3,
    };

    constexpr uint32_t GLOBAL_RESOURCE_TTL = 60;

    struct __declspec(novtable) GlobalResourceView final
    {
        constexpr GlobalResourceView() : resource_handle { 0 }, rtv { 0 }, rtv_srgb { 0 }, srv { 0 }, srv_srgb { 0 }, state(GlobalResourceState::RESOURCE_UNINITIALIZED), ttl(GLOBAL_RESOURCE_TTL) { }
        constexpr GlobalResourceView(uint64_t handle) : resource_handle{ handle }, rtv{ 0 }, rtv_srgb{ 0 }, srv{ 0 }, srv_srgb{ 0 }, state(GlobalResourceState::RESOURCE_UNINITIALIZED), ttl(GLOBAL_RESOURCE_TTL) { }
        constexpr GlobalResourceView(reshade::api::resource_desc desc) : resource_handle { 0 }, rtv { 0 }, rtv_srgb{ 0 }, srv{ 0 }, srv_srgb{ 0 }, state(GlobalResourceState::RESOURCE_UNINITIALIZED), ttl(GLOBAL_RESOURCE_TTL){ }

        uint64_t resource_handle;
        reshade::api::resource_view rtv;
        reshade::api::resource_view rtv_srgb;
        reshade::api::resource_view srv;
        reshade::api::resource_view srv_srgb;
        GlobalResourceState state;
        uint32_t ttl;
    };

    class __declspec(novtable) ResourceManager final
    {
    public:
        void InitBackbuffer(reshade::api::swapchain* runtime);
        void ClearBackbuffer(reshade::api::swapchain* runtime);

        bool OnCreateResource(reshade::api::device* device, reshade::api::resource_desc& desc, reshade::api::subresource_data* initial_data, reshade::api::resource_usage initial_state);
        void OnInitResource(reshade::api::device* device, const reshade::api::resource_desc& desc, const reshade::api::subresource_data* initData, reshade::api::resource_usage usage, reshade::api::resource handle);
        void OnDestroyResource(reshade::api::device* device, reshade::api::resource res);
        bool OnCreateResourceView(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, reshade::api::resource_view_desc& desc);
        void OnInitResourceView(reshade::api::device* device, reshade::api::resource resource, reshade::api::resource_usage usage_type, const reshade::api::resource_view_desc& desc, reshade::api::resource_view view);
        void OnDestroyResourceView(reshade::api::device* device, reshade::api::resource_view view);
        bool OnCreateSwapchain(reshade::api::swapchain_desc& desc, void* hwnd);
        void OnInitSwapchain(reshade::api::swapchain* swapchain);
        void OnDestroySwapchain(reshade::api::swapchain* swapchain);
        void OnDestroyDevice(reshade::api::device*);

        void SetResourceShim(const std::string& shim) { _shimType = ResolveResourceShimType(shim); }
        void Init();

        void DisposePreview(reshade::api::effect_runtime* runtime);
        void CheckPreview(reshade::api::command_list* cmd_list, reshade::api::device* device, reshade::api::effect_runtime* runtime);
        void SetPingPreviewHandles(reshade::api::resource* res, reshade::api::resource_view* rtv, reshade::api::resource_view* srv);
        void SetPongPreviewHandles(reshade::api::resource* res, reshade::api::resource_view* rtv, reshade::api::resource_view* srv);
        bool IsCompatibleWithPreviewFormat(reshade::api::effect_runtime* runtime, reshade::api::resource res);

        GlobalResourceView& GetResourceView(uint64_t handle);
        void CheckResourceViews(reshade::api::effect_runtime* runtime);

        static EmbeddedResourceData GetResourceData(uint16_t id);

        reshade::api::resource dummy_res;
        reshade::api::resource_view dummy_rtv;
    private:
        void DisposeView(reshade::api::device* device, const GlobalResourceView& views);
        void CreateViews(reshade::api::device* device, GlobalResourceView& gview);
        static ResourceShimType ResolveResourceShimType(const std::string&);

        ResourceShimType _shimType = ResourceShimType::Resource_Shim_None;
        Shim::Resources::ResourceShim* rShim = nullptr;
        bool in_destroy_device = false;

        //std::unordered_map<uint64_t, std::pair<reshade::api::resource_view, reshade::api::resource_view>> s_sRGBResourceViews;
        //std::unordered_map<uint64_t, std::pair<reshade::api::resource_view, reshade::api::resource_view>> s_SRVs;
        //
        //std::unordered_map<uint64_t, uint32_t> _resourceViewRefCount;
        //std::unordered_map<uint64_t, uint64_t> _resourceViewToResource;

        std::shared_mutex resource_mutex;
        std::shared_mutex view_mutex;

        reshade::api::resource preview_res[2];
        reshade::api::resource_view preview_rtv[2];
        reshade::api::resource_view preview_srv[2];

        std::unordered_map<uint64_t, GlobalResourceView> global_resources;
        std::unordered_set<uint64_t> resources;
    };
}