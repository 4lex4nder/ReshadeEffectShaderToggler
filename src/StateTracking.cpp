/*
 * Copyright (C) 2022 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#include <limits>
#include "reshade.hpp"
#include "StateTracking.h"

using namespace reshade::api;
using namespace StateTracking;

bool state_tracking::track_descriptors = true;

void state_block::apply_descriptors_dx12_vulkan(command_list* cmd_list) const
{
    uint32_t shader_stages_set = 0;
    for (uint32_t stageIdx = 0; stageIdx < ALL_SHADER_STAGES_SIZE; stageIdx++)
    {
        auto& descriptor_data = cmd_list->get_device()->get_private_data<descriptor_tracking>();
        const auto& [pipelinelayout, root_table] = root_tables[stageIdx];
        shader_stage stages = root_table_stages[stageIdx];

        if ((static_cast<uint32_t>(stages) | shader_stages_set) <= shader_stages_set)
        {
            continue;
        }

        shader_stages_set |= static_cast<uint32_t>(stages);

        // Restore root signature and descriptor heaps
        if (pipelinelayout != 0)
        {
            cmd_list->bind_descriptor_tables(stages, pipelinelayout, 0, 0, nullptr);
        }
        else
        {
            continue;
        }

        // Restore tables in first pass to assure heaps are restored, do constants in a second pass,
        // pushed descriptors should be restored along with the tables when the heap is restored to the game internal one
        for (uint32_t i = 0; i < root_table.size(); i++)
        {
            if (root_table[i].type == root_entry_type::descriptor_table && root_table[i].descriptor_table.handle != 0)
            {
                cmd_list->bind_descriptor_tables(stages, pipelinelayout, i, 1, &root_table[i].descriptor_table);
            }

            if (root_table[i].type == root_entry_type::push_constants && root_table[i].buffer_index >= 0 && constant_buffer[root_table[i].buffer_index].size() > 0)
            {
                cmd_list->push_constants(stages, pipelinelayout, i, 0, static_cast<uint32_t>(constant_buffer[root_table[i].buffer_index].size()), constant_buffer[root_table[i].buffer_index].data());
            }
        }
    }
}

void state_block::apply_descriptors(command_list* cmd_list) const
{
    auto& [desc_layout, descriptors] = cmd_list->get_private_data<state_tracking>().root_tables[0];
    const size_t it = std::min(static_cast<size_t>(2), descriptors.size());

    for (uint32_t i = 0; i < it; i++)
    {
        if (descriptors[i].type == root_entry_type::push_descriptors && descriptors[i].buffer_index >= 0 && descriptor_buffer[0][descriptors[i].buffer_index].size() > 0)
        {
            const descriptor_tracking::descriptor_data* desc = &descriptor_buffer[0][descriptors[i].buffer_index][0];

            switch (desc->type)
            {
            case descriptor_type::sampler:
                cmd_list->push_descriptors(shader_stage::pixel, desc_layout, i, descriptor_table_update{ {}, 0, 0, 1, descriptor_type::sampler, reinterpret_cast<const void*>(&desc->sampler) });
                break;
            case descriptor_type::constant_buffer:
                cmd_list->push_descriptors(shader_stage::pixel, desc_layout, i, descriptor_table_update{ {}, 0, 0, 1, descriptor_type::constant_buffer, reinterpret_cast<const void*>(&desc->constant) });
                break;
            case descriptor_type::sampler_with_resource_view:
                cmd_list->push_descriptors(shader_stage::pixel, desc_layout, i, descriptor_table_update{ {}, 0, 0, 1, descriptor_type::sampler_with_resource_view, reinterpret_cast<const void*>(&desc->sampler_and_view) });
                break;
            case descriptor_type::shader_resource_view:
                cmd_list->push_descriptors(shader_stage::pixel, desc_layout, i, descriptor_table_update{ {}, 0, 0, 1, descriptor_type::shader_resource_view, reinterpret_cast<const void*>(&desc->view) });
                break;
            case descriptor_type::unordered_access_view:
                cmd_list->push_descriptors(shader_stage::pixel, desc_layout, i, descriptor_table_update{ {}, 0, 0, 1, descriptor_type::unordered_access_view, reinterpret_cast<const void*>(&desc->view) });
                break;
            }
        }
    }
}

void state_block::capture(command_list* cmd_list, bool force_restore)
{
    if (force_restore && cmd_list->get_device()->get_api() == device_api::d3d9 && dx_state == nullptr)
    {
        IDirect3DDevice9* device = reinterpret_cast<IDirect3DDevice9*>(cmd_list->get_device()->get_native());

        if (SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, &dx_state)))
        {
            dx_state->Capture();
        }
    }
}

void state_block::apply(command_list* cmd_list, bool force_restore)
{
    switch (cmd_list->get_device()->get_api())
    {
    case device_api::d3d9:
        apply_dx9(cmd_list, force_restore);
        break;
    default:
        apply_default(cmd_list, force_restore);
    }
}

void state_block::apply_dx9(reshade::api::command_list* cmd_list, bool force_restore)
{
    if (!force_restore)
    {
        return;
    }

    // ???
    if (!render_targets.empty() || depth_stencil != 0)
        cmd_list->bind_render_targets_and_depth_stencil(static_cast<uint32_t>(render_targets.size()), render_targets.data(), depth_stencil);

    if (dx_state != nullptr)
    {
        dx_state->Apply();
        dx_state->Release();
        dx_state = nullptr;
    }
}

void state_block::apply_default(reshade::api::command_list* cmd_list, bool force_restore) const
{
    // For dx12 and vulkan, always force full state restoration
    if (!force_restore && cmd_list->get_device()->get_api() != device_api::d3d12 && cmd_list->get_device()->get_api() != device_api::vulkan)
    {
        return;
    }

    if (!render_targets.empty() || depth_stencil != 0)
        cmd_list->bind_render_targets_and_depth_stencil(static_cast<uint32_t>(render_targets.size()), render_targets.data(), depth_stencil);

    uint32_t pipeline_stages_set = 0;
    for (uint32_t s = 0; s < ALL_PIPELINE_STAGES_SIZE; s++)
    {
        if ((static_cast<uint32_t>(current_pipeline_stage[s]) | pipeline_stages_set) > pipeline_stages_set)
        {
            pipeline_stages_set |= static_cast<uint32_t>(current_pipeline_stage[s]);
            cmd_list->bind_pipeline(current_pipeline_stage[s], current_pipeline[s]);
        }
    }

    if (primitive_topology != primitive_topology::undefined)
        cmd_list->bind_pipeline_state(dynamic_state::primitive_topology, static_cast<uint32_t>(primitive_topology));
    if (blend_constant != 0)
        cmd_list->bind_pipeline_state(dynamic_state::blend_constant, blend_constant);
    if (sample_mask != 0xFFFFFFFF)
        cmd_list->bind_pipeline_state(dynamic_state::sample_mask, sample_mask);
    if (front_stencil_reference_value != 0)
        cmd_list->bind_pipeline_state(dynamic_state::front_stencil_reference_value, front_stencil_reference_value);
    if (back_stencil_reference_value != 0)
        cmd_list->bind_pipeline_state(dynamic_state::back_stencil_reference_value, back_stencil_reference_value);

    if (!viewports.empty())
        cmd_list->bind_viewports(0, static_cast<uint32_t>(viewports.size()), viewports.data());
    if (!scissor_rects.empty())
        cmd_list->bind_scissor_rects(0, static_cast<uint32_t>(scissor_rects.size()), scissor_rects.data());

    if (cmd_list->get_device()->get_api() == device_api::d3d12 || cmd_list->get_device()->get_api() == device_api::vulkan)
    {
        apply_descriptors_dx12_vulkan(cmd_list);
    }
    else
    {
        apply_descriptors(cmd_list);
    }
}

void state_block::clear()
{
    render_targets.clear();
    depth_stencil = { 0 };
    primitive_topology = primitive_topology::undefined;
    blend_constant = 0;
    front_stencil_reference_value = 0;
    back_stencil_reference_value = 0;
    sample_mask = 0xFFFFFFFF;
    viewports.clear();
    scissor_rects.clear();
    root_tables.fill(make_pair(pipeline_layout{ 0 }, std::vector<root_entry>()));
    root_table_stages.fill(static_cast<shader_stage>(0));
    std::for_each(constant_buffer.begin(), constant_buffer.end(), [](std::vector<std::vector<uint32_t>>& v) { v.clear(); });
    std::for_each(descriptor_buffer.begin(), descriptor_buffer.end(), [](std::vector<std::vector<descriptor_tracking::descriptor_data>>& v) { v.clear(); });
    current_pipeline.fill(pipeline{ 0 });
    current_pipeline_stage.fill(static_cast<pipeline_stage>(0));
    resource_barrier_track.clear();
}

void state_block::clear_present(effect_runtime* runtime)
{
     render_targets.clear();
     depth_stencil = { 0 };
}

static inline int32_t get_shader_stage_index(shader_stage stages)
{
    const uint32_t stage_value = static_cast<uint32_t>(stages);

    for (int32_t index = 0; index < ALL_SHADER_STAGES_SIZE; ++index)
    {
        if (stage_value & static_cast<uint32_t>(ALL_SHADER_STAGES[index]))
        {
            return index;
        }
    }

    return -1;
}

static inline int32_t get_pipeline_stage_index(pipeline_stage stages)
{
    const uint32_t stage_value = static_cast<uint32_t>(stages);

    for (int32_t index = 0; index < ALL_PIPELINE_STAGES_SIZE; ++index)
    {
        if (stage_value & static_cast<uint32_t>(ALL_PIPELINE_STAGES[index]))
        {
            return index;
        }
    }

    return -1;
}

static void on_init_command_list(command_list* cmd_list)
{
    cmd_list->create_private_data<state_tracking>();
}
static void on_destroy_command_list(command_list* cmd_list)
{
    cmd_list->destroy_private_data<state_tracking>();
}

static void on_bind_render_targets_and_depth_stencil(command_list* cmd_list, uint32_t count, const resource_view* rtvs, resource_view dsv)
{
    auto& state = cmd_list->get_private_data<state_tracking>();
    state.render_targets.assign(rtvs, rtvs + count);
    state.depth_stencil = dsv;
}

static void on_bind_pipeline(command_list* cmd_list, pipeline_stage stages, pipeline pipeline)
{
    auto& state = cmd_list->get_private_data<state_tracking>();

    uint32_t idx = get_pipeline_stage_index(stages);

    state.current_pipeline[idx] = pipeline;
    state.current_pipeline_stage[idx] = stages;
}

static void on_bind_pipeline_states(command_list* cmd_list, uint32_t count, const dynamic_state* states, const uint32_t* values)
{
    auto& state = cmd_list->get_private_data<state_tracking>();

    for (uint32_t i = 0; i < count; ++i)
    {
        switch (states[i])
        {
        case dynamic_state::primitive_topology:
            state.primitive_topology = static_cast<primitive_topology>(values[i]);
            break;
        case dynamic_state::blend_constant:
            state.blend_constant = values[i];
            break;
        case dynamic_state::front_stencil_reference_value:
            state.front_stencil_reference_value = values[i];
            break;
        case dynamic_state::back_stencil_reference_value:
            state.back_stencil_reference_value = values[i];
            break;
        case dynamic_state::sample_mask:
            state.sample_mask = values[i];
            break;
        }
    }
}

static void on_bind_viewports(command_list* cmd_list, uint32_t first, uint32_t count, const viewport* viewports)
{
    auto& state = cmd_list->get_private_data<state_tracking>();

    if (state.viewports.size() < (first + count))
        state.viewports.resize(first + count);

    for (uint32_t i = 0; i < count; ++i)
        state.viewports[i + first] = viewports[i];
}

static void on_bind_scissor_rects(command_list* cmd_list, uint32_t first, uint32_t count, const rect* rects)
{
    auto& state = cmd_list->get_private_data<state_tracking>();

    if (state.scissor_rects.size() < (first + count))
        state.scissor_rects.resize(first + count);

    for (uint32_t i = 0; i < count; ++i)
        state.scissor_rects[i + first] = rects[i];
}

static void on_bind_descriptor_tables(command_list* cmd_list, shader_stage stages, pipeline_layout layout, uint32_t first, uint32_t count, const descriptor_table* tables)
{
    int32_t idx = get_shader_stage_index(stages);

    if (idx < 0)
        return;

    auto& state_tracker = cmd_list->get_private_data<state_tracking>();
    const auto& descriptor_state = cmd_list->get_device()->get_private_data<descriptor_tracking>();
    auto& [desc_layout, root_table] = state_tracker.root_tables[idx];
    auto& state_stages = state_tracker.root_table_stages[idx];
    auto& descriptor_buffer = state_tracker.descriptor_buffer[idx];
    auto& constant_buffer = state_tracker.constant_buffer[idx];

    if (desc_layout != layout)
    {
        root_table.clear(); // Layout changed, which resets all descriptor set bindings
        descriptor_buffer.clear();
        constant_buffer.clear();
    }

    desc_layout = layout;
    state_stages = stages;

    if (root_table.size() < (first + count))
        root_table.resize(first + count);

    for (uint32_t i = 0; i < count; ++i)
    {
        const pipeline_layout_param param = descriptor_state.get_pipeline_layout_param(layout, first + i);
        if (param.type != pipeline_layout_param_type::descriptor_table)
            continue;
    
        uint32_t max_descriptor_size = 0;
        for (uint32_t k = 0; k < param.descriptor_table.count; ++k)
        {
            const descriptor_range& range = param.descriptor_table.ranges[k];
            if (range.count != UINT32_MAX && range.type != descriptor_type::sampler)
                max_descriptor_size = std::max(max_descriptor_size, range.binding + range.count);
        }

        std::vector<descriptor_tracking::descriptor_data> descriptors(max_descriptor_size);
    
        for (uint32_t k = 0; k < param.descriptor_table.count; ++k)
        {
            const descriptor_range& range = param.descriptor_table.ranges[k];
    
            if (range.count == UINT32_MAX || range.type == descriptor_type::sampler)
                continue; // Skip unbounded ranges
    
            uint32_t base_offset = 0;
            descriptor_heap heap = { 0 };
            cmd_list->get_device()->get_descriptor_heap_offset(tables[i], range.binding, 0, &heap, &base_offset);
    
            descriptor_state.set_all_descriptors(heap, base_offset, range.count, descriptors, range.binding);
        }

        descriptor_buffer.push_back(std::move(descriptors));

        root_table[i + first] = { root_entry_type::descriptor_table, static_cast<int32_t>(descriptor_buffer.size()) - 1, tables[i]};
    }
}

static void on_bind_descriptor_tables_no_track(command_list* cmd_list, shader_stage stages, pipeline_layout layout, uint32_t first, uint32_t count, const descriptor_table* tables)
{
    int32_t idx = get_shader_stage_index(stages);

    if (idx < 0)
        return;

    auto& state_tracker = cmd_list->get_private_data<state_tracking>();
    auto& [desc_layout, root_table] = state_tracker.root_tables[idx];
    auto& state_stages = state_tracker.root_table_stages[idx];
    auto& constant_buffer = state_tracker.constant_buffer[idx];
    auto& descriptor_buffer = state_tracker.descriptor_buffer[idx];

    if (desc_layout != layout)
    {
        root_table.clear(); // Layout changed, which resets all descriptor set bindings
        constant_buffer.clear();
        descriptor_buffer.clear();
    }

    desc_layout = layout;
    state_stages = stages;

    if (root_table.size() < (first + count))
        root_table.resize(first + count);

    for (uint32_t i = 0; i < count; ++i)
    {
        root_table[i + first] = tables[i];
    }
}

static inline void fill_descriptors(std::vector<descriptor_tracking::descriptor_data>& table, const descriptor_table_update& update)
{
    for (uint32_t i = 0; i < update.count; i++)
    {
        descriptor_tracking::descriptor_data& descriptor = table[update.binding + i];

        descriptor.type = update.type;

        switch (update.type)
        {
        case descriptor_type::sampler:
            descriptor.sampler = reinterpret_cast<const sampler*>(update.descriptors)[i];
            break;
        case descriptor_type::sampler_with_resource_view:
            descriptor.sampler_and_view = reinterpret_cast<const sampler_with_resource_view*>(update.descriptors)[i];
            descriptor.view = descriptor.sampler_and_view.view;
            descriptor.sampler = descriptor.sampler_and_view.sampler;
            break;
        case descriptor_type::shader_resource_view:
        case descriptor_type::unordered_access_view:
            descriptor.view = reinterpret_cast<const resource_view*>(update.descriptors)[i];
            break;
        case descriptor_type::constant_buffer:
        case descriptor_type::shader_storage_buffer:
            descriptor.constant = reinterpret_cast<const buffer_range*>(update.descriptors)[i];
        }
    }
}

static void on_push_descriptors(command_list* cmd_list, shader_stage stages, pipeline_layout layout, uint32_t layout_param, const descriptor_table_update& update)
{
    int32_t idx = get_shader_stage_index(stages);

    if (idx < 0)
        return;

    auto& state_tracker = cmd_list->get_private_data<state_tracking>();
    auto& [desc_layout, root_table] = state_tracker.root_tables[idx];
    auto& state_stages = state_tracker.root_table_stages[idx];
    auto& descriptor_buffer = state_tracker.descriptor_buffer[idx];
    
    desc_layout = layout;
    state_stages = stages;

    if (root_table.size() < layout_param + 1)
    {
        root_table.resize(layout_param + 1);
    }

    auto& root_table_entry = root_table[layout_param];

    // Initialize table for descriptors
    if (root_table_entry.type == root_entry_type::undefined)
    {
        std::vector<descriptor_tracking::descriptor_data> buf(update.binding + update.count);

        fill_descriptors(buf, update);

        descriptor_buffer.push_back(std::move(buf));
        root_table_entry = { root_entry_type::push_descriptors, static_cast<int32_t>(descriptor_buffer.size()) - 1, {} };
    }
    else
    {
        auto& buf = descriptor_buffer[root_table_entry.buffer_index];

        if (buf.size() < update.binding + update.count)
        {
            buf.resize(update.binding + update.count);
        }

        fill_descriptors(buf, update);
    }
}

static void on_push_constants(command_list* cmd_list, shader_stage stages, pipeline_layout layout, uint32_t layout_param, uint32_t first, uint32_t count, const void* values)
{
    int32_t idx = get_shader_stage_index(stages);

    if (idx < 0)
        return;

    auto& state_tracker = cmd_list->get_private_data<state_tracking>();
    auto& [desc_layout, root_table] = state_tracker.root_tables[idx];
    auto& state_stages = state_tracker.root_table_stages[idx];
    auto& constant_buffer = state_tracker.constant_buffer[idx];
    
    desc_layout = layout;
    state_stages = stages;

    if (root_table.size() < layout_param + 1)
    {
        root_table.resize(layout_param + 1);
    }

    auto& root_table_entry = root_table[layout_param];

    // Not buffered yet, initialize
    if (root_table_entry.type == root_entry_type::undefined)
    {
        std::vector<uint32_t> buf(first + count);

        for (uint32_t i = 0; i < count; i++)
        {
            buf[first + i] = reinterpret_cast<const uint32_t*>(values)[i];
        }

        constant_buffer.push_back(std::move(buf));

        root_table_entry = { root_entry_type::push_constants, static_cast<int32_t>(constant_buffer.size()) - 1, {} };
    }
    else // Write to existing buffer index
    {
        auto& buf = constant_buffer[root_table_entry.buffer_index];

        if (buf.size() < first + count)
        {
            buf.resize(first + count);
        }

        for (uint32_t i = 0; i < count; i++)
        {
            buf[first + i] = reinterpret_cast<const uint32_t*>(values)[i];
        }
    }
}

const descriptor_tracking::descriptor_data* state_block::get_descriptor_at(uint32_t stageIndex, uint32_t layout_param, uint32_t binding) const
{
    if (root_tables[stageIndex].second.size() > layout_param)
    {
        const auto& root_entry = root_tables[stageIndex].second[layout_param];

        if ((root_entry.type == root_entry_type::push_descriptors || root_entry.type == root_entry_type::descriptor_table) && root_entry.buffer_index >= 0)
        {
            const auto& table_entry = descriptor_buffer[stageIndex][root_entry.buffer_index];

            if (table_entry.size() > binding)
            {
                return &table_entry[binding];
            }
        }
    }

    return nullptr;
}


const size_t state_block::get_root_table_entry_size_at(uint32_t stageIndex, uint32_t layout_param) const
{
    if (root_tables[stageIndex].second.size() > layout_param)
    {
        const auto& root_entry = root_tables[stageIndex].second[layout_param];

        if ((root_entry.type == root_entry_type::push_descriptors || root_entry.type == root_entry_type::descriptor_table) && root_entry.buffer_index >= 0)
        {
            return descriptor_buffer[root_entry.buffer_index].size();
        }
        else if (root_entry.type == root_entry_type::push_constants && root_entry.buffer_index >= 0)
        {
            return constant_buffer[root_entry.buffer_index].size();
        }
    }

    return 0;
}

const size_t state_block::get_root_table_size_at(uint32_t stageIndex) const
{
    return root_tables[stageIndex].second.size();
}

const std::vector<uint32_t>* state_block::get_constants_at(uint32_t stageIndex, uint32_t layout_param) const
{
    if (root_tables[stageIndex].second.size() > layout_param)
    {
        const auto& root_entry = root_tables[stageIndex].second[layout_param];

        if (root_entry.type == root_entry_type::push_constants && root_entry.buffer_index >= 0)
        {
            return &constant_buffer[stageIndex][root_entry.buffer_index];
        }
    }

    return {};
}

static void on_reset_command_list(command_list* cmd_list)
{
    auto& state = cmd_list->get_private_data<state_tracking>();
    state.clear();
}

static void on_reshade_present(effect_runtime* runtime)
{
    if (runtime->get_device()->get_api() != device_api::d3d12 && runtime->get_device()->get_api() != device_api::vulkan)
    {
        auto& state = runtime->get_command_queue()->get_immediate_command_list()->get_private_data<state_tracking>();
        state.clear_present(runtime);
    }
}

void state_block::start_resource_barrier_tracking(reshade::api::resource res, reshade::api::resource_usage current_usage)
{
    const auto& restrack = resource_barrier_track.find(res.handle);

    if (restrack != resource_barrier_track.end())
    {
        restrack->second.ref_count++;
    }
    else
    {
        resource_barrier_track.emplace(res.handle, barrier_track{ current_usage, 1 });
    }
}

reshade::api::resource_usage state_block::stop_resource_barrier_tracking(reshade::api::resource res)
{
    const auto& restrack = resource_barrier_track.find(res.handle);

    if (restrack != resource_barrier_track.end())
    {
        resource_usage usage = restrack->second.usage;

        restrack->second.ref_count--;
        if (restrack->second.ref_count <= 0)
        {
            resource_barrier_track.erase(res.handle);
        }

        return usage;
    }

    return resource_usage::undefined;
}

static void on_barrier(command_list* cmd_list, uint32_t count, const resource* resources, const resource_usage* old_states, const resource_usage* new_states)
{
    auto& barrier_track = cmd_list->get_private_data<state_tracking>().resource_barrier_track;
    if (barrier_track.size() > 0)
    {
        for (uint32_t i = 0; i < count; i++)
        {
            const auto& restrack = barrier_track.find(resources[i].handle);

            if (restrack != barrier_track.end())
            {
                restrack->second.usage = new_states[i];
            }
        }
    }
}

void state_tracking::register_events(bool track)
{
    // disable for now
    track = false;

    track_descriptors = track;
    descriptor_tracking::register_events(track);

    reshade::register_event<reshade::addon_event::init_command_list>(on_init_command_list);
    reshade::register_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);

    reshade::register_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(on_bind_render_targets_and_depth_stencil);
    reshade::register_event<reshade::addon_event::bind_pipeline>(on_bind_pipeline);
    reshade::register_event<reshade::addon_event::bind_pipeline_states>(on_bind_pipeline_states);
    reshade::register_event<reshade::addon_event::bind_viewports>(on_bind_viewports);
    reshade::register_event<reshade::addon_event::bind_scissor_rects>(on_bind_scissor_rects);
    reshade::register_event<reshade::addon_event::barrier>(on_barrier);
    reshade::register_event<reshade::addon_event::reshade_present>(on_reshade_present);

    reshade::register_event<reshade::addon_event::push_descriptors>(on_push_descriptors);
    reshade::register_event<reshade::addon_event::push_constants>(on_push_constants);

    if (track_descriptors)
    {
        reshade::register_event<reshade::addon_event::bind_descriptor_tables>(on_bind_descriptor_tables);
    }
    else
    {
        reshade::register_event<reshade::addon_event::bind_descriptor_tables>(on_bind_descriptor_tables_no_track);
    }

    reshade::register_event<reshade::addon_event::reset_command_list>(on_reset_command_list);
}
void state_tracking::unregister_events()
{
    descriptor_tracking::unregister_events(track_descriptors);

    reshade::unregister_event<reshade::addon_event::init_command_list>(on_init_command_list);
    reshade::unregister_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);

    reshade::unregister_event<reshade::addon_event::bind_render_targets_and_depth_stencil>(on_bind_render_targets_and_depth_stencil);
    reshade::unregister_event<reshade::addon_event::bind_pipeline>(on_bind_pipeline);
    reshade::unregister_event<reshade::addon_event::bind_pipeline_states>(on_bind_pipeline_states);
    reshade::unregister_event<reshade::addon_event::bind_viewports>(on_bind_viewports);
    reshade::unregister_event<reshade::addon_event::bind_scissor_rects>(on_bind_scissor_rects);
    reshade::unregister_event<reshade::addon_event::barrier>(on_barrier);
    reshade::unregister_event<reshade::addon_event::reshade_present>(on_reshade_present);

    reshade::unregister_event<reshade::addon_event::push_descriptors>(on_push_descriptors);
    reshade::unregister_event<reshade::addon_event::push_constants>(on_push_constants);

    if (track_descriptors)
    {
        reshade::unregister_event<reshade::addon_event::bind_descriptor_tables>(on_bind_descriptor_tables);
    }
    else
    {
        reshade::unregister_event<reshade::addon_event::bind_descriptor_tables>(on_bind_descriptor_tables_no_track);
    }

    reshade::unregister_event<reshade::addon_event::reset_command_list>(on_reset_command_list);
}
