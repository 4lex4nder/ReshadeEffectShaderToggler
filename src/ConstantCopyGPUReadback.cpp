#include <cstring>
#include "ConstantCopyGPUReadback.h"
#include "PipelinePrivateData.h"

using namespace Shim::Constants;
using namespace reshade::api;
using namespace ShaderToggler;
using namespace std;


void ConstantCopyGPUReadback::GetHostConstantBuffer(reshade::api::command_list* cmd_list, ShaderToggler::ToggleGroup* group, vector<uint8_t>& dest, size_t size, uint64_t resourceHandle)
{
    resource src = resource{ resourceHandle };
    ShaderToggler::GroupResource& dst = group->GetGroupResource(ShaderToggler::GroupResourceType::RESOURCE_CONSTANTS_COPY);
    if (groupResourceManager.IsCompatibleWithGroupFormat(cmd_list->get_device(), ShaderToggler::GroupResourceType::RESOURCE_CONSTANTS_COPY, src, group))
    {
        void* data = nullptr;
        cmd_list->copy_resource(src, dst.res);
        if (cmd_list->get_device()->map_buffer_region(dst.res, 0, size, map_access::read_only, &data))
        {
            memcpy(dest.data(), data, size);
            cmd_list->get_device()->unmap_buffer_region(dst.res);
        }
    }
    else
    {
        dst.state = ShaderToggler::GroupResourceState::RESOURCE_INVALID;
        dst.target_description = cmd_list->get_device()->get_resource_desc(src);
    }
}