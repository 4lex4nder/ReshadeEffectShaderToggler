#pragma once

#include <reshade.hpp>
#include <vector>
#include <unordered_map>
#include <array>
#include <shared_mutex>
#include <functional>
#include "PipelinePrivateData.h"

namespace Rendering
{
    class __declspec(novtable) ToggleGroupResourceManager final
    {
    public:
        void DisposeGroupBuffers(reshade::api::effect_runtime* runtime, std::unordered_map<int, ShaderToggler::ToggleGroup>& groups);
        void CheckGroupBuffers(reshade::api::effect_runtime* runtime, std::unordered_map<int, ShaderToggler::ToggleGroup>& groups);
        void SetGroupBufferHandles(ShaderToggler::ToggleGroup* group, const ShaderToggler::GroupResourceType type, reshade::api::resource* res, reshade::api::resource_view* rtv, reshade::api::resource_view* rtv_srgb, reshade::api::resource_view* srv);
        bool IsCompatibleWithGroupFormat(reshade::api::device* device, const ShaderToggler::GroupResourceType type, reshade::api::resource res, ShaderToggler::ToggleGroup* group);

        void ToggleGroupRemoved(reshade::api::effect_runtime*, ShaderToggler::ToggleGroup*);
    private:
        void DisposeGroupResources(reshade::api::effect_runtime* runtime, reshade::api::resource& res, reshade::api::resource_view& rtv, reshade::api::resource_view& rtv_srgb, reshade::api::resource_view& srv);
    };
}