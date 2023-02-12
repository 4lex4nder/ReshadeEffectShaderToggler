#include "ConstantHandlerNestedMapping.h"

using namespace ConstantFeedback;

ConstantHandlerNestedMapping::ConstantHandlerNestedMapping() : ConstantHandlerBase()
{

}

ConstantHandlerNestedMapping::~ConstantHandlerNestedMapping()
{

}

void ConstantHandlerNestedMapping::OnMapBufferRegion(device* device, resource resource, uint64_t offset, uint64_t size, map_access access, void** data)
{
    if (access == map_access::write_discard || access == map_access::write_only)
    {
        resource_desc desc = device->get_resource_desc(resource);
        if (desc.heap == memory_heap::cpu_to_gpu && static_cast<uint32_t>(desc.usage & resource_usage::constant_buffer))
        {
            std::unique_lock<shared_mutex> lock(_map_mutex);
            _resourceMemoryMapping[resource.handle] = BufferCopy { resource.handle, *data, nullptr, offset, size, desc.buffer.size };
        }
    }
}

void ConstantHandlerNestedMapping::OnUnmapBufferRegion(device* device, resource resource)
{

    resource_desc desc = device->get_resource_desc(resource);
    if (desc.heap == memory_heap::cpu_to_gpu && static_cast<uint32_t>(desc.usage & resource_usage::constant_buffer))
    {
        std::unique_lock<shared_mutex> lock(_map_mutex);
        _resourceMemoryMapping.erase(resource.handle);
    }
}

void ConstantHandlerNestedMapping::OnMemcpy(void *volatile dest, void* src, size_t size)
{
    std::shared_lock<shared_mutex> lock(_map_mutex);
    if (_resourceMemoryMapping.size() > 0)
    {
        for (auto& mapping : _resourceMemoryMapping)
        {
            if(dest >= mapping.second.destination && static_cast<uintptr_t>(reinterpret_cast<intptr_t>(dest)) <= reinterpret_cast<intptr_t>(mapping.second.destination) + mapping.second.bufferSize - mapping.second.offset)
            {
                SetHostConstantBuffer(mapping.second.resource, src, size, reinterpret_cast<intptr_t>(dest) - reinterpret_cast<intptr_t>(mapping.second.destination), mapping.second.bufferSize);
                break;
            }
        }
    }
}