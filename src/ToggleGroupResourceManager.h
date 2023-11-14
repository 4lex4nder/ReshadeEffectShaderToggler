#pragma once

#include <reshade.hpp>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <functional>
#include "PipelinePrivateData.h"

namespace Rendering
{
    class __declspec(novtable) ToggleGroupResourceManager final
    {
    public:
        void DisposeGroupBuffers(reshade::api::effect_runtime* runtime);
        void CheckGroupBuffers(reshade::api::command_list* cmd_list, reshade::api::device* device, reshade::api::effect_runtime* runtime);
        void SetGroupBufferHandles(ShaderToggler::ToggleGroup* group, reshade::api::resource* res, reshade::api::resource_view* rtv, reshade::api::resource_view* rtv_srgb, reshade::api::resource_view* srv);
        bool IsCompatibleWithGroupFormat(reshade::api::effect_runtime* runtime, reshade::api::resource res, ShaderToggler::ToggleGroup* group);

        void ToggleGroupRemoved(reshade::api::effect_runtime*, ShaderToggler::ToggleGroup*);
    private:
        void DisposeGroupResources(reshade::api::effect_runtime* runtime, reshade::api::resource& res, reshade::api::resource_view& rtv, reshade::api::resource_view& rtv_srgb, reshade::api::resource_view& srv);

        std::unordered_map<ShaderToggler::ToggleGroup*, std::tuple<reshade::api::resource, reshade::api::resource_view, reshade::api::resource_view, reshade::api::resource_view>> group_buffers;
    };
}