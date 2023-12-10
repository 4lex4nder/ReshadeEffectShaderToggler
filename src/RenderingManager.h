#pragma once

#include <reshade.hpp>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <functional>
#include "ToggleGroup.h"
#include "PipelinePrivateData.h"
#include "AddonUIData.h"
#include "ResourceManager.h"

namespace Rendering
{
    constexpr uint64_t CALL_DRAW = 0;
    constexpr uint64_t CALL_BIND_PIPELINE = 1;
    constexpr uint64_t CALL_BIND_RENDER_TARGET = 2;

    constexpr uint64_t MATCH_NONE       = 0b000000000000;
    constexpr uint64_t MATCH_EFFECT_PS  = 0b000000000001; // 0
    constexpr uint64_t MATCH_EFFECT_VS  = 0b000000000010; // 1
    constexpr uint64_t MATCH_EFFECT_CS  = 0b000000000100; // 2
    constexpr uint64_t MATCH_BINDING_PS = 0b000000001000; // 3
    constexpr uint64_t MATCH_BINDING_VS = 0b000000010000; // 4
    constexpr uint64_t MATCH_BINDING_CS = 0b000000100000; // 5
    constexpr uint64_t MATCH_CONST_PS   = 0b000001000000; // 6
    constexpr uint64_t MATCH_CONST_VS   = 0b000010000000; // 7
    constexpr uint64_t MATCH_CONST_CS   = 0b000100000000; // 8
    constexpr uint64_t MATCH_PREVIEW_PS = 0b001000000000; // 9
    constexpr uint64_t MATCH_PREVIEW_VS = 0b010000000000; // 10
    constexpr uint64_t MATCH_PREVIEW_CS = 0b100000000000; // 11

    constexpr uint64_t MATCH_ALL        = 0b111111111111;
    constexpr uint64_t MATCH_EFFECT     = 0b000000000111;
    constexpr uint64_t MATCH_BINDING    = 0b000000111000;
    constexpr uint64_t MATCH_CONST      = 0b000111000000;
    constexpr uint64_t MATCH_PREVIEW    = 0b111000000000;
    constexpr uint64_t MATCH_PS         = 0b001001001001;
    constexpr uint64_t MATCH_VS         = 0b010010010010;
    constexpr uint64_t MATCH_CS         = 0b100100100100;
    constexpr uint64_t MATCH_DELIMITER  = 12;

    constexpr uint64_t CHECK_MATCH_DRAW         = MATCH_ALL << (CALL_DRAW * MATCH_DELIMITER);
    constexpr uint64_t CHECK_MATCH_DRAW_EFFECT  = MATCH_EFFECT << (CALL_DRAW * MATCH_DELIMITER);
    constexpr uint64_t CHECK_MATCH_DRAW_BINDING = MATCH_BINDING << (CALL_DRAW * MATCH_DELIMITER);
    constexpr uint64_t CHECK_MATCH_DRAW_PREVIEW = MATCH_PREVIEW << (CALL_DRAW * MATCH_DELIMITER);

    constexpr uint64_t CHECK_MATCH_BIND_PIPELINE         = MATCH_ALL << (CALL_BIND_PIPELINE * MATCH_DELIMITER);
    constexpr uint64_t CHECK_MATCH_BIND_PIPELINE_EFFECT  = MATCH_EFFECT << (CALL_BIND_PIPELINE * MATCH_DELIMITER);
    constexpr uint64_t CHECK_MATCH_BIND_PIPELINE_BINDING = MATCH_BINDING << (CALL_BIND_PIPELINE * MATCH_DELIMITER);
    constexpr uint64_t CHECK_MATCH_BIND_PIPELINE_PREVIEW = MATCH_PREVIEW << (CALL_BIND_PIPELINE * MATCH_DELIMITER);

    constexpr uint64_t CHECK_MATCH_BIND_RENDERTARGET         = MATCH_ALL << (CALL_BIND_RENDER_TARGET * MATCH_DELIMITER);
    constexpr uint64_t CHECK_MATCH_BIND_RENDERTARGET_EFFECT  = MATCH_EFFECT << (CALL_BIND_RENDER_TARGET * MATCH_DELIMITER);
    constexpr uint64_t CHECK_MATCH_BIND_RENDERTARGET_BINDING = MATCH_BINDING << (CALL_BIND_RENDER_TARGET * MATCH_DELIMITER);
    constexpr uint64_t CHECK_MATCH_BIND_RENDERTARGET_PREVIEW = MATCH_PREVIEW << (CALL_BIND_RENDER_TARGET * MATCH_DELIMITER);

    class __declspec(novtable) RenderingManager final
    {
    public:
        static const reshade::api::resource GetCurrentResourceView(reshade::api::command_list* cmd_list, DeviceDataContainer& deviceData, ShaderToggler::ToggleGroup* group, CommandListDataContainer& commandListData, uint32_t descIndex, uint64_t action);
        static bool check_aspect_ratio(float width_to_check, float height_to_check, uint32_t width, uint32_t height, uint32_t matchingMode);
        
        static void EnumerateTechniques(reshade::api::effect_runtime* runtime, std::function<void(reshade::api::effect_runtime*, reshade::api::effect_technique, std::string&, std::string&)> func);
        static void QueueOrDequeue(
            reshade::api::command_list* cmd_list,
            DeviceDataContainer& deviceData,
            CommandListDataContainer& commandListData,
            std::unordered_map<EffectData*, std::tuple<ShaderToggler::ToggleGroup*, uint64_t, reshade::api::resource>>& queue,
            std::unordered_set<EffectData*>& immediateQueue,
            uint64_t callLocation,
            uint32_t layoutIndex,
            uint64_t action);
    private:
        static constexpr size_t CHAR_BUFFER_SIZE = 256;
        static size_t g_charBufferSize;
        static char g_charBuffer[CHAR_BUFFER_SIZE];
    };
}