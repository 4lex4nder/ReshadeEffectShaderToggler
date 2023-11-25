#pragma once

#include <reshade_api.hpp>
#include <reshade_api_device.hpp>
#include <reshade_api_pipeline.hpp>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include "ConstantCopyBase.h"
#include "ToggleGroupResourceManager.h"

namespace Shim
{
    namespace Constants
    {
        class ConstantCopyGPUReadback final : public virtual ConstantCopyBase {
        public:
            ConstantCopyGPUReadback(Rendering::ToggleGroupResourceManager& gResourceManager) : groupResourceManager(gResourceManager) { }

            bool Init() override final { return true; };
            bool UnInit() override final { return true; };

            virtual void GetHostConstantBuffer(reshade::api::command_list* cmd_list, ShaderToggler::ToggleGroup* group, std::vector<uint8_t>& dest, size_t size, uint64_t resourceHandle) override final;
            virtual void CreateHostConstantBuffer(reshade::api::device* dev, reshade::api::resource resource, size_t size) override final {};
            virtual void DeleteHostConstantBuffer(reshade::api::resource resource) override final {};
            virtual void SetHostConstantBuffer(const uint64_t handle, const void* buffer, size_t size, uintptr_t offset, uint64_t bufferSize) override final {};

            virtual void OnInitResource(reshade::api::device* device, const reshade::api::resource_desc& desc, const reshade::api::subresource_data* initData, reshade::api::resource_usage usage, reshade::api::resource handle) override final {};
            virtual void OnDestroyResource(reshade::api::device* device, reshade::api::resource res) override final {};

            virtual void OnUpdateBufferRegion(reshade::api::device* device, const void* data, reshade::api::resource resource, uint64_t offset, uint64_t size) override final {};
            virtual void OnMapBufferRegion(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data) override final {};
            virtual void OnUnmapBufferRegion(reshade::api::device* device, reshade::api::resource resource) override final {};

        private:
            std::unordered_map<uint64_t, reshade::api::resource> resToCopyBuffer;
            Rendering::ToggleGroupResourceManager& groupResourceManager;
        };
    }
}