#pragma once

#include <reshade_api.hpp>
#include <reshade_api_device.hpp>
#include <reshade_api_pipeline.hpp>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <sigmatch.hpp>
#include "ConstantCopyDefinitions.h"
#include "ToggleGroup.h"
#include "ConstantCopyBase.h"

using namespace std;
using namespace reshade::api;
using namespace ShaderToggler;
using namespace sigmatch_literals;

#if _WIN64
static const vector<sigmatch::signature> memcpy_static = {
    // vcruntime140
    "48 8B C1 4C 8D 15 ?? ?? ?? ?? 49 83 F8 0F"_sig,
    // msvcrt
    "48 8B C1 49 83 F8 08 72 ?? 49 83 F8 10"_sig,
    // msvcr120, msvcr110
    "4C 8B D9 4C 8B D2 49 83 F8 10"_sig,
    // msvcr100
    "4C 8B D9 48 2B D1 ?? ?? ?? ?? ?? ?? 49 83 F8 08 ?? ?? F6 C1 07"_sig
};
#else
static const vector<sigmatch::signature> memcpy_static = {
    // vcruntime140
    "57 56 8B 74 24 ?? 8B 4C 24 ?? 8B 7C 24 ?? 8B C1 8B D1 03 C6 3B FE 76 ??"_sig,
    // msvcrt
    "55 8B EC 57 56 8B 75 ?? 8B 4D ?? 8B 7D ?? 8B C1 8B D1 03 C6 3B FE 76 ?? 3B F8 0F 82 ?? ?? ?? ?? 81 F9 00 01 00 00 72 ?? 83 3D ?? ?? ?? ?? 00 74 ?? 57 56 83 E7 0F 83 E6 0F 3B FE 5E 5F 75 ?? 5E 5F 5D E9 ?? ?? ?? ?? F7 C7 03 00 00 00 75 ?? C1 E9 02 83 E2 03 83 F9 08 72 ?? F3 A5 FF 24 95 ?? ?? ?? ?? 8B C7"_sig,
    // msvcr120, msvcr110
    "57 56 8B 74 24 ?? 8B 4C 24 ?? 8B 7C 24 ?? 8B C1 8B D1 03 C6 3B FE 77 ??"_sig,
    // msvcr100
    "55 8B EC 57 56 8B 75 ?? 8B 4D ?? 8B 7D ?? 8B C1 8B D1 03 C6 3B FE 76 ?? 3B F8 0F 82 ?? ?? ?? ?? 81 F9 80 00 00 00 72 ?? 83 3D ?? ?? ?? ?? 00 74 ?? 57 56 83 E7 0F 83 E6 0F 3B FE 5E 5F 75 ?? E9 ?? ?? ?? ?? F7 C7 03 00 00 00 75 ?? C1 E9 02 83 E2 03 83 F9 08 72 ?? F3 A5 FF 24 95 ?? ?? ?? ?? 8B C7 BA 03 00 00 00 83 E9 04 72 ?? 83 E0 03 03 C8 FF 24 85 ?? ?? ?? ?? FF 24 8D ?? ?? ?? ?? FF 24 8D ?? ?? ?? ??"_sig
};
#endif

static const vector<tuple<wstring, string>> memcpy_dynamic = {
    {L"vcruntime140", "memcpy"},
    {L"msvcrt", "memcpy"},
    {L"msvcr120", "memcpy"},
    {L"msvcr110", "memcpy"},
    {L"msvcr100", "memcpy"}
};

namespace ConstantFeedback {
    class ConstantCopyMemcpy : public virtual ConstantCopyBase {
    public:
        ConstantCopyMemcpy();
        ~ConstantCopyMemcpy();

        bool Init();
        bool UnInit();

        bool Hook(sig_memcpy** original, sig_memcpy* detour);
        bool Unhook();

    private:
        bool HookStatic(sig_memcpy** original, sig_memcpy* detour);
        bool HookDynamic(sig_memcpy** original, sig_memcpy* detour);

        static sig_memcpy* org_memcpy;
        static void* __fastcall detour_memcpy(void* dest, void* src, size_t size);
    };
}