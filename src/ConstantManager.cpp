#include "ConstantManager.h"
#include "ConstantHandlerFFXIV.h"
#include "ConstantHandlerNestedMapping.h"
#include "ConstantHandlerSingularMapping.h"
#include "ConstantCopyMemcpy.h"
#include "ConstantCopyFFXIV.h"

ConstantCopyType ConstantManager::ResolveConstantCopyType(const string& ctype)
{
    if (ctype == "none")
        return ConstantCopyType::Copy_None;
    else if (ctype == "memcpy")
        return ConstantCopyType::Copy_Memcpy;
    else if (ctype == "ffxiv")
        return ConstantCopyType::Copy_FFXIV;
    
    return ConstantCopyType::Copy_None;
}

ConstantHandlerType ConstantManager::ResolveConstantHandlerType(const string& htype)
{
    if (htype == "singular")
        return ConstantHandlerType::Handler_Singular;
    else if (htype == "nested")
        return ConstantHandlerType::Handler_Nested;
    else if (htype == "ffxiv")
        return ConstantHandlerType::Handler_FFXIV;

    return ConstantHandlerType::Handler_Nested;
}

bool ConstantManager::Init(AddonImGui::AddonUIData& data, ConstantFeedback::ConstantCopyBase** constantCopy, ConstantFeedback::ConstantHandlerBase** constantHandler)
{
    // Initialize MinHook.
    if (MH_Initialize() != MH_OK)
    {
        return false;
    }

    const string& hookType = data.GetConstHookType();
    const string& hookCopyType = data.GetConstHookCopyType();

    switch (ResolveConstantCopyType(hookType))
    {
    case ConstantCopyType::Copy_None:
    {
        *constantCopy = nullptr;
    }
        break;
    case ConstantCopyType::Copy_Memcpy:
    {
        static ConstantCopyMemcpy constantTypeMemcpy;
        *constantCopy = &constantTypeMemcpy;
    }
        break;
    case ConstantCopyType::Copy_FFXIV:
    {
        static ConstantCopyFFXIV constantTypeFFXIV;
        *constantCopy = &constantTypeFFXIV;
    }
        break;
    default:
        *constantCopy = nullptr;
    }

    if (*constantCopy != nullptr && (*constantCopy)->Init())
    {
        switch (ResolveConstantHandlerType(hookCopyType))
        {
        case ConstantHandlerType::Handler_Singular:
        {
            static ConstantHandlerSingularMapping constantUnnestedMap;
            *constantHandler = &constantUnnestedMap;
        }
            break;
        case ConstantHandlerType::Handler_Nested:
        {
            static ConstantHandlerNestedMapping constantNestedMap;
            *constantHandler = &constantNestedMap;
        }
            break;
        case ConstantHandlerType::Handler_FFXIV:
        {
            static ConstantHandlerFFXIV constantHandlerFFXIV;
            *constantHandler = &constantHandlerFFXIV;
        }
            break;
        default:
        {
            static ConstantHandlerNestedMapping constantNestedMap;
            *constantHandler = &constantNestedMap;
        }
        }

        ConstantCopyBase::SetConstantHandler(*constantHandler);
        data.SetConstantHandler(*constantHandler);

        return true;
    }

    return false;
}

bool ConstantManager::UnInit()
{
    if (MH_Uninitialize() != MH_OK)
    {
        return false;
    }

    return true;
}