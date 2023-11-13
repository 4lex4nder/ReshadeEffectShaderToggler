#pragma once

#include "RenderingManager.h"

namespace Rendering
{
    class __declspec(novtable) RenderingShaderManager final
    {
    public:
        RenderingShaderManager(AddonImGui::AddonUIData& data, ResourceManager& rManager);
        ~RenderingShaderManager();

        void InitShaders(reshade::api::device* device);
        void DestroyShaders(reshade::api::device* device);

        void CopyResource(reshade::api::command_list* cmd_list, reshade::api::resource_view srv_src, reshade::api::resource_view rtv_dst, uint32_t width, uint32_t height);
        void CopyResourceMaskAlpha(reshade::api::command_list* cmd_list, reshade::api::resource_view srv_src, reshade::api::resource_view rtv_dst, uint32_t width, uint32_t height);
    private:
        struct vert_uv
        {
            float x, y;
        };

        struct vert_input
        {
            vert_uv uv;
        };

        void InitShader(reshade::api::device* device, uint16_t ps_resource_id, uint16_t vs_resource_id, reshade::api::pipeline& sh_pipeline, reshade::api::pipeline_layout& sh_layout, reshade::api::sampler& sh_sampler);
        void ApplyShader(reshade::api::command_list* cmd_list, reshade::api::resource_view srv_src, reshade::api::resource_view rtv_dst, reshade::api::pipeline& sh_pipeline,
            reshade::api::pipeline_layout& sh_layout, reshade::api::sampler& sh_sampler, uint32_t width, uint32_t height);
        bool CreatePipeline(reshade::api::device* device, reshade::api::pipeline_layout layout, uint16_t ps_resource_id, uint16_t vs_resource_id, reshade::api::pipeline& sh_pipeline, uint8_t write_mask = 0xF);

        AddonImGui::AddonUIData& uiData;
        ResourceManager& resourceManager;

        reshade::api::pipeline copyPipeline;
        reshade::api::pipeline copyPipelineAlpha;
        reshade::api::pipeline_layout copyPipelineLayout;
        reshade::api::sampler copyPipelineSampler;

        reshade::api::resource fullscreenQuadVertexBuffer = {};
    };
}