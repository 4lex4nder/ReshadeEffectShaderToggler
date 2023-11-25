#pragma once

#include "AddonUIData.h"
#include "ConstantHandlerBase.h"
#include "ConstantCopyBase.h"
#include "ToggleGroupResourceManager.h"

namespace Shim
{
    namespace Constants
    {
        enum ConstantCopyType
        {
            Copy_None,
            Copy_MemcpySingular,
            Copy_MemcpyNested,
            Copy_FFXIV,
            Copy_NierReplicant,
            Copy_GPUReadback,
        };

        static const std::vector<std::string> ConstantCopyTypeNames = {
            "none",
            "gpu_readback",
            "ffxiv",
            "nier_replicant",
            "memcpy_singular",
            "memcpy_nested"
        };

        enum ConstantHandlerType
        {
            Handler_Default,
        };

        class ConstantManager
        {
        public:
            static bool Init(AddonImGui::AddonUIData& data, Rendering::ToggleGroupResourceManager& gResourceManager, ConstantCopyBase**, ConstantHandlerBase**);
            static bool UnInit();

        private:
            static ConstantCopyType ResolveConstantCopyType(const std::string&);
            static ConstantHandlerType ResolveConstantHandlerType(const std::string&);
        };
    }
}
