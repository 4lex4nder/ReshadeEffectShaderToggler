#pragma once

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
    class ConstantHandlerNestedMapping : public virtual ConstantHandlerBase
    {
    public:
        ConstantHandlerNestedMapping();
        ~ConstantHandlerNestedMapping();

        void OnMapBufferRegion(device* device, resource resource, uint64_t offset, uint64_t size, map_access access, void** data) override;
        void OnUnmapBufferRegion(device* device, resource resource) override;
        void OnMemcpy(void* dest, void* src, size_t size) override;

    private:
        unordered_map<uint64_t, BufferCopy> _resourceMemoryMapping;
        std::shared_mutex _map_mutex;
    };
}
