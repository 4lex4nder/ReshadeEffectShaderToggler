#pragma once

#include <windows.h>
#include <stdio.h>
#include <reshade_api.hpp>
#include <reshade_api_device.hpp>
#include <reshade_api_pipeline.hpp>
#include <unordered_map>
#include "ToggleGroup.h"
#include "ConstantHandlerBase.h"

using namespace std;
using namespace reshade::api;
using namespace ShaderToggler;

namespace ConstantFeedback {
    class ConstantHandlerFFXIV : public virtual ConstantHandlerBase
    {
    public:
        ConstantHandlerFFXIV();
        ~ConstantHandlerFFXIV();

        static void* Origin;
        static size_t Size;

        void OnMapBufferRegion(device* device, resource resource, uint64_t offset, uint64_t size, map_access access, void** data) override;
        void OnUnmapBufferRegion(device* device, resource resource) override;
        void OnMemcpy(void* dest, void* src, size_t size) override { }
    };
}