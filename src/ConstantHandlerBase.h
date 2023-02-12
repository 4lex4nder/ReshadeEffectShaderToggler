#pragma once

#include <reshade_api.hpp>
#include <reshade_api_device.hpp>
#include <reshade_api_pipeline.hpp>
#include <unordered_map>
#include <functional>
#include <shared_mutex>
#include "ToggleGroup.h"
#include "ShaderManager.h"

using namespace std;
using namespace reshade::api;
using namespace ShaderToggler;

namespace ConstantFeedback {
    struct BufferCopy
    {
        uint64_t resource = 0;
        void* destination = nullptr;
        uint8_t* hostDestination = nullptr;
        uint64_t offset = 0;
        uint64_t size = 0;
        uint64_t bufferSize = 0;
    };

    enum class constant_type
    {
        type_unknown = 0,
        type_float,
        type_float2,
        type_float3,
        type_float4,
        type_float3x3,
        type_float4x3,
        type_float4x4,
        type_int,
        type_uint
    };

    static constexpr size_t type_size[] =
    {
        0,	// dummy
        4,	// float
        4,	// float2
        4,	// float3
        4,  // float4
        4,	// float3x3
        4,  // float4x3
        4,	// float4x4
        4,	// int
        4	// uint
    };

    static constexpr size_t type_length[] =
    {
        0,	// dummy
        1,	// float
        2,	// float2
        3,	// float3
        4,	// float4
        9,	// float3x3
        12, // float4x3
        16,	// float4x4
        1,	// int
        1	// uint
    };

    static constexpr const char* type_desc[] =
    {
        "",	// dummy
        "float",	// float
        "float2",	// float2
        "float3",	// float3
        "float4",	// float4
        "float3x3",	// float3x3
        "float4x3", // float4x3
        "float4x4",	// float4x4
        "int",	// int
        "uint"	// uint
    };

    static constexpr size_t CHAR_BUFFER_SIZE = 256;

    class ConstantHandlerBase {
    public:
        ConstantHandlerBase();
        ~ConstantHandlerBase();

        virtual void SetBufferRange(const ToggleGroup* group, buffer_range range, device* dev, command_list* cmd_list, command_queue* queue);
        virtual void RemoveGroup(const ToggleGroup*, device* dev, command_queue* queue);
        uint8_t* GetConstantBuffer(const ToggleGroup* group);
        size_t GetConstantBufferSize(const ToggleGroup* group);
        static const ToggleGroup* CheckDescriptors(command_list* commandList, ShaderManager& pixelShaderManager, ShaderManager& vertexShaderManager, uint32_t psShaderHash, uint32_t vsShaderHash);
        void ReloadConstantVariables(effect_runtime* runtime);

        void OnPushDescriptors(command_list* cmd_list, shader_stage stages, pipeline_layout layout, uint32_t layout_param, const descriptor_set_update& update,
            ShaderManager& pixelShaderManager, ShaderManager& vertexShaderManager);
        virtual void OnInitResource(device* device, const resource_desc& desc, const subresource_data* initData, resource_usage usage, reshade::api::resource handle);
        virtual void OnDestroyResource(device* device, resource res);

        void ApplyConstantValues(effect_runtime* runtime, const ToggleGroup*, const unordered_map<string, tuple<constant_type, vector<effect_uniform_variable>>>& constants);

        const unordered_set<uint64_t>& GetConstantBuffers();

        unordered_map<string, tuple<constant_type, vector<effect_uniform_variable>>>* GetRESTVariables();

        const uint8_t* GetHostConstantBuffer(uint64_t resourceHandle);
        void CreateHostConstantBuffer(device* dev, resource resource, size_t size);
        void DeleteHostConstantBuffer(resource resource);
        inline void SetHostConstantBuffer(const uint64_t handle, const void* buffer, size_t size, uintptr_t offset, uint64_t bufferSize);

        virtual void OnMapBufferRegion(device* device, resource resource, uint64_t offset, uint64_t size, map_access access, void** data) = 0;
        virtual void OnUnmapBufferRegion(device* device, resource resource) = 0;
        virtual void OnMemcpy(void* dest, void* src, size_t size) = 0;
    protected:
        unordered_map<const ToggleGroup*, vector<uint8_t>> groupBufferContent;
        unordered_map<const ToggleGroup*, vector<uint8_t>> groupPrevBufferContent;
        unordered_map<const ToggleGroup*, size_t> groupBufferSize;

        unordered_set<uint64_t> constantBuffers;
        shared_mutex constbuffer_mutex;
        static unordered_map<string, tuple<constant_type, vector<effect_uniform_variable>>> restVariables;
        static char charBuffer[CHAR_BUFFER_SIZE];

        unordered_map<const ToggleGroup*, buffer_range> groupBufferRanges;
        unordered_map<uint64_t, vector<uint8_t>> deviceToHostConstantBuffer;

        shared_mutex deviceHostMutex;

        bool CreateScratchpad(const ToggleGroup* group, device* dev, resource_desc& target);
        void CopyToScratchpad(const ToggleGroup* group, device* dev, command_list* cmd_list, command_queue* queue);
    };
}