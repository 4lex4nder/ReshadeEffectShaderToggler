#include <cstring>
#include <MinHook.h>
#include <reshade.hpp>
#include "ConstantCopyFFXIV.h"
#include "PipelinePrivateData.h"

using namespace ConstantFeedback;

sig_ffxiv_cbload* ConstantCopyFFXIV::org_ffxiv_cbload = nullptr;

ConstantCopyFFXIV::ConstantCopyFFXIV()
{
}

ConstantCopyFFXIV::~ConstantCopyFFXIV()
{
}

bool ConstantCopyFFXIV::Init()
{
    return Hook(&org_ffxiv_cbload, detour_ffxiv_cbload);
}

bool ConstantCopyFFXIV::UnInit()
{
    return MH_Uninitialize() == MH_OK;
}

bool ConstantCopyFFXIV::Hook(sig_ffxiv_cbload** original, sig_ffxiv_cbload* detour)
{
    string exe = GetExecutableName();
    if (exe.length() == 0)
        return false;

    sigmatch::this_process_target target;
    sigmatch::search_result result = target.in_module(exe).search(ffxiv_cbload);

    for (const std::byte* address : result.matches()) {
        void* adr = static_cast<void*>(const_cast<std::byte*>(address));
        *original = InstallHook<sig_ffxiv_cbload>(adr, detour);
    }

    if (original != nullptr)
        return MH_EnableHook(MH_ALL_HOOKS) == MH_OK;

    return false;
}

bool ConstantCopyFFXIV::Unhook()
{
    if (MH_DisableHook(MH_ALL_HOOKS) != MH_OK)
    {
        return false;
    }

    return true;
}

void __fastcall ConstantCopyFFXIV::detour_ffxiv_cbload(
    int64_t p1,
    uint16_t* p2,
    uint64_t p3,
    intptr_t** p4
)
{
    ConstantHandlerFFXIV::Origin = *p4;
    ConstantHandlerFFXIV::Size = (reinterpret_cast<uint64_t>(p4[1]) & 0xffffffff) << 4;

    org_ffxiv_cbload(p1, p2, p3, p4);

    ConstantHandlerFFXIV::Origin = nullptr;
    ConstantHandlerFFXIV::Size = 0;
}