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
#include <reshade.hpp>
#include "ShaderManager.h"
#include "CDataFile.h"
#include "ToggleGroup.h"
#include "ConstantHandler.h"

#define FRAMECOUNT_COLLECTION_PHASE_DEFAULT 250;
#define HASH_FILE_NAME	"ReshadeEffectShaderToggler.ini"

using namespace reshade::api;
using namespace ShaderToggler;
using namespace ConstantFeedback;

namespace AddonImGui
{
	class AddonUIData
	{
	private:
		ShaderManager* _pixelShaderManager;
		ShaderManager* _vertexShaderManager;
		ConstantHandler* _constantHandler;
		atomic_uint32_t* _activeCollectorFrameCounter;
		vector<string>* _allTechniques;
		unordered_map<string, tuple<constant_type, vector<effect_uniform_variable>>>* _constants;
		KeyData _keyCollector;
		atomic_int _historyIndexSelection = 0;
		atomic_int _toggleGroupIdShaderEditing = -1;
		atomic_int _toggleGroupIdEffectEditing = -1;
		atomic_int _toggleGroupIdKeyBindingEditing = -1;
		atomic_int _toggleGroupIdConstantEditing = -1;
		std::unordered_map<int, ToggleGroup> _toggleGroups;
		int _startValueFramecountCollectionPhase = FRAMECOUNT_COLLECTION_PHASE_DEFAULT;
		float _overlayOpacity = 1.0f;
	public:
		AddonUIData(ShaderManager* pixelShaderManager, ShaderManager* vertexShaderManager, ConstantHandler* constants, atomic_uint32_t* activeCollectorFrameCounter,
			vector<string>* techniques, unordered_map<string, tuple<constant_type, vector<effect_uniform_variable>>>*);
		std::unordered_map<int, ToggleGroup>& GetToggleGroups();
		void AddDefaultGroup();
		const atomic_int& GetToggleGroupIdShaderEditing() const;
		void EndShaderEditing(bool acceptCollectedShaderHashes, ToggleGroup& groupEditing);
		void StartShaderEditing(ToggleGroup& groupEditing);
		void StartEffectEditing(ToggleGroup& groupEditing);
		void EndEffectEditing();
		void StartConstantEditing(ToggleGroup& groupEditing);
		void EndConstantEditing();
		void EndKeyBindingEditing(bool acceptCollectedBinding, ToggleGroup& groupEditing);
		void StartKeyBindingEditing(ToggleGroup& groupEditing);
		void StopHuntingMode();
		void SaveShaderTogglerIniFile();
		void LoadShaderTogglerIniFile();
		void ResetKeyBinding(ToggleGroup& groupgroupEditing);
		atomic_int& GetToggleGroupIdKeyBindingEditing() { return _toggleGroupIdKeyBindingEditing; }
		atomic_int& GetToggleGroupIdShaderEditing() { return _toggleGroupIdShaderEditing; }
		atomic_int& GetToggleGroupIdEffectEditing() { return _toggleGroupIdEffectEditing; }
		atomic_int& GetToggleGroupIdConstantEditing() { return _toggleGroupIdConstantEditing; }
		atomic_int& GetHistoryIndex() { return _historyIndexSelection; }
		KeyData& GetKeyCollector() { return _keyCollector; }
		const vector<string>* GetAllTechniques() const;
		int* StartValueFramecountCollectionPhase() { return &_startValueFramecountCollectionPhase; }
		float* OverlayOpacity() { return &_overlayOpacity; }
		atomic_uint32_t* ActiveCollectorFrameCounter() { return _activeCollectorFrameCounter; }
		ShaderManager* GetPixelShaderManager() { return _pixelShaderManager; }
		ShaderManager* GetVertexShaderManager() { return _vertexShaderManager; }
		ConstantHandler* GetConstantHandler() { return _constantHandler; }
		const unordered_map<string, tuple<constant_type, vector<effect_uniform_variable>>>* GetRESTVariables() { return _constants; };
		reshade::api::format cFormat;
	};
}