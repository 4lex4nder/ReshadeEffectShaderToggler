#pragma once

#include "AddonUIData.h"
#include "ConstantHandlerBase.h"
#include "ConstantCopyBase.h"

namespace ConstantFeedback
{
    enum ConstantCopyType
    {
        Copy_None,
        Copy_Memcpy,
        Copy_FFXIV //actually only works with ConstantHandlerType::FFXIV, but shhhhh
    };

    enum ConstantHandlerType
    {
        Handler_Singular,
        Handler_Nested,
        Handler_FFXIV
    };

    class ConstantManager
    {
    public:
        static bool Init(AddonImGui::AddonUIData& data, ConstantFeedback::ConstantCopyBase**, ConstantFeedback::ConstantHandlerBase**);
        static bool UnInit();

    private:
        static ConstantCopyType ResolveConstantCopyType(const string&);
        static ConstantHandlerType ResolveConstantHandlerType(const string&);
    };
}
