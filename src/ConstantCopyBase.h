#pragma once

#include <reshade_api.hpp>
#include <reshade_api_device.hpp>
#include <reshade_api_pipeline.hpp>
#include <unordered_map>
#include <MinHook.h>
#include "ConstantCopyDefinitions.h"
#include "ToggleGroup.h"
#include "ConstantHandlerBase.h"

namespace ConstantFeedback {
    class ConstantCopyBase {
    public:
        ConstantCopyBase();
        ~ConstantCopyBase();

        virtual bool Init() = 0;
        virtual bool UnInit() = 0;

        static void SetConstantHandler(ConstantHandlerBase* constantHandler);
    protected:
        static ConstantHandlerBase* _constHandler;

        template<typename T>
        static T* InstallHook(void* target, T* callback);
        template<typename T>
        static T* InstallApiHook(LPCWSTR pszModule, LPCSTR pszProcName, T* callback);
        static string GetExecutableName();
    };
}
