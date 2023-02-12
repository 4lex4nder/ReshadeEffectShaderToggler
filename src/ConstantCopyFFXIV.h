#pragma once

#include <reshade_api.hpp>
#include <reshade_api_device.hpp>
#include <reshade_api_pipeline.hpp>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <sigmatch.hpp>
#include "ConstantCopyDefinitions.h"
#include "ConstantHandlerFFXIV.h"
#include "ToggleGroup.h"
#include "ConstantCopyBase.h"

using namespace sigmatch_literals;

static const sigmatch::signature ffxiv_cbload = "4C 89 44 24 ?? 56 57 41 57"_sig;

namespace ConstantFeedback {
    class ConstantCopyFFXIV : public virtual ConstantCopyBase {
    public:
        ConstantCopyFFXIV();
        ~ConstantCopyFFXIV();

        bool Init();
        bool UnInit();

        bool Hook(sig_ffxiv_cbload** original, sig_ffxiv_cbload* detour);
        bool Unhook();

    private:
        static sig_ffxiv_cbload* org_ffxiv_cbload;
        static void __fastcall detour_ffxiv_cbload(int64_t p1, uint16_t* p2, uint64_t p3, intptr_t** p4);
    };
}