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

#include "CDataFile.h"

namespace ShaderToggler
{
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
        void storeCollectedHashes(const std::unordered_set<uint32_t> pixelShaderHashes, const std::unordered_set<uint32_t> vertexShaderHashes);
        bool isBlockedVertexShader(uint32_t shaderHash) const;
        bool isBlockedPixelShader(uint32_t shaderHash) const;
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
        void setHistoryIndex(int32_t index) { _historyIndex = index; }
        int32_t getHistoryIndex() const { return _historyIndex; }
        bool isProvidingTextureBinding() const { return _isProvidingTextureBinding; }
        void setProvidingTextureBinding(bool isProvidingTextureBinding) { _isProvidingTextureBinding = isProvidingTextureBinding; }
        const std::string& getTextureBindingName() const { return _textureBindingName; }
        void setTextureBindingName(std::string textureBindingName) { _textureBindingName = textureBindingName; }
        bool getAllowAllTechniques() const { return _allowAllTechniques; }
        void setAllowAllTechniques(bool allowAllTechniques) { _allowAllTechniques = allowAllTechniques; }
        bool getExtractConstants() const { return _extractConstants; }
        void setExtractConstant(bool extract) { _extractConstants = extract; }
        bool getHasTechniqueExceptions() const { return _hasTechniqueExceptions; }
        void setHasTechniqueExceptions(bool exceptions) { _hasTechniqueExceptions = exceptions; }
        const std::unordered_map<string, tuple<uintptr_t, bool>>& GetVarOffsetMapping() const { return _varOffsetMapping; }
        bool SetVarMapping(uintptr_t, string&, bool);
        bool RemoveVarMapping(string&);

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
        int32_t _historyIndex;
        bool _isActive;				// true means the group is actively toggled (so the hashes have to be hidden.
        bool _isEditing;			// true means the group is actively edited (name, key)
        bool _allowAllTechniques;	// true means all techniques are allowed, regardless of preferred techniques.
        bool _isProvidingTextureBinding;
        bool _extractConstants;
        bool _hasTechniqueExceptions; // _preferredTechniques are handled as exception to _allowAllTechniques
        std::string _textureBindingName;
        std::unordered_set<std::string> _preferredTechniques;
        std::unordered_map<string, tuple<uintptr_t, bool>> _varOffsetMapping;
    };
}
