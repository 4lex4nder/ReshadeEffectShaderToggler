#pragma once

#include <reshade_api.hpp>
#include <reshade_api_device.hpp>
#include <reshade_api_pipeline.hpp>
#include <unordered_map>
#include <functional>
#include <shared_mutex>
#include "ToggleGroup.h"
#include "ShaderManager.h"
#include "ConstantCopyBase.h"

struct CommandListDataContainer;
struct DeviceDataContainer;

namespace Shim
{
    namespace Constants
    {
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

        class __declspec(novtable) ConstantHandlerBase final {
        public:
            ConstantHandlerBase();
            ~ConstantHandlerBase();

            void SetBufferRange(ShaderToggler::ToggleGroup* group, reshade::api::buffer_range range, reshade::api::device * dev, reshade::api::command_list* cmd_list);
            void SetConstants(const ShaderToggler::ToggleGroup* group, const std::vector<uint32_t>& buf, reshade::api::device* dev, reshade::api::command_list* cmd_list);
            std::shared_mutex& GetBufferMutex() { return groupBufferMutex; }
            void RemoveGroup(const ShaderToggler::ToggleGroup*, reshade::api::device* dev);
            const uint8_t* GetConstantBuffer(const ShaderToggler::ToggleGroup* group);
            size_t GetConstantBufferSize(const ShaderToggler::ToggleGroup* group);
            void ReloadConstantVariables(reshade::api::effect_runtime* runtime);
            void UpdateConstants(reshade::api::command_list* cmd_list);
            void ClearConstantVariables();
            void ApplyConstantValues(reshade::api::effect_runtime* runtime, const ShaderToggler::ToggleGroup*, const std::unordered_map<std::string, std::tuple<constant_type, std::vector<reshade::api::effect_uniform_variable>>>& constants);

            void OnReshadeReloadedEffects(reshade::api::effect_runtime* runtime, int32_t enabledCount);
            void OnReshadeSetTechniqueState(reshade::api::effect_runtime* runtime, int32_t enabledCount);

            std::unordered_map<std::string, std::tuple<constant_type, std::vector<reshade::api::effect_uniform_variable>>>* GetRESTVariables();

            static void SetConstantCopy(ConstantCopyBase* constantHandler);
        private:
            std::unordered_map<const ShaderToggler::ToggleGroup*, std::vector<uint8_t>> groupBufferContent;
            std::unordered_map<const ShaderToggler::ToggleGroup*, std::vector<uint8_t>> groupPrevBufferContent;
            std::unordered_map<const ShaderToggler::ToggleGroup*, size_t> groupBufferSize;
            int32_t previousEnableCount = std::numeric_limits<int32_t>::max();
            std::shared_mutex varMutex;
            static std::shared_mutex groupBufferMutex;

            static std::unordered_map<std::string, std::tuple<constant_type, std::vector<reshade::api::effect_uniform_variable>>> restVariables;
            static char charBuffer[CHAR_BUFFER_SIZE];

            static ConstantCopyBase* _constCopy;

            void InitBuffers(const ShaderToggler::ToggleGroup* group, size_t size);
            bool UpdateConstantEntries(reshade::api::command_list* cmd_list, CommandListDataContainer& cmdData, DeviceDataContainer& devData, ShaderToggler::ToggleGroup* group, uint32_t index);
            bool UpdateConstantBufferEntries(reshade::api::command_list* cmd_list, CommandListDataContainer& cmdData, DeviceDataContainer& devData, ShaderToggler::ToggleGroup* group, uint32_t index);
        };
    }
}