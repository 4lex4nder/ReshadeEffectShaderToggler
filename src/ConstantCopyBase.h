#pragma once

#include <reshade_api.hpp>
#include <reshade_api_device.hpp>
#include <reshade_api_pipeline.hpp>
#include <unordered_map>
#include <shared_mutex>
#include "ToggleGroup.h"

namespace Shim
{
    namespace Constants
    {
        class ConstantCopyBase {
        public:
            ConstantCopyBase();
            ~ConstantCopyBase();

            virtual bool Init() = 0;
            virtual bool UnInit() = 0;

            virtual void GetHostConstantBuffer(reshade::api::command_list* cmd_list, ShaderToggler::ToggleGroup* group, std::vector<uint8_t>& dest, size_t size, uint64_t resourceHandle);
            virtual void CreateHostConstantBuffer(reshade::api::device* dev, reshade::api::resource resource, size_t size);
            virtual void DeleteHostConstantBuffer(reshade::api::resource resource);
            virtual inline void SetHostConstantBuffer(const uint64_t handle, const void* buffer, size_t size, uintptr_t offset, uint64_t bufferSize);

            virtual void OnInitResource(reshade::api::device* device, const reshade::api::resource_desc& desc, const reshade::api::subresource_data* initData, reshade::api::resource_usage usage, reshade::api::resource handle);
            virtual void OnDestroyResource(reshade::api::device* device, reshade::api::resource res);

            virtual void OnUpdateBufferRegion(reshade::api::device* device, const void* data, reshade::api::resource resource, uint64_t offset, uint64_t size) = 0;
            virtual void OnMapBufferRegion(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data) = 0;
            virtual void OnUnmapBufferRegion(reshade::api::device* device, reshade::api::resource resource) = 0;
        protected:
            static std::unordered_map<uint64_t, std::vector<uint8_t>> deviceToHostConstantBuffer;
            static std::shared_mutex deviceHostMutex;
        };
    }
}
