#include "ConstantHandlerSingularMapping.h"

using namespace ConstantFeedback;

BufferCopy ConstantHandlerSingularMapping::_bufferCopy;

ConstantHandlerSingularMapping::ConstantHandlerSingularMapping() : ConstantHandlerBase()
{

}

ConstantHandlerSingularMapping::~ConstantHandlerSingularMapping()
{

}

void ConstantHandlerSingularMapping::OnMapBufferRegion(device* device, resource resource, uint64_t offset, uint64_t size, map_access access, void** data)
{
    if ((access == map_access::write_discard || access == map_access::write_only))
    {
        resource_desc desc = device->get_resource_desc(resource);
        uint8_t* buf = const_cast<uint8_t*>(GetHostConstantBuffer(resource.handle));

        if(buf != nullptr)
        {
            _bufferCopy.resource = resource.handle;
            _bufferCopy.destination = *data;
            _bufferCopy.size = size;
            _bufferCopy.offset = offset;
            _bufferCopy.bufferSize = desc.buffer.size;
            _bufferCopy.hostDestination = buf;
        }
    }
}

void ConstantHandlerSingularMapping::OnUnmapBufferRegion(device* device, resource resource)
{
    _bufferCopy.resource = 0;
    _bufferCopy.destination = nullptr;
}

void ConstantHandlerSingularMapping::OnMemcpy(void* dest, void* src, size_t size)
{
    uintptr_t destPtr = reinterpret_cast<uintptr_t>(dest);
    uintptr_t destinationPtr = reinterpret_cast<uintptr_t>(_bufferCopy.destination);
    
    if (_bufferCopy.resource != 0 &&
        destPtr >= destinationPtr &&
        destPtr <= destinationPtr + _bufferCopy.bufferSize - _bufferCopy.offset)
    {
        memcpy(_bufferCopy.hostDestination, src, size);
    }
}