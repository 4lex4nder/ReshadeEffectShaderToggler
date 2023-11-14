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

#include <sstream>
#include "stdafx.h"
#include "ToggleGroup.h"

using namespace std;

namespace ShaderToggler
{
    ToggleGroup::ToggleGroup(string name, int id)
    {
        _name = name.size() > 0 ? name : "Default";
        _id = id;
        _isActive = false;
        _isEditing = false;
        _allowAllTechniques = true;
        _isProvidingTextureBinding = false;
        _textureBindingName = "";
        _hasTechniqueExceptions = false;
        _extractConstants = false;
        _extractResourceViews = false;
        _matchSwapchainResolution = true;
        _copyTextureBinding = false;
        _previewClearAlpha = true;
        _tonemapHDRtoSDRtoHDR = false;
        _preserveAlpha = false;
    }


    ToggleGroup::ToggleGroup() : ToggleGroup("Default", 0)
    {
    }


    int ToggleGroup::getNewGroupId()
    {
        static atomic_int s_groupId = 0;

        ++s_groupId;
        return s_groupId;
    }


    void ToggleGroup::storeCollectedHashes(const unordered_set<uint32_t> pixelShaderHashes, const unordered_set<uint32_t> vertexShaderHashes, const unordered_set<uint32_t> computeShaderHashes)
    {
        _vertexShaderHashes.clear();
        _pixelShaderHashes.clear();
        _computeShaderHashes.clear();

        for (const auto hash : vertexShaderHashes)
        {
            _vertexShaderHashes.emplace(hash);
        }
        for (const auto hash : pixelShaderHashes)
        {
            _pixelShaderHashes.emplace(hash);
        }
        for (const auto hash : computeShaderHashes)
        {
            _computeShaderHashes.emplace(hash);
        }
    }


    bool ToggleGroup::isBlockedVertexShader(uint32_t shaderHash) const
    {
        return _isActive && (_vertexShaderHashes.contains(shaderHash));
    }


    bool ToggleGroup::isBlockedPixelShader(uint32_t shaderHash) const
    {
        return _isActive && (_pixelShaderHashes.contains(shaderHash));
    }


    bool ToggleGroup::isBlockedComputeShader(uint32_t shaderHash) const
    {
        return _isActive && (_computeShaderHashes.contains(shaderHash));
    }


    void ToggleGroup::clearHashes()
    {
        _pixelShaderHashes.clear();
        _vertexShaderHashes.clear();
        _computeShaderHashes.clear();
    }


    void ToggleGroup::setName(string newName)
    {
        if (newName.size() <= 0)
        {
            return;
        }
        _name = newName;
    }


    bool ToggleGroup::SetVarMapping(uintptr_t offset, string& variable, bool prev)
    {
        _varOffsetMapping.emplace(variable, make_tuple(offset, prev));

        return true; // do some sanity checking?
    }


    bool ToggleGroup::RemoveVarMapping(string& variable)
    {
        _varOffsetMapping.erase(variable);

        return true; // do some sanity checking?
    }


    void ToggleGroup::saveState(CDataFile& iniFile, int groupCounter) const
    {
        const string sectionRoot = "Group" + std::to_string(groupCounter);
        const string vertexHashesCategory = sectionRoot + "_VertexShaders";
        const string pixelHashesCategory = sectionRoot + "_PixelShaders";
        const string computeHashesCategory = sectionRoot + "_ComputeShaders";
        const string constantsCategory = sectionRoot + "_Constants";

        int counter = 0;
        for (const auto hash : _vertexShaderHashes)
        {
            iniFile.SetUInt("ShaderHash" + std::to_string(counter), hash, "", vertexHashesCategory);
            counter++;
        }
        iniFile.SetUInt("AmountHashes", counter, "", vertexHashesCategory);

        counter = 0;
        for (const auto hash : _pixelShaderHashes)
        {
            iniFile.SetUInt("ShaderHash" + std::to_string(counter), hash, "", pixelHashesCategory);
            counter++;
        }
        iniFile.SetUInt("AmountHashes", counter, "", pixelHashesCategory);

        counter = 0;
        for (const auto hash : _computeShaderHashes)
        {
            iniFile.SetUInt("ShaderHash" + std::to_string(counter), hash, "", computeHashesCategory);
            counter++;
        }
        iniFile.SetUInt("AmountHashes", counter, "", computeHashesCategory);

        counter = 0;
        for (const auto& [varName, varData] : _varOffsetMapping)
        {
            const auto& [varOffset, varUsePref] = varData;
            iniFile.SetUInt("Offset" + std::to_string(counter), static_cast<uint32_t>(varOffset), "", constantsCategory);
            iniFile.SetValue("Variable" + std::to_string(counter), varName, "", constantsCategory);
            iniFile.SetBool("UsePreviousValue" + std::to_string(counter), varUsePref, "", constantsCategory);
            counter++;
        }
        iniFile.SetUInt("AmountConstants", counter, "", constantsCategory);

        iniFile.SetValue("Name", _name, "", sectionRoot);
        iniFile.SetUInt("ToggleKey", _keybind, "", sectionRoot);
        iniFile.SetBool("Active", _isActive, "", sectionRoot);

        stringstream ss("");
        bool firstElement = true;
        for (const auto& el : _preferredTechniques)
        {
            if (!firstElement)
            {
                ss << ",";
            }

            ss << el;

            firstElement = false;
        }
        iniFile.SetUInt("RenderTargetIndex", _rtIndex, "", sectionRoot);
        iniFile.SetUInt("InvocationLocation", _invocationLocation, "", sectionRoot);
        iniFile.SetUInt("MatchSwapchainResolutionOnly", _matchSwapchainResolution, "", sectionRoot);
        iniFile.SetBool("RequeueAfterRTMatchingFailure", _requeueAfterRTMatchingFailure, "", sectionRoot);

        iniFile.SetValue("Techniques", ss.str(), "", sectionRoot);
        iniFile.SetBool("AllowAllTechniques", _allowAllTechniques, "", sectionRoot);
        iniFile.SetBool("TechniqueExceptions", _hasTechniqueExceptions, "", sectionRoot);

        iniFile.SetBool("ProvideTextureBinding", _isProvidingTextureBinding, "", sectionRoot);
        iniFile.SetValue("TextureBindingName", _textureBindingName, "", sectionRoot);
        iniFile.SetBool("ClearTextureBindings", _clearBindings, "", sectionRoot);
        iniFile.SetBool("CopyTextureBinding", _copyTextureBinding, "", sectionRoot);

        iniFile.SetBool("ExtractConstants", _extractConstants, "", sectionRoot);
        iniFile.SetUInt("ConstantPipelineSlot", _cbSlotIndex, "", sectionRoot);
        iniFile.SetUInt("ConstantDescriptorIndex", _cbDescIndex, "", sectionRoot);
        iniFile.SetBool("ConstantPushMode", _cbModePush, "", sectionRoot);
        iniFile.SetUInt("ConstantShaderStage", _cbShaderStage, "", sectionRoot);

        iniFile.SetBool("ExtractSRVs", _extractResourceViews, "", sectionRoot);
        iniFile.SetUInt("SRVPipelineSlot", _bindingSrvSlotIndex, "", sectionRoot);
        iniFile.SetUInt("SRVDescriptorIndex", _bindingSrvDescIndex, "", sectionRoot);
        iniFile.SetUInt("SRVShaderStage", _bindingSrvShaderStage, "", sectionRoot);
        iniFile.SetUInt("BindingRenderTargetIndex", _bindingRTIndex, "", sectionRoot);
        iniFile.SetUInt("BindingInvocationLocation", _bindingInvocationLocation, "", sectionRoot);
        iniFile.SetUInt("BindingMatchSwapchainResolutionOnly", _bindingMatchSwapchainResolution, "", sectionRoot);
        iniFile.SetBool("ClearPreviewAlpha", _previewClearAlpha, "", sectionRoot);
        iniFile.SetBool("TonemapHDRtoSDRtoHDR", _tonemapHDRtoSDRtoHDR, "", sectionRoot);
        iniFile.SetBool("PreserveTargetAlphaChannel", _preserveAlpha, "", sectionRoot);
        iniFile.SetBool("FlipBuffer", _flipBuffer, "", sectionRoot);
    }


    void ToggleGroup::loadState(CDataFile& iniFile, int groupCounter)
    {
        if (groupCounter < 0)
        {
            int amount = iniFile.GetInt("AmountHashes", "PixelShaders");
            for (int i = 0; i < amount; i++)
            {
                uint32_t hash = iniFile.GetUInt("ShaderHash" + std::to_string(i), "PixelShaders");
                if (hash != UINT_MAX)
                {
                    _pixelShaderHashes.emplace(hash);
                }
            }
            amount = iniFile.GetInt("AmountHashes", "VertexShaders");
            for (int i = 0; i < amount; i++)
            {
                uint32_t hash = iniFile.GetUInt("ShaderHash" + std::to_string(i), "VertexShaders");
                if (hash != UINT_MAX)
                {
                    _vertexShaderHashes.emplace(hash);
                }
            }
            amount = iniFile.GetInt("AmountHashes", "ComputeShaders");
            for (int i = 0; i < amount; i++)
            {
                uint32_t hash = iniFile.GetUInt("ShaderHash" + std::to_string(i), "ComputeShaders");
                if (hash != UINT_MAX)
                {
                    _computeShaderHashes.emplace(hash);
                }
            }

            // done
            return;
        }

        const string sectionRoot = "Group" + std::to_string(groupCounter);
        const string vertexHashesCategory = sectionRoot + "_VertexShaders";
        const string pixelHashesCategory = sectionRoot + "_PixelShaders";
        const string computeHashesCategory = sectionRoot + "_ComputeShaders";
        const string constantsCategory = sectionRoot + "_Constants";

        int amountShaders = iniFile.GetInt("AmountHashes", vertexHashesCategory);
        for (int i = 0; i < amountShaders; i++)
        {
            uint32_t hash = iniFile.GetUInt("ShaderHash" + std::to_string(i), vertexHashesCategory);
            if (hash != UINT_MAX)
            {
                _vertexShaderHashes.emplace(hash);
            }
        }

        amountShaders = iniFile.GetInt("AmountHashes", pixelHashesCategory);
        for (int i = 0; i < amountShaders; i++)
        {
            uint32_t hash = iniFile.GetUInt("ShaderHash" + std::to_string(i), pixelHashesCategory);
            if (hash != UINT_MAX)
            {
                _pixelShaderHashes.emplace(hash);
            }
        }

        amountShaders = iniFile.GetInt("AmountHashes", computeHashesCategory);
        for (int i = 0; i < amountShaders; i++)
        {
            uint32_t hash = iniFile.GetUInt("ShaderHash" + std::to_string(i), computeHashesCategory);
            if (hash != UINT_MAX)
            {
                _computeShaderHashes.emplace(hash);
            }
        }

        int amountConstants = iniFile.GetInt("AmountConstants", constantsCategory);
        for (int i = 0; i < amountConstants; i++)
        {
            uint32_t offset = iniFile.GetUInt("Offset" + std::to_string(i), constantsCategory);
            string varName = iniFile.GetString("Variable" + std::to_string(i), constantsCategory);
            bool prevValue = iniFile.GetBool("UsePreviousValue" + std::to_string(i), constantsCategory);
            if (offset != UINT_MAX && varName.size() > 0)
            {
                _varOffsetMapping.emplace(varName, make_tuple(offset, prevValue));
            }
        }

        _name = iniFile.GetValue("Name", sectionRoot);
        if (_name.size() <= 0)
        {
            _name = "Default";
        }
        const uint32_t toggleKeyValue = iniFile.GetUInt("ToggleKey", sectionRoot);
        if (toggleKeyValue == UINT_MAX)
        {
            _keybind = VK_CAPITAL;
        }
        else
        {
            _keybind = toggleKeyValue;
        }

        _isActive = iniFile.GetBool("Active", sectionRoot);

        const int32_t invocationLocation = iniFile.GetUInt("InvocationLocation", sectionRoot);
        if (invocationLocation != UINT_MAX)
        {
            _invocationLocation = invocationLocation;
        }
        else
        {
            _invocationLocation = 0;
        }

        _matchSwapchainResolution = iniFile.GetUInt("MatchSwapchainResolutionOnly", sectionRoot);
        if (_matchSwapchainResolution == UINT_MAX)
        {
            _matchSwapchainResolution = SWAPCHAIN_MATCH_MODE_RESOLUTION; //fallback to effect invocation location when loading old configs
        }

        _requeueAfterRTMatchingFailure = iniFile.GetBoolOrDefault("RequeueAfterRTMatchingFailure", sectionRoot, false);

        string techniques = iniFile.GetString("Techniques", sectionRoot);
        if (techniques.size() > 0) {
            stringstream ss(techniques);

            while (ss.good())
            {
                string substr;
                getline(ss, substr, ',');
                _preferredTechniques.insert(substr);
            }
        }

        _allowAllTechniques = iniFile.GetBool("AllowAllTechniques", sectionRoot);
        _hasTechniqueExceptions = iniFile.GetBool("TechniqueExceptions", sectionRoot);

        _isProvidingTextureBinding = iniFile.GetBool("ProvideTextureBinding", sectionRoot);
        _clearBindings = iniFile.GetBoolOrDefault("ClearTextureBindings", sectionRoot, true);
        _textureBindingName = iniFile.GetString("TextureBindingName", sectionRoot);
        _copyTextureBinding = iniFile.GetBoolOrDefault("CopyTextureBinding", sectionRoot, true);

        _extractConstants = iniFile.GetBool("ExtractConstants", sectionRoot);

        uint32_t slotIndex = iniFile.GetUInt("ConstantPipelineSlot", sectionRoot);
        if (slotIndex != UINT_MAX)
        {
            _cbSlotIndex = slotIndex;
        }
        else
        {
            _cbSlotIndex = 2;
        }

        uint32_t descIndex = iniFile.GetUInt("ConstantDescriptorIndex", sectionRoot);
        if (descIndex != UINT_MAX)
        {
            _cbDescIndex = descIndex;
        }
        else
        {
            _cbDescIndex = 0;
        }

        bool _cbPushMode = iniFile.GetBoolOrDefault("ConstantPushMode", sectionRoot, false);

        uint32_t shaderStage = iniFile.GetUInt("ConstantShaderStage", sectionRoot);
        if (shaderStage != UINT_MAX && shaderStage < 3)
        {
            _cbShaderStage = shaderStage;
        }
        else
        {
            _cbShaderStage = 0;
        }

        _extractResourceViews = iniFile.GetBool("ExtractSRVs", sectionRoot);

        uint32_t srvSlotIndex = iniFile.GetUInt("SRVPipelineSlot", sectionRoot);
        if (srvSlotIndex != UINT_MAX)
        {
            _bindingSrvSlotIndex = srvSlotIndex;
        }
        else
        {
            _bindingSrvSlotIndex = 1;
        }

        uint32_t srvDescIndex = iniFile.GetUInt("SRVDescriptorIndex", sectionRoot);
        if (srvDescIndex != UINT_MAX)
        {
            _bindingSrvDescIndex = srvDescIndex;
        }
        else
        {
            _bindingSrvDescIndex = 0;
        }

        uint32_t srvShaderStage = iniFile.GetUInt("SRVShaderStage", sectionRoot);
        if (srvShaderStage != UINT_MAX && srvShaderStage < 3)
        {
            _bindingSrvShaderStage = srvShaderStage;
        }
        else
        {
            _bindingSrvShaderStage = 0;
        }

        uint32_t rtvIndex = iniFile.GetUInt("RenderTargetIndex", sectionRoot);
        if (rtvIndex != UINT_MAX)
        {
            _rtIndex = rtvIndex;
        }
        else
        {
            _rtIndex = _cbDescIndex; //rt index and cb descriptor index were previously shared. Fallback for older configs
        }

        uint32_t bindingRTIndex = iniFile.GetUInt("BindingRenderTargetIndex", sectionRoot);
        if (bindingRTIndex != UINT_MAX)
        {
            _bindingRTIndex = bindingRTIndex;
        }
        else
        {
            _bindingRTIndex = _cbDescIndex; //rt index and cb descriptor index were previously shared. Fallback for older configs
        }

        uint32_t bindingInvocationLocation = iniFile.GetUInt("BindingInvocationLocation", sectionRoot);
        if (bindingInvocationLocation != UINT_MAX)
        {
            _bindingInvocationLocation = bindingInvocationLocation;
        }
        else
        {
            _bindingInvocationLocation = _invocationLocation; //fallback to effect invocation location when loading old configs
        }

        _bindingMatchSwapchainResolution = iniFile.GetUInt("BindingMatchSwapchainResolutionOnly", sectionRoot);
        if(_bindingMatchSwapchainResolution == UINT_MAX)
        {
            _bindingMatchSwapchainResolution = _matchSwapchainResolution; //fallback to effect invocation location when loading old configs
        }

        _previewClearAlpha = iniFile.GetBoolOrDefault("ClearPreviewAlpha", sectionRoot, true);

        _tonemapHDRtoSDRtoHDR = iniFile.GetBoolOrDefault("TonemapHDRtoSDRtoHDR", sectionRoot, false);

        _preserveAlpha = iniFile.GetBoolOrDefault("PreserveTargetAlphaChannel", sectionRoot, false);

        _flipBuffer = iniFile.GetBoolOrDefault("FlipBuffer", sectionRoot, false);
    }
}
