///////////////////////////////////////////////////////////////////////
//
// Part of ShaderToggler, a shader toggler add on for Reshade 5+ which allows you
// to define groups of shaders to toggle them on/off with one key press
// 
// (c) Frans 'Otis_Inf' Bouma.
//
// All rights reserved.
// https://github.com/FransBouma/ShaderToggler
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met :
//
//  * Redistributions of source code must retain the above copyright notice, this
//	  list of conditions and the following disclaimer.
//
//  * Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and / or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
/////////////////////////////////////////////////////////////////////////
#pragma once

#include <unordered_map>
#include <filesystem>
#include <reshade.hpp>
#include "ShaderManager.h"
#include "CDataFile.h"
#include "ToggleGroup.h"
#include "ConstantHandlerBase.h"

constexpr auto FRAMECOUNT_COLLECTION_PHASE_DEFAULT = 10;
constexpr auto HASH_FILE_NAME = "ReshadeEffectShaderToggler.ini";

namespace AddonImGui
{
    enum Keybind : uint32_t
    {
        PIXEL_SHADER_DOWN = 0,
        PIXEL_SHADER_UP,
        PIXEL_SHADER_MARK,
        PIXEL_SHADER_MARKED_DOWN,
        PIXEL_SHADER_MARKED_UP,
        VERTEX_SHADER_DOWN,
        VERTEX_SHADER_UP,
        VERTEX_SHADER_MARK,
        VERTEX_SHADER_MARKED_DOWN,
        VERTEX_SHADER_MARKED_UP,
        INVOCATION_DOWN,
        INVOCATION_UP,
        DESCRIPTOR_DOWN,
        DESCRIPTOR_UP
    };

    static const char* KeybindNames[] = {
        "PIXEL_SHADER_DOWN",
        "PIXEL_SHADER_UP",
        "PIXEL_SHADER_MARK",
        "PIXEL_SHADER_MARKED_DOWN",
        "PIXEL_SHADER_MARKED_UP",
        "VERTEX_SHADER_DOWN",
        "VERTEX_SHADER_UP",
        "VERTEX_SHADER_MARK",
        "VERTEX_SHADER_MARKED_DOWN",
        "VERTEX_SHADER_MARKED_UP",
        "INVOCATION_DOWN",
        "INVOCATION_UP",
        "DESCRIPTOR_DOWN",
        "DESCRIPTOR_UP"
    };

    enum TabType : uint32_t
    {
        TAB_NONE = 0,
        TAB_RENDER_TARGET,
        TAB_TEXTURE_BINDING,
        TAB_CONSTANT_BUFFER,
    };

    class AddonUIData
    {
    private:
        ShaderToggler::ShaderManager* _pixelShaderManager;
        ShaderToggler::ShaderManager* _vertexShaderManager;
        ShaderToggler::ShaderManager* _computeShaderManager;
        Shim::Constants::ConstantHandlerBase* _constantHandler;
        std::atomic_uint32_t* _activeCollectorFrameCounter;
        std::vector<std::string>* _allTechniques;
        std::atomic_uint _invocationLocation = 0;
        std::atomic_uint _descriptorIndex = 0;
        std::atomic_int _toggleGroupIdShaderEditing = -1;
        std::atomic_int _toggleGroupIdEffectEditing = -1;
        std::atomic_int _toggleGroupIdConstantEditing = -1;
        std::unordered_map<int, ShaderToggler::ToggleGroup> _toggleGroups;
        std::unordered_map<uint32_t, std::vector<ShaderToggler::ToggleGroup*>> _pixelShaderHashToToggleGroups;
        std::unordered_map<uint32_t, std::vector<ShaderToggler::ToggleGroup*>> _vertexShaderHashToToggleGroups;
        std::unordered_map<uint32_t, std::vector<ShaderToggler::ToggleGroup*>> _computeShaderHashToToggleGroups;
        int _startValueFramecountCollectionPhase = FRAMECOUNT_COLLECTION_PHASE_DEFAULT;
        float _overlayOpacity = 0.2f;
        uint32_t _keyBindings[ARRAYSIZE(KeybindNames)];
        std::string _constHookType = "default";
        std::string _constHookCopyType = "gpu_readback";
        std::string _resourceShim = "none";
        bool _trackDescriptors = true;
        bool _preventRuntimeReload = false;
        std::filesystem::path _basePath;
        TabType _currentTab = TabType::TAB_NONE;

        std::vector<std::function<void(reshade::api::effect_runtime*, ShaderToggler::ToggleGroup*)>> _removalCallbacks;
    public:
        AddonUIData(ShaderToggler::ShaderManager* pixelShaderManager, ShaderToggler::ShaderManager* vertexShaderManager, ShaderToggler::ShaderManager* computeShaderManager, Shim::Constants::ConstantHandlerBase* constants, std::atomic_uint32_t* activeCollectorFrameCounter,
            std::vector<std::string>* techniques);
        std::unordered_map<int, ShaderToggler::ToggleGroup>& GetToggleGroups();
        const std::vector<ShaderToggler::ToggleGroup*>* GetToggleGroupsForPixelShaderHash(uint32_t hash);
        const std::vector<ShaderToggler::ToggleGroup*>* GetToggleGroupsForVertexShaderHash(uint32_t hash);
        const std::vector<ShaderToggler::ToggleGroup*>* GetToggleGroupsForComputeShaderHash(uint32_t hash);
        void UpdateToggleGroupsForShaderHashes();
        void AddDefaultGroup();
        const std::atomic_int& GetToggleGroupIdShaderEditing() const;
        void EndShaderEditing(bool acceptCollectedShaderHashes, ShaderToggler::ToggleGroup& groupEditing);
        void StartShaderEditing(ShaderToggler::ToggleGroup& groupEditing);
        void StartEffectEditing(ShaderToggler::ToggleGroup& groupEditing);
        void EndEffectEditing();
        void StartConstantEditing(ShaderToggler::ToggleGroup& groupEditing);
        void EndConstantEditing();
        void StopHuntingMode();
        void SetBasePath(const std::filesystem::path& basePath) { _basePath = basePath; };
        std::filesystem::path GetBasePath() { return _basePath; };
        void SaveShaderTogglerIniFile(const std::string& fileName = HASH_FILE_NAME);
        void LoadShaderTogglerIniFile(const std::string& fileName = HASH_FILE_NAME);
        std::atomic_int& GetToggleGroupIdShaderEditing() { return _toggleGroupIdShaderEditing; }
        std::atomic_int& GetToggleGroupIdEffectEditing() { return _toggleGroupIdEffectEditing; }
        std::atomic_int& GetToggleGroupIdConstantEditing() { return _toggleGroupIdConstantEditing; }
        std::atomic_uint& GetInvocationLocation() { return _invocationLocation; }
        std::atomic_uint& GetDescriptorIndex() { return _descriptorIndex; }
        void SetCurrentTabType(TabType type) { _currentTab = type; }
        TabType GetCurrentTabType() const { return _currentTab; }
        const std::vector<std::string>* GetAllTechniques() const;
        int* StartValueFramecountCollectionPhase() { return &_startValueFramecountCollectionPhase; }
        float* OverlayOpacity() { return &_overlayOpacity; }
        std::atomic_uint32_t* ActiveCollectorFrameCounter() { return _activeCollectorFrameCounter; }
        ShaderToggler::ShaderManager* GetPixelShaderManager() { return _pixelShaderManager; }
        ShaderToggler::ShaderManager* GetVertexShaderManager() { return _vertexShaderManager; }
        ShaderToggler::ShaderManager* GetComputeShaderManager() { return _computeShaderManager; }
        void SetConstantHandler(Shim::Constants::ConstantHandlerBase* handler) { _constantHandler = handler; }
        Shim::Constants::ConstantHandlerBase* GetConstantHandler() { return _constantHandler; }
        uint32_t GetKeybinding(Keybind keybind) const;
        const std::string& GetConstHookType() { return _constHookType; }
        const std::string& GetConstHookCopyType()  { return _constHookCopyType; }
        const std::string& GetResourceShim() { return _resourceShim; }
        void SetConstHookCopyType(std::string& copyType) { _constHookCopyType = copyType; }
        void SetResourceShim(std::string& shim) { _resourceShim = shim; }
        void SetKeybinding(Keybind keybind, uint32_t keys);
        const std::unordered_map<std::string, std::tuple<Shim::Constants::constant_type, std::vector<reshade::api::effect_uniform_variable>>>* GetRESTVariables() { return _constantHandler->GetRESTVariables(); };
        bool GetTrackDescriptors() const { return _trackDescriptors; }
        void SetTrackDescriptors(bool track) { _trackDescriptors = track; }
        void AddToggleGroupRemovalCallback(std::function<void(reshade::api::effect_runtime*, ShaderToggler::ToggleGroup*)> callback);
        void SignalToggleGroupRemoved(reshade::api::effect_runtime*, ShaderToggler::ToggleGroup*);
        bool GetPreventRuntimeReload() const { return _preventRuntimeReload; }
        void SetPreventRuntimeReload(bool reload) { _preventRuntimeReload = reload; }
    };
}
