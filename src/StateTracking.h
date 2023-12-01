/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include <vector>
#include <array>
#include <unordered_map>
#include <d3d9.h>
#include "DescriptorTracking.h"

 /// <summary>
 /// A state block capturing current state of a command list.
 /// </summary>

namespace StateTracking
{
    constexpr reshade::api::pipeline_stage ALL_PIPELINE_STAGES[] = {
        reshade::api::pipeline_stage::pixel_shader,
        reshade::api::pipeline_stage::vertex_shader,
        reshade::api::pipeline_stage::compute_shader,
        reshade::api::pipeline_stage::depth_stencil,
        reshade::api::pipeline_stage::domain_shader,
        reshade::api::pipeline_stage::geometry_shader,
        reshade::api::pipeline_stage::hull_shader,
        reshade::api::pipeline_stage::input_assembler,
        reshade::api::pipeline_stage::output_merger,
        reshade::api::pipeline_stage::rasterizer,
        reshade::api::pipeline_stage::stream_output
    };

    constexpr uint32_t ALL_PIPELINE_STAGES_SIZE = sizeof(ALL_PIPELINE_STAGES) / sizeof(reshade::api::pipeline_stage);

    constexpr reshade::api::shader_stage ALL_SHADER_STAGES[] = {
        reshade::api::shader_stage::pixel,
        reshade::api::shader_stage::vertex,
        reshade::api::shader_stage::compute,
        reshade::api::shader_stage::hull,
        reshade::api::shader_stage::geometry,
        reshade::api::shader_stage::domain
    };

    constexpr uint32_t ALL_SHADER_STAGES_SIZE = sizeof(ALL_SHADER_STAGES) / sizeof(reshade::api::shader_stage);

    struct barrier_track
    {
        reshade::api::resource_usage usage = reshade::api::resource_usage::undefined;
        int32_t ref_count = 0;
    };

    enum class root_entry_type : int32_t
    {
        undefined = -1,
        push_constants = 1,
        descriptor_table = 0,
        push_descriptors = 2,
        push_descriptors_with_ranges = 3
    };

    struct root_entry
    {
        constexpr root_entry() : type(root_entry_type::undefined), buffer_index(-1), descriptor_table({ 0 }) {}
        constexpr root_entry(root_entry_type t, int32_t index, const reshade::api::descriptor_table& table) : type(t), buffer_index(index), descriptor_table(table) {}
        constexpr root_entry(const reshade::api::descriptor_table& table) : type(root_entry_type::descriptor_table), buffer_index(-1), descriptor_table(table) {}

        root_entry_type type = root_entry_type::undefined;
        int32_t buffer_index = -1;;
        reshade::api::descriptor_table descriptor_table = {};
    };

    struct state_block
    {
        /// <summary>
        /// Binds all state captured by this state block on the specified command list.
        /// </summary>
        /// <param name="cmd_list">Target command list to bind the state on.</param>

        void capture(reshade::api::command_list* cmd_list, bool force_restore = false);

        void apply(reshade::api::command_list* cmd_list, bool force_restore = false);
        void apply_dx9(reshade::api::command_list* cmd_list, bool force_restore);
        void apply_default(reshade::api::command_list* cmd_list, bool force_restore) const;

        void apply_descriptors_dx12_vulkan(reshade::api::command_list* cmd_list) const;
        void apply_descriptors(reshade::api::command_list* cmd_list) const;

        void start_resource_barrier_tracking(reshade::api::resource res, reshade::api::resource_usage current_usage);
        reshade::api::resource_usage stop_resource_barrier_tracking(reshade::api::resource res);

        const descriptor_tracking::descriptor_data* get_descriptor_at(uint32_t stageIndex, uint32_t layout_param, uint32_t binding) const;
        const size_t get_root_table_entry_size_at(uint32_t stageIndex, uint32_t layout_param) const;
        const size_t get_root_table_size_at(uint32_t stageIndex) const;
        const std::vector<uint32_t>* get_constants_at(uint32_t stageIndex, uint32_t layout_param) const;

        /// <summary>
        /// Removes all state in this state block.
        /// </summary>
        void clear();
        void clear_present(reshade::api::effect_runtime* runtime);

        std::vector<reshade::api::resource_view> render_targets;
        reshade::api::resource_view depth_stencil = { 0 };
        std::array<reshade::api::pipeline, ALL_PIPELINE_STAGES_SIZE> current_pipeline;
        std::array<reshade::api::pipeline_stage, ALL_PIPELINE_STAGES_SIZE> current_pipeline_stage;
        reshade::api::primitive_topology primitive_topology = reshade::api::primitive_topology::undefined;
        uint32_t blend_constant = 0;
        uint32_t sample_mask = 0xFFFFFFFF;
        uint32_t front_stencil_reference_value = 0;
        uint32_t back_stencil_reference_value = 0;
        std::vector<reshade::api::viewport> viewports;
        std::vector<reshade::api::rect> scissor_rects;

        std::array<std::pair<reshade::api::pipeline_layout, std::vector<root_entry>>, ALL_SHADER_STAGES_SIZE> root_tables;
        std::array<reshade::api::shader_stage, ALL_SHADER_STAGES_SIZE> root_table_stages;
        std::array<std::vector<std::vector<uint32_t>>, ALL_SHADER_STAGES_SIZE> constant_buffer;
        std::array<std::vector<std::vector<descriptor_tracking::descriptor_data>>, ALL_SHADER_STAGES_SIZE> descriptor_buffer;

        std::unordered_map<uint64_t, barrier_track> resource_barrier_track;

        IDirect3DStateBlock9* dx_state;
    };
}

/// <summary>
/// An instance of this is automatically created for all command lists and can be queried with <c>cmd_list->get_private_data&lt;state_tracking&gt;()</c> (assuming state tracking was registered via <see cref="state_tracking::register_events"/>).
/// </summary>
class __declspec(uuid("c9abddf0-f9c2-4a7b-af49-89d8d470e207")) state_tracking : public StateTracking::state_block
{
public:
    /// <summary>
    /// Registers all the necessary add-on events for state tracking to work.
    /// </summary>
    static void register_events(bool track = true);
    /// <summary>
    /// Unregisters all the necessary add-on events for state tracking to work.
    /// </summary>
    static void unregister_events();
private:
    static bool track_descriptors;
};
