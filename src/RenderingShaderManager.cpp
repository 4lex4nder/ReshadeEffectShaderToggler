#include <d3d12.h>
#include "RenderingShaderManager.h"
#include "StateTracking.h"
#include "resource.h"

using namespace Rendering;
using namespace ShaderToggler;
using namespace reshade::api;
using namespace std;

RenderingShaderManager::RenderingShaderManager(AddonImGui::AddonUIData& data, ResourceManager& rManager) : uiData(data), resourceManager(rManager)
{
}

RenderingShaderManager::~RenderingShaderManager()
{
}

bool RenderingShaderManager::CreatePipeline(reshade::api::device* device, reshade::api::pipeline_layout layout, uint16_t ps_resource_id, uint16_t vs_resource_id, reshade::api::pipeline& sh_pipeline, uint8_t write_mask)
{
    if (sh_pipeline == 0 && (device->get_api() == device_api::d3d9 || device->get_api() == device_api::d3d10 || device->get_api() == device_api::d3d11 || device->get_api() == device_api::d3d12))
    {
        const EmbeddedResourceData vs = resourceManager.GetResourceData(vs_resource_id);
        const EmbeddedResourceData ps = resourceManager.GetResourceData(ps_resource_id);

        shader_desc vs_desc = { vs.data, vs.size };
        shader_desc ps_desc = { ps.data, ps.size };

        std::vector<pipeline_subobject> subobjects;
        subobjects.push_back({ pipeline_subobject_type::vertex_shader, 1, &vs_desc });
        subobjects.push_back({ pipeline_subobject_type::pixel_shader, 1, &ps_desc });

        if (device->get_api() == device_api::d3d9)
        {
            static input_element input_layout[1] = {
                { 0, "TEXCOORD", 0, format::r32g32_float, 0, offsetof(vert_input, uv), sizeof(vert_input), 0},
            };

            subobjects.push_back({ pipeline_subobject_type::input_layout, 1, reinterpret_cast<void*>(input_layout) });
        }

        blend_desc blend_state;
        blend_state.blend_enable[0] = true;
        blend_state.source_color_blend_factor[0] = blend_factor::source_alpha;
        blend_state.dest_color_blend_factor[0] = blend_factor::one_minus_source_alpha;
        blend_state.color_blend_op[0] = blend_op::add;
        blend_state.source_alpha_blend_factor[0] = blend_factor::one;
        blend_state.dest_alpha_blend_factor[0] = blend_factor::one_minus_source_alpha;
        blend_state.alpha_blend_op[0] = blend_op::add;
        blend_state.render_target_write_mask[0] = write_mask;

        subobjects.push_back({ pipeline_subobject_type::blend_state, 1, &blend_state });

        rasterizer_desc rasterizer_state;
        rasterizer_state.cull_mode = cull_mode::none;
        rasterizer_state.scissor_enable = true;

        subobjects.push_back({ pipeline_subobject_type::rasterizer_state, 1, &rasterizer_state });

        if (!device->create_pipeline(layout, static_cast<uint32_t>(subobjects.size()), subobjects.data(), &sh_pipeline))
        {
            sh_pipeline = {};
            reshade::log_message(reshade::log_level::warning, "Unable to create pipeline");

            return false;
        }

        return true;
    }

    return false;
}

void RenderingShaderManager::InitShader(reshade::api::device* device, uint16_t ps_resource_id, uint16_t vs_resource_id, reshade::api::pipeline& sh_pipeline, reshade::api::pipeline_layout& sh_layout, reshade::api::sampler& sh_sampler)
{
    if (sh_pipeline == 0 && (device->get_api() == device_api::d3d9 || device->get_api() == device_api::d3d10 || device->get_api() == device_api::d3d11 || device->get_api() == device_api::d3d12))
    {
        sampler_desc sampler_desc = {};
        sampler_desc.filter = filter_mode::min_mag_mip_point;
        sampler_desc.address_u = texture_address_mode::clamp;
        sampler_desc.address_v = texture_address_mode::clamp;
        sampler_desc.address_w = texture_address_mode::clamp;

        pipeline_layout_param layout_params[2];
        layout_params[0] = descriptor_range{ 0, 0, 0, 1, shader_stage::all, 1, descriptor_type::sampler };
        layout_params[1] = descriptor_range{ 0, 0, 0, 1, shader_stage::all, 1, descriptor_type::shader_resource_view };

        if (!device->create_pipeline_layout(2, layout_params, &sh_layout) ||
            !CreatePipeline(device, sh_layout, ps_resource_id, vs_resource_id, sh_pipeline) ||
            !device->create_sampler(sampler_desc, &sh_sampler))
        {
            sh_pipeline = {};
            sh_layout = {};
            sh_sampler = {};
            reshade::log_message(reshade::log_level::warning, "Unable to create preview copy pipeline");
        }

        if (fullscreenQuadVertexBuffer == 0 && device->get_api() == device_api::d3d9)
        {
            const uint32_t num_vertices = 4;

            if (!device->create_resource(resource_desc(num_vertices * sizeof(vert_input), memory_heap::cpu_to_gpu, resource_usage::vertex_buffer), nullptr, resource_usage::cpu_access, &fullscreenQuadVertexBuffer))
            {
                reshade::log_message(reshade::log_level::warning, "Unable to create preview copy pipeline vertex buffer");
            }
            else
            {
                const vert_input vertices[num_vertices] = {
                    vert_input { vert_uv { 0.0f, 0.0f } },
                    vert_input { vert_uv { 0.0f, 1.0f } },
                    vert_input { vert_uv { 1.0f, 0.0f } },
                    vert_input { vert_uv { 1.0f, 1.0f } }
                };

                void* host_memory;

                if (device->map_buffer_region(fullscreenQuadVertexBuffer, 0, UINT64_MAX, map_access::write_only, &host_memory))
                {
                    memcpy(host_memory, vertices, num_vertices * sizeof(vert_input));
                    device->unmap_buffer_region(fullscreenQuadVertexBuffer);
                }
            }
        }
    }
}

void RenderingShaderManager::InitShaders(reshade::api::device* device)
{
    if (device->get_api() == device_api::d3d9)
    {
        InitShader(device, SHADER_PREVIEW_COPY_PS_3_0, SHADER_FULLSCREEN_VS_3_0, copyPipeline, copyPipelineLayout, copyPipelineSampler);
        CreatePipeline(device, copyPipelineLayout, SHADER_PREVIEW_COPY_PS_3_0, SHADER_FULLSCREEN_VS_3_0, copyPipelineAlpha, 0x7);
    }
    else
    {
        InitShader(device, SHADER_PREVIEW_COPY_PS_4_0, SHADER_FULLSCREEN_VS_4_0, copyPipeline, copyPipelineLayout, copyPipelineSampler);
        CreatePipeline(device, copyPipelineLayout, SHADER_PREVIEW_COPY_PS_4_0, SHADER_FULLSCREEN_VS_4_0, copyPipelineAlpha, 0x7);
    }
}

void RenderingShaderManager::DestroyShaders(reshade::api::device* device)
{
    copyPipeline = {};
    copyPipelineAlpha = {};
    copyPipelineLayout = {};
    copyPipelineSampler = {};
}

void RenderingShaderManager::ApplyShader(command_list* cmd_list, resource_view srv_src, resource_view rtv_dst, pipeline& sh_pipeline,
    pipeline_layout& sh_layout, sampler& sh_sampler, uint32_t width, uint32_t height)
{
    device* device = cmd_list->get_device();

    //if (device->get_api() == device_api::d3d12)
    //{
    //    DestroyShaders(device);
    //    InitShaders(device);
    //}

    if (sh_pipeline == 0 || !(device->get_api() == device_api::d3d9 || device->get_api() == device_api::d3d10 || device->get_api() == device_api::d3d11 || device->get_api() == device_api::d3d12))
    {
        return;
    }

    cmd_list->get_private_data<state_tracking>().capture(cmd_list, true);

    cmd_list->bind_render_targets_and_depth_stencil(1, &rtv_dst);

    cmd_list->bind_pipeline(pipeline_stage::all_graphics, sh_pipeline);

    cmd_list->push_descriptors(shader_stage::pixel, sh_layout, 0, descriptor_table_update{ {}, 0, 0, 1, descriptor_type::sampler, &sh_sampler });
    cmd_list->push_descriptors(shader_stage::pixel, sh_layout, 1, descriptor_table_update{ {}, 0, 0, 1, descriptor_type::shader_resource_view, &srv_src });

    const viewport viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
    cmd_list->bind_viewports(0, 1, &viewport);

    const rect scissor_rect = { 0, 0, static_cast<int32_t>(width), static_cast<int32_t>(height) };
    cmd_list->bind_scissor_rects(0, 1, &scissor_rect);

    if (cmd_list->get_device()->get_api() == device_api::d3d9)
    {
        cmd_list->bind_pipeline_state(dynamic_state::primitive_topology, static_cast<uint32_t>(primitive_topology::triangle_strip));
        cmd_list->bind_vertex_buffer(0, fullscreenQuadVertexBuffer, 0, sizeof(vert_input));
        cmd_list->draw(4, 1, 0, 0);
    }
    else
    {
        cmd_list->draw(3, 1, 0, 0);
    }
}

void RenderingShaderManager::CopyResource(command_list* cmd_list, resource_view srv_src, resource_view rtv_dst, uint32_t width, uint32_t height)
{
    ApplyShader(cmd_list, srv_src, rtv_dst, copyPipeline, copyPipelineLayout, copyPipelineSampler, width, height);
    cmd_list->get_private_data<state_tracking>().apply(cmd_list, true);
}

void RenderingShaderManager::CopyResourceMaskAlpha(reshade::api::command_list* cmd_list, reshade::api::resource_view srv_src, reshade::api::resource_view rtv_dst, uint32_t width, uint32_t height)
{
    ApplyShader(cmd_list, srv_src, rtv_dst, copyPipelineAlpha, copyPipelineLayout, copyPipelineSampler, width, height);
    cmd_list->get_private_data<state_tracking>().apply(cmd_list, true);
}