#include <cstring>
#include <MinHook.h>
#include <reshade.hpp>
#include "ConstantCopyMemcpy.h"
#include "PipelinePrivateData.h"

using namespace ConstantFeedback;

sig_memcpy* ConstantCopyMemcpy::org_memcpy = nullptr;

ConstantCopyMemcpy::ConstantCopyMemcpy()
{

}

ConstantCopyMemcpy::~ConstantCopyMemcpy()
{
}

bool ConstantCopyMemcpy::Init()
{
    return Hook(&org_memcpy, detour_memcpy);
}

bool ConstantCopyMemcpy::UnInit()
{
    return MH_Uninitialize() == MH_OK;
}


bool ConstantCopyMemcpy::HookStatic(sig_memcpy** original, sig_memcpy* detour)
{
    string exe = GetExecutableName();
    if (exe.length() == 0)
        return false;

    for (const auto& sig : memcpy_static)
    {
        sigmatch::this_process_target target;
        sigmatch::search_result result = target.in_module(exe).search(sig);

        for (const std::byte* address : result.matches()) {
            void* adr = static_cast<void*>(const_cast<std::byte*>(address));
            *original = InstallHook<sig_memcpy>(adr, detour);

            // Assume signature is unique
            if (*original != nullptr)
            {
                return true;
            }
        }
    }

    return false;
}

bool ConstantCopyMemcpy::HookDynamic(sig_memcpy** original, sig_memcpy* detour)
{
    for (const auto& libFunc : memcpy_dynamic)
    {
        *original = InstallApiHook(get<0>(libFunc).c_str(), get<1>(libFunc).c_str(), detour);

        // Pick first hit and hope for the best
        if (*original != nullptr)
        {
            return true;
        }
    }

    return false;
}

bool ConstantCopyMemcpy::Hook(sig_memcpy** original, sig_memcpy* detour)
{
    // Try hooking statically linked memcpy first, then look into dynamically linked ones
    if (HookStatic(original, detour) || HookDynamic(original, detour))
    {
        return !(MH_EnableHook(MH_ALL_HOOKS) != MH_OK);
    }

    return false;
}

bool ConstantCopyMemcpy::Unhook()
{
    if (MH_DisableHook(MH_ALL_HOOKS) != MH_OK)
    {
        return false;
    }

    return true;
}

void* __fastcall ConstantCopyMemcpy::detour_memcpy(void* dest, void* src, size_t size)
{
    if (_constHandler != nullptr)
        _constHandler->OnMemcpy(dest, src, size);

    return org_memcpy(dest, src, size);
}