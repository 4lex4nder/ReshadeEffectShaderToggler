#pragma once

#include <reshade_api.hpp>
#include <reshade_api_device.hpp>
#include <reshade_api_pipeline.hpp>
#include <unordered_map>
#include <MinHook.h>
#include <string>

#pragma warning(push)
#pragma warning(disable : 4005)
#include <sigmatch.hpp>
#pragma warning(pop)

struct ID3D11Resource;
struct ID3D11DeviceContext;
struct D3D11_MAPPED_SUBRESOURCE;

using sig_memcpy = void* (__fastcall)(void*, void*, size_t);
using sig_ffxiv_cbload0 = void(uint64_t param_1, uint16_t* param_2, uint64_t param_3, D3D11_MAPPED_SUBRESOURCE* param_4);
using sig_ffxiv_cbload1 = uint64_t(__fastcall)(uintptr_t param_1, ID3D11DeviceContext* param_2, D3D11_MAPPED_SUBRESOURCE* param_3, ID3D11Resource** param_4);
using sig_ffxiv_memcpy = void(__fastcall)(void* param_1, void* param_2, uintptr_t param_3);
using sig_nier_replicant_cbload = void(__fastcall)(intptr_t p1, intptr_t* p2, uintptr_t p3);
using sig_ffxiv_texture_create = void(__fastcall)(uintptr_t*, uintptr_t*);
using sig_ffxiv_textures_recreate = uintptr_t(__fastcall)(uintptr_t);
using sig_ffxiv_textures_create = uintptr_t(__fastcall)(uintptr_t);

namespace Shim
{
    class GameHook {
    protected:
        static bool _hooked;
    };

    template<typename T>
    class GameHookT : public GameHook {
    public:
        static bool Hook(T** original, T* detour, const sigmatch::signature& sig);
        static bool Unhook();
        static std::string GetExecutableName();
        static T* InstallHook(void* target, T* callback);
        static T* InstallApiHook(LPCWSTR pszModule, LPCSTR pszProcName, T* callback);
    };
}