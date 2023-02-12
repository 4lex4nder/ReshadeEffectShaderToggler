#include "ConstantHandlerFFXIV.h"

using namespace ConstantFeedback;

void* ConstantHandlerFFXIV::Origin = nullptr;
size_t ConstantHandlerFFXIV::Size = 0;

ConstantHandlerFFXIV::ConstantHandlerFFXIV() : ConstantHandlerBase()
{

}

ConstantHandlerFFXIV::~ConstantHandlerFFXIV()
{

}

void ConstantHandlerFFXIV::OnMapBufferRegion(device* device, resource resource, uint64_t offset, uint64_t size, map_access access, void** data)
{
    if (Origin != nullptr && (access == map_access::write_discard || access == map_access::write_only))
    {
        resource_desc desc = device->get_resource_desc(resource);
        uint8_t* buf = const_cast<uint8_t*>(GetHostConstantBuffer(resource.handle));

        if (buf != nullptr)
        {
            memmove(buf, Origin, Size);
        }
    }
}

void ConstantHandlerFFXIV::OnUnmapBufferRegion(device* device, resource resource)
{
}
