#pragma once

#include <reshade_api.hpp>
#include <reshade_api_device.hpp>
#include <reshade_api_pipeline.hpp>
#include <unordered_map>
#include "ToggleGroup.h"

using namespace std;
using namespace reshade::api;
using namespace ShaderToggler;

//using var_mapping = unordered_set<tuple<

namespace ConstantFeedback {
	enum class constant_type
	{
		type_unknown = 0,
		type_float,
		type_float2,
		type_float3,
		type_float4,
		type_float3x3,
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
		"float4x4",	// float4x4
		"int",	// int
		"uint"	// uint
	};

	class ConstantHandler {
	public:
		ConstantHandler();
		~ConstantHandler();

		void SetBufferRange(const ToggleGroup* group, buffer_range range, device* dev, command_list* cmd_list, command_queue* queue);
		void RemoveGroup(const ToggleGroup*, device* dev, command_queue* queue);
		const uint8_t* GetConstantBuffer(const ToggleGroup* group);
		size_t GetConstantBufferSize(const ToggleGroup* group);
		void ApplyConstantValues(effect_runtime* runtime, const ToggleGroup*, unordered_map<string, tuple<constant_type, vector<effect_uniform_variable>>>& constants);
	private:
		unordered_map<const ToggleGroup*, vector<uint8_t>> groupBufferContent;
		unordered_map<const ToggleGroup*, resource> groupBufferResourceScratchpad;
		unordered_map<const ToggleGroup*, buffer_range> groupBufferRanges;
		unordered_map<const ToggleGroup*, unordered_map<string, uintptr_t>> groupVarMapping;
		unordered_map<const ToggleGroup*, size_t> groupBufferSize;

		void DestroyScratchpad(const ToggleGroup* group, device* dev, command_queue* queue);
		bool CreateScratchpad(const ToggleGroup* group, device* dev, resource_desc& target);
		void CopyToScratchpad(const ToggleGroup* group, device* dev, command_list* cmd_list, command_queue* queue);
	};
}