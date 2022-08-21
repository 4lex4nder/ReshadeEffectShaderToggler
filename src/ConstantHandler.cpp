#include <cstring>
#include "ConstantHandler.h"

using namespace ConstantFeedback;

ConstantHandler::ConstantHandler()
{
}

ConstantHandler::~ConstantHandler()
{
}

void ConstantHandler::SetBufferRange(const ToggleGroup* group, buffer_range range, device* dev, command_list* cmd_list, command_queue* queue)
{
	if (dev == nullptr || cmd_list == nullptr || range.buffer == 0)
	{
		return;
	}

	if (!groupBufferContent.contains(group))
	{
		groupBufferRanges.emplace(group, range);
		groupBufferResourceScratchpad.emplace(group, resource{ 0 });
		groupBufferSize.emplace(group, 0);
	}
	else
	{
		groupBufferRanges[group] = range;
	}

	CopyToScratchpad(group, dev, cmd_list, queue);
}

void ConstantHandler::CopyToScratchpad(const ToggleGroup* group, device* dev, command_list* cmd_list, command_queue* queue)
{
	resource bufferResource = { 0 };
	buffer_range currentBufferRange = groupBufferRanges[group];
	resource_desc targetBufferDesc = dev->get_resource_desc(currentBufferRange.buffer);

	if (CreateScratchpad(group, dev, targetBufferDesc))
	{
		bufferResource = groupBufferResourceScratchpad[group];
		resource_desc desc = dev->get_resource_desc(bufferResource);

		if (desc.buffer.size != targetBufferDesc.buffer.size)
		{
			DestroyScratchpad(group, dev, queue);

			if (!CreateScratchpad(group, dev, targetBufferDesc))
			{
				return;
			}

			bufferResource = groupBufferResourceScratchpad[group];
		}
	}
	else
	{
		return;
	}

	vector<uint8_t>& bufferContent = groupBufferContent[group];

	cmd_list->copy_resource(currentBufferRange.buffer, bufferResource);

	void* mapped_buffer;
	uint64_t size = targetBufferDesc.buffer.size;
	if (dev->map_buffer_region(bufferResource, currentBufferRange.offset, currentBufferRange.size, map_access::read_only, &mapped_buffer))
	{
		std::memcpy(bufferContent.data(), mapped_buffer, size);

		dev->unmap_buffer_region(bufferResource);
	}
}

void ConstantHandler::RemoveGroup(const ToggleGroup* group, device* dev, command_queue* queue)
{
	if (!groupBufferContent.contains(group))
	{
		return;
	}
	
	DestroyScratchpad(group, dev, queue);
	
	groupBufferRanges.erase(group);
	groupBufferResourceScratchpad.erase(group);
	groupBufferContent.erase(group);
}

void ConstantHandler::DestroyScratchpad(const ToggleGroup* group, device* dev, command_queue* queue)
{
	if (dev == nullptr || queue == nullptr || !groupBufferResourceScratchpad.contains(group))
	{
		return;
	}

	resource res = groupBufferResourceScratchpad[group];

	if (res != 0)
	{
		queue->wait_idle();
		dev->destroy_resource(res);
		groupBufferResourceScratchpad[group] = { 0 };
		groupBufferContent[group].clear();
	}
}

bool ConstantHandler::CreateScratchpad(const ToggleGroup* group, device* dev, resource_desc& targetBufferDesc)
{
	resource bufferResource = groupBufferResourceScratchpad[group];

	if (bufferResource == 0)
	{
		memory_heap heap = targetBufferDesc.heap == memory_heap::cpu_only ? memory_heap::cpu_only : memory_heap::gpu_to_cpu;

		if (!dev->create_resource(
			resource_desc(targetBufferDesc.buffer.size, heap, resource_usage::copy_dest | resource_usage::copy_source),
			nullptr, resource_usage::copy_dest, &bufferResource))
		{
			return false;
		}

		groupBufferSize[group] = targetBufferDesc.buffer.size;
		groupBufferResourceScratchpad[group] = bufferResource;
		if(groupBufferContent.contains(group))
			groupBufferContent[group].resize(targetBufferDesc.buffer.size, 0);
		else
			groupBufferContent.emplace(group, vector<uint8_t>(targetBufferDesc.buffer.size, 0));
		return true;
	}

	return true;
}

size_t ConstantHandler::GetConstantBufferSize(const ToggleGroup* group)
{
	if (groupBufferSize.contains(group))
	{
		return groupBufferSize[group];
	}

	return 0;
}

const uint8_t* ConstantHandler::GetConstantBuffer(const ToggleGroup* group)
{
	if (groupBufferContent.contains(group))
	{
		return groupBufferContent[group].data();
	}

	return nullptr;
}

void ConstantHandler::ApplyConstantValues(effect_runtime* runtime, const ToggleGroup* group,
	unordered_map<string, tuple<constant_type, vector<effect_uniform_variable>>>& constants)
{
	if (!groupBufferContent.contains(group) || runtime == nullptr)
	{
		return;
	}

	const uint8_t* buffer = groupBufferContent[group].data();

	for (const auto& vars : group->GetVarOffsetMapping())
	{
		string var = vars.first;
		uintptr_t offset = vars.second;

		if (!constants.contains(var))
		{
			continue;
		}

		constant_type type = std::get<0>(constants[var]);
		uint32_t typeIndex = static_cast<uint32_t>(type);

		if (offset + type_size[typeIndex] * type_length[typeIndex] >= groupBufferSize[group])
		{
			continue;
		}

		const vector<effect_uniform_variable>& effect_variables = std::get<1>(constants[var]);

		for (const auto& effect_var : effect_variables)
		{
			if (type <= constant_type::type_float4x4)
			{
				runtime->set_uniform_value_float(effect_var, reinterpret_cast<const float*>(buffer + offset), type_length[typeIndex], 0);
			}
			else if (type == constant_type::type_int)
			{
				runtime->set_uniform_value_int(effect_var, reinterpret_cast<const int32_t*>(buffer + offset), type_length[typeIndex], 0);
			}
			else
			{
				runtime->set_uniform_value_uint(effect_var, reinterpret_cast<const uint32_t*>(buffer + offset), type_length[typeIndex], 0);
			}
		}
	}
}
