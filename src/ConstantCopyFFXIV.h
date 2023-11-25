#pragma once

#include <reshade_api.hpp>
#include <reshade_api_device.hpp>
#include <reshade_api_pipeline.hpp>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include "ConstantCopyBase.h"
#include "GameHookT.h"

using namespace sigmatch_literals;

static const sigmatch::signature ffxiv_cbload = "4C 89 44 24 ?? 56 57 41 57"_sig;

struct ID3D11DeviceContext;
struct ID3D11Resource;

struct HostBufferData {
    int64_t* data;
    uint32_t size;
    uint32_t dunno;
};

struct param_2_struct {
    uint16_t something0;
    uint16_t something1;
    uint32_t reserved;
    uint8_t something_else[16];
    ID3D11Resource* res[9];
};

struct param_3_struct {
    uint16_t something2;
    uint16_t something1;
    uint16_t something0;
    uint16_t dunno;
};

struct BufferData {
    ID3D11Resource* res[4];
    int64_t* data[4];
    uint32_t count0;
    uint32_t count1;
    uint32_t count2;
    uint32_t count3;
};

struct TailBufferData {
    ID3D11Resource* res;
    int64_t* data;
};

struct ResourceData {
    uint64_t reserved0;
    ID3D11DeviceContext* deviceContext;
    int64_t reserved1;
    BufferData resources[24];
    TailBufferData tail_resources[4];
};

namespace Shim
{
    namespace Constants
    {
        class ConstantCopyFFXIV final : public virtual ConstantCopyBase {
        public:
            ConstantCopyFFXIV();
            ~ConstantCopyFFXIV();

            bool Init() override final;
            bool UnInit() override final;

            void OnInitResource(reshade::api::device* device, const reshade::api::resource_desc& desc, const reshade::api::subresource_data* initData, reshade::api::resource_usage usage, reshade::api::resource handle) override final {};
            void OnDestroyResource(reshade::api::device* device, reshade::api::resource res) override final {};
            void OnUpdateBufferRegion(reshade::api::device* device, const void* data, reshade::api::resource resource, uint64_t offset, uint64_t size) override final {};
            void OnMapBufferRegion(reshade::api::device* device, reshade::api::resource resource, uint64_t offset, uint64_t size, reshade::api::map_access access, void** data) override final {};
            void OnUnmapBufferRegion(reshade::api::device* device, reshade::api::resource resource) override final {};
            void GetHostConstantBuffer(reshade::api::command_list* cmd_list, ShaderToggler::ToggleGroup* group, std::vector<uint8_t>& dest, size_t size, uint64_t resourceHandle) override final;
        private:
            static std::vector<std::tuple<const void*, uint64_t, size_t>> _hostResourceBuffer;
            static sig_ffxiv_cbload* org_ffxiv_cbload;
            static int64_t __fastcall detour_ffxiv_cbload(ResourceData* param_1, param_2_struct* param_2, param_3_struct param_3, HostBufferData* param_4);
            static inline void set_host_resource_data_location(void* origin, size_t len, int64_t resource_handle, size_t index);
        };
    }
}