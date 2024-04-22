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

static const sigmatch::signature ffxiv_cbload0 = "48 89 5C 24 ?? 55 56 57 48 83 EC 50 49 8B 29"_sig;
static const sigmatch::signature ffxiv_cbload1 = "48 89 5C 24 ?? 56 41 56 41 57 48 83 EC 40 49 8B 18"_sig;
static const sigmatch::signature ffxiv_memcpy = "48 8B C1 4C 8D 15 ?? ?? ?? ??"_sig;

struct ID3D11DeviceContext;
struct ID3D11Resource;
struct D3D11_MAPPED_SUBRESOURCE;

struct astruct_2
{
    ID3D11Resource* resource;
    uint32_t RowPitch;
    uint32_t DepthPitch;
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
            static std::vector<std::tuple<const void*, uint64_t, size_t, bool>> _hostResourceBuffer;
            static std::unordered_map<uint64_t, uint64_t> _hostResourceBufferMap;
            static sig_ffxiv_cbload0* org_ffxiv_cbload0;
            static sig_ffxiv_cbload1* org_ffxiv_cbload1;
            static sig_ffxiv_memcpy* org_ffxiv_memcpy;
           

            static void detour_ffxiv_cbload0(uint64_t param_1, uint16_t* param_2, uint64_t param_3, D3D11_MAPPED_SUBRESOURCE* param_4);
            static uint64_t __fastcall detour_ffxiv_cbload1(uintptr_t param_1, ID3D11DeviceContext* param_2, D3D11_MAPPED_SUBRESOURCE* param_3, ID3D11Resource** param_4, uint64_t index);
            static void __fastcall detour_ffxiv_memcpy(void* param_1, void* param_2, uintptr_t param_3);

            static inline void set_host_resource_data_location(void* origin, size_t len, int64_t resource_handle, size_t index);
        };
    }
}