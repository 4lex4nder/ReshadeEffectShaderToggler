#pragma once

#include <reshade_api.hpp>
#include <reshade_api_device.hpp>
#include <reshade_api_pipeline.hpp>
#include <unordered_map>
#include "ToggleGroup.h"
#include "ConstantHandlerMemcpy.h"
#include "ConstantCopyMethod.h"

using namespace std;
using namespace reshade::api;
using namespace ShaderToggler;

namespace ConstantFeedback {
    class ConstantCopyMethodSingularMapping : public virtual ConstantCopyMethod
    {
    public:
        ConstantCopyMethodSingularMapping(ConstantHandlerMemcpy* constHandler);
        ~ConstantCopyMethodSingularMapping();

        void OnMapBufferRegion(device* device, resource resource, uint64_t offset, uint64_t size, map_access access, void** data) override;
        void OnUnmapBufferRegion(device* device, resource resource) override;
        void OnMemcpy(void* dest, void* src, size_t size) override;

    private:
        BufferCopy _bufferCopy;
    };
}