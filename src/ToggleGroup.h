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

#include <string>
#include <unordered_set>
#include <unordered_map>

#include "reshade.hpp"
#include "CDataFile.h"

namespace ShaderToggler
{
    enum DescriptorCycle
    {
        CYCLE_NONE,
        CYCLE_UP,
        CYCLE_DOWN
    };

    enum SwapChainMatchMode : uint32_t
    {
        SWAPCHAIN_MATCH_MODE_RESOLUTION = 0,
        SWAPCHAIN_MATCH_MODE_ASPECT_RATIO = 1,
        SWAPCHAIN_MATCH_MODE_EXTENDED_ASPECT_RATIO = 2,
        SWAPCHAIN_MATCH_MODE_NONE = 3
    };

    class ToggleGroup
    {
    public:
        ToggleGroup(std::string name, int Id);
        ToggleGroup();

        static int getNewGroupId();

        void setToggleKey(uint32_t keybind) { _keybind = keybind; }
        void setName(std::string newName);
        /// <summary>
        /// Writes the shader hashes, name and toggle key to the ini file specified, using a Group + groupCounter section.
        /// </summary>
        /// <param name="iniFile"></param>
        /// <param name="groupCounter"></param>
        void saveState(CDataFile& iniFile, int groupCounter) const;
        /// <summary>
        /// Loads the shader hashes, name and toggle key from the ini file specified, using a Group + groupCounter section.
        /// </summary>
        /// <param name="iniFile"></param>
        /// <param name="groupCounter">if -1, the ini file is in the pre-1.0 format</param>
        void loadState(CDataFile& iniFile, int groupCounter);
        void storeCollectedHashes(const std::unordered_set<uint32_t> pixelShaderHashes, const std::unordered_set<uint32_t> vertexShaderHashes, const std::unordered_set<uint32_t> computeShaderHashes);
        bool isBlockedVertexShader(uint32_t shaderHash) const;
        bool isBlockedPixelShader(uint32_t shaderHash) const;
        bool isBlockedComputeShader(uint32_t shaderHash) const;
        void clearHashes();

        void toggleActive() { _isActive = !_isActive; }
        void setEditing(bool isEditing) { _isEditing = isEditing; }

        uint32_t getToggleKey() { return _keybind; }
        std::string getName() { return _name; }
        bool isActive() const { return _isActive; }
        bool isEditing() { return _isEditing; }
        bool isEmpty() const { return _vertexShaderHashes.size() <= 0 && _pixelShaderHashes.size() <= 0; }
        int getId() const { return _id; }
        const std::unordered_set<std::string>& preferredTechniques() const { return _preferredTechniques; }
        void setPreferredTechniques(std::unordered_set<std::string> techniques) { _preferredTechniques = techniques; }
        std::unordered_set<uint32_t> getPixelShaderHashes() const { return _pixelShaderHashes; }
        std::unordered_set<uint32_t> getVertexShaderHashes() const { return _vertexShaderHashes; }
        std::unordered_set<uint32_t> getComputeShaderHashes() const { return _computeShaderHashes; }
        void setInvocationLocation(uint32_t location) { _invocationLocation = location; }
        uint32_t getInvocationLocation() const { return _invocationLocation; }
        void setBindingInvocationLocation(uint32_t location) { _bindingInvocationLocation = location; }
        uint32_t getBindingInvocationLocation() const { return _bindingInvocationLocation; }
        void setCBSlotIndex(uint32_t index) { _cbSlotIndex = index; }
        uint32_t getCBSlotIndex() const { return _cbSlotIndex; }
        void setCBDescriptorIndex(uint32_t index) { _cbDescIndex = index; }
        uint32_t getCBDescriptorIndex() const { return _cbDescIndex; }
        bool getCBIsPushMode() const { return _cbModePush; }
        void setCBIsPushMode(bool isPushMode) { _cbModePush = isPushMode; }
        void setRenderTargetIndex(uint32_t index) { _rtIndex = index; }
        uint32_t getRenderTargetIndex() const { return _rtIndex; }
        bool isProvidingTextureBinding() const { return _isProvidingTextureBinding; }
        void setProvidingTextureBinding(bool isProvidingTextureBinding) { _isProvidingTextureBinding = isProvidingTextureBinding; }
        const std::string& getTextureBindingName() const { return _textureBindingName; }
        void setTextureBindingName(std::string textureBindingName) { _textureBindingName = textureBindingName; }
        bool getClearBindings() { return _clearBindings; }
        void setClearBindings(bool clear) { _clearBindings = clear; }
        bool getAllowAllTechniques() const { return _allowAllTechniques; }
        void setAllowAllTechniques(bool allowAllTechniques) { _allowAllTechniques = allowAllTechniques; }
        bool getExtractConstants() const { return _extractConstants; }
        void setExtractConstant(bool extract) { _extractConstants = extract; }
        uint32_t getCBShaderStage() const { return _cbShaderStage; }
        void setCBShaderStage(uint32_t shaderStage) { _cbShaderStage = shaderStage; }
        bool getExtractResourceViews() const { return _extractResourceViews; }
        void setExtractResourceViews(bool extract) { _extractResourceViews = extract; }
        void setBindingSRVSlotIndex(uint32_t index) { _bindingSrvSlotIndex = index; }
        uint32_t getBindingSRVSlotIndex() const { return _bindingSrvSlotIndex; }
        void setBindingSRVDescriptorIndex(uint32_t index) { _bindingSrvDescIndex = index; }
        uint32_t getBindingSRVDescriptorIndex() const { return _bindingSrvDescIndex; }
        uint32_t getSRVShaderStage() const { return _bindingSrvShaderStage; }
        void setSRVShaderStage(uint32_t shaderStage) { _bindingSrvShaderStage = shaderStage; }
        void setBindingRenderTargetIndex(uint32_t index) { _bindingRTIndex = index; }
        uint32_t getBindingRenderTargetIndex() const { return _bindingRTIndex; }
        bool getHasTechniqueExceptions() const { return _hasTechniqueExceptions; }
        void setHasTechniqueExceptions(bool exceptions) { _hasTechniqueExceptions = exceptions; }
        uint32_t getMatchSwapchainResolution() const { return _matchSwapchainResolution; }
        void setMatchSwapchainResolution(uint32_t match) { _matchSwapchainResolution = match; }
        uint32_t getBindingMatchSwapchainResolution() const { return _bindingMatchSwapchainResolution; }
        void setBindingMatchSwapchainResolution(uint32_t match) { _bindingMatchSwapchainResolution = match; }
        bool getRequeueAfterRTMatchingFailure() const { return _requeueAfterRTMatchingFailure; }
        void setRequeueAfterRTMatchingFailure(bool requeue) { _requeueAfterRTMatchingFailure = requeue; }
        bool getCopyTextureBinding() const { return _copyTextureBinding; }
        void setCopyTextureBinding(bool copy) { _copyTextureBinding = copy; }
        const std::unordered_map<std::string, std::tuple<uintptr_t, bool>>& GetVarOffsetMapping() const { return _varOffsetMapping; }
        bool SetVarMapping(uintptr_t, std::string&, bool);
        bool RemoveVarMapping(std::string&);
        bool getClearPreviewAlpha() const { return _previewClearAlpha; }
        void setClearPreviewAlpha(bool previewClearAlpha) { _previewClearAlpha = previewClearAlpha; }
        bool getToneMap() const { return _tonemapHDRtoSDRtoHDR; }
        void setToneMap(bool tonemap) { _tonemapHDRtoSDRtoHDR = tonemap; }
        bool getPreserveAlpha() const { return _preserveAlpha; }
        void setPreserveAlpha(bool alpha) { _preserveAlpha = alpha; }
        const reshade::api::resource_desc& getTargetBufferDescription() const { return _bufferDesc; }
        void setTargetBufferDescription(const reshade::api::resource_desc& desc) { _bufferDesc = desc; }
        bool getRecreateBuffer() const { return _recreateBuffer; }
        void setRecreateBuffer(bool recreate) { _recreateBuffer = recreate; }
        void dispatchCBCycle(DescriptorCycle cycle) { _cbCycle = cycle; }
        DescriptorCycle consumeCBCycle() 
        { 
            DescriptorCycle ret = _cbCycle;
            _cbCycle = CYCLE_NONE;
            return ret; 
        }
        void dispatchSRVCycle(DescriptorCycle cycle) { _srvCycle = cycle; }
        DescriptorCycle consumeSRVCycle()
        {
            DescriptorCycle ret = _srvCycle;
            _srvCycle = CYCLE_NONE;
            return ret;
        }
        void dispatchRTCycle(DescriptorCycle cycle) { _rtCycle = cycle; }
        DescriptorCycle consumeRTCycle()
        {
            DescriptorCycle ret = _rtCycle;
            _rtCycle = CYCLE_NONE;
            return ret;
        }

        bool operator==(const ToggleGroup& rhs)
        {
            return getId() == rhs.getId();
        }

    private:
        int _id;
        std::string	_name;
        uint32_t _keybind;
        std::unordered_set<uint32_t> _vertexShaderHashes;
        std::unordered_set<uint32_t> _pixelShaderHashes;
        std::unordered_set<uint32_t> _computeShaderHashes;
        uint32_t _invocationLocation = 0;
        uint32_t _rtIndex = 0;
        uint32_t _cbSlotIndex = 2;
        uint32_t _cbDescIndex = 0;
        uint32_t _cbShaderStage = 0;
        uint32_t _bindingInvocationLocation = 0;
        uint32_t _bindingRTIndex = 0;
        uint32_t _bindingSrvSlotIndex = 1;
        uint32_t _bindingSrvDescIndex = 0;
        uint32_t _bindingSrvShaderStage = 0;
        bool _isActive;				// true means the group is actively toggled (so the hashes have to be hidden.
        bool _isEditing;			// true means the group is actively edited (name, key)
        bool _allowAllTechniques;	// true means all techniques are allowed, regardless of preferred techniques.
        bool _isProvidingTextureBinding;
        bool _copyTextureBinding;
        bool _extractConstants;
        bool _extractResourceViews;
        bool _clearBindings;
        bool _previewClearAlpha = true;
        bool _hasTechniqueExceptions; // _preferredTechniques are handled as exception to _allowAllTechniques
        bool _tonemapHDRtoSDRtoHDR = false;
        bool _preserveAlpha = false;
        uint32_t _matchSwapchainResolution = SWAPCHAIN_MATCH_MODE_RESOLUTION;
        uint32_t _bindingMatchSwapchainResolution = SWAPCHAIN_MATCH_MODE_RESOLUTION;
        bool _requeueAfterRTMatchingFailure;
        bool _cbModePush = false;
        std::string _textureBindingName;
        std::unordered_set<std::string> _preferredTechniques;
        std::unordered_map<std::string, std::tuple<uintptr_t, bool>> _varOffsetMapping;
        DescriptorCycle _cbCycle;
        DescriptorCycle _srvCycle;
        DescriptorCycle _rtCycle;
        reshade::api::resource_desc _bufferDesc = {};
        bool _recreateBuffer = false;
    };
}
