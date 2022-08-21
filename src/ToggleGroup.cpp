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
#include "KeyData.h"

namespace ShaderToggler
{
	ToggleGroup::ToggleGroup(std::string name, int id): _id(id), _isActive(false), _isEditing(false), _allowAllTechniques(false),
		_isProvidingTextureBinding(false), _textureBindingName("")
	{
		_name = name.size() > 0 ? name : "Default";
	}


	ToggleGroup::ToggleGroup(): _name(""), _id(0), _isActive(false), _isEditing(false), _allowAllTechniques(false),
		_isProvidingTextureBinding(false), _textureBindingName("")
	{

	}


	int ToggleGroup::getNewGroupId()
	{
		static atomic_int s_groupId = 0;

		++s_groupId;
		return s_groupId;
	}


	void ToggleGroup::setToggleKey(uint8_t newKeyValue, bool shiftRequired, bool altRequired, bool ctrlRequired)
	{
		_keyData.setKey(newKeyValue, shiftRequired, altRequired, ctrlRequired);
	}


	void ToggleGroup::setToggleKey(KeyData newData)
	{
		_keyData = newData;
	}


	void ToggleGroup::storeCollectedHashes(const std::unordered_set<uint32_t> pixelShaderHashes, const std::unordered_set<uint32_t> vertexShaderHashes)
	{
		_vertexShaderHashes.clear();
		_pixelShaderHashes.clear();

		for(const auto hash : vertexShaderHashes)
		{
			_vertexShaderHashes.emplace(hash);
		}
		for(const auto hash : pixelShaderHashes)
		{
			_pixelShaderHashes.emplace(hash);
		}
	}


	bool ToggleGroup::isBlockedVertexShader(uint32_t shaderHash)
	{
		return _isActive && (_vertexShaderHashes.count(shaderHash)==1);
	}


	bool ToggleGroup::isBlockedPixelShader(uint32_t shaderHash)
	{
		return _isActive && (_pixelShaderHashes.count(shaderHash)==1);
	}


	void ToggleGroup::clearHashes()
	{
		_pixelShaderHashes.clear();
		_vertexShaderHashes.clear();
	}


	void ToggleGroup::setName(std::string newName)
	{
		if(newName.size()<=0)
		{
			return;
		}
		_name = newName;
	}


	bool ToggleGroup::SetVarMapping(uintptr_t offset, string& variable)
	{
		_varOffsetMapping.emplace(variable, offset);

		return true; // do some sanity checking?
	}


	bool ToggleGroup::RemoveVarMapping(string& variable)
	{
		_varOffsetMapping.erase(variable);

		return true; // do some sanity checking?
	}


	void ToggleGroup::saveState(CDataFile& iniFile, int groupCounter) const
	{
		const std::string sectionRoot = "Group" + std::to_string(groupCounter);
		const std::string vertexHashesCategory = sectionRoot + "_VertexShaders";
		const std::string pixelHashesCategory = sectionRoot + "_PixelShaders";
		const std::string constantsCategory = sectionRoot + "_Constants";

		int counter = 0;
		for(const auto hash: _vertexShaderHashes)
		{
			iniFile.SetUInt("ShaderHash" + std::to_string(counter), hash, "", vertexHashesCategory);
			counter++;
		}
		iniFile.SetUInt("AmountHashes", counter, "", vertexHashesCategory);

		counter=0;
		for(const auto hash: _pixelShaderHashes)
		{
			iniFile.SetUInt("ShaderHash" + std::to_string(counter), hash, "", pixelHashesCategory);
			counter++;
		}
		iniFile.SetUInt("AmountHashes", counter, "", pixelHashesCategory);

		counter = 0;
		for (const auto var : _varOffsetMapping)
		{
			iniFile.SetUInt("Offset" + std::to_string(counter), var.second, "", constantsCategory);
			iniFile.SetValue("Variable" + std::to_string(counter), var.first, "", constantsCategory);
			counter++;
		}
		iniFile.SetUInt("AmountConstants", counter, "", constantsCategory);

		iniFile.SetValue("Name", _name, "", sectionRoot);
		iniFile.SetUInt("ToggleKey", _keyData.getKeyForIniFile(), "", sectionRoot);
		iniFile.SetBool("Active", _isActive, "", sectionRoot);

		std::stringstream ss("");
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
		iniFile.SetValue("Techniques", ss.str(), "", sectionRoot);
		iniFile.SetBool("AllowAllTechniques", _allowAllTechniques, "", sectionRoot);

		iniFile.SetInt("HistoryIndex", _historyIndex, "", sectionRoot);

		iniFile.SetBool("ProvideTextureBinding", _isProvidingTextureBinding, "", sectionRoot);
		iniFile.SetValue("TextureBindingName", _textureBindingName, "", sectionRoot);

		iniFile.SetBool("ExtractConstants", _extractConstants, "", sectionRoot);
	}


	void ToggleGroup::loadState(CDataFile& iniFile, int groupCounter)
	{
		if(groupCounter<0)
		{
			int amount = iniFile.GetInt("AmountHashes", "PixelShaders");
			for(int i = 0;i< amount;i++)
			{
				uint32_t hash = iniFile.GetUInt("ShaderHash" + std::to_string(i), "PixelShaders");
				if(hash!=UINT_MAX)
				{
					_pixelShaderHashes.emplace(hash);
				}
			}
			amount = iniFile.GetInt("AmountHashes", "VertexShaders");
			for(int i = 0;i< amount;i++)
			{
				uint32_t hash = iniFile.GetUInt("ShaderHash" + std::to_string(i), "VertexShaders");
				if(hash!=UINT_MAX)
				{
					_vertexShaderHashes.emplace(hash);
				}
			}

			// done
			return;
		}

		const std::string sectionRoot = "Group" + std::to_string(groupCounter);
		const std::string vertexHashesCategory = sectionRoot + "_VertexShaders";
		const std::string pixelHashesCategory = sectionRoot + "_PixelShaders";
		const std::string constantsCategory = sectionRoot + "_Constants";

		int amountShaders = iniFile.GetInt("AmountHashes", vertexHashesCategory);
		for(int i = 0;i< amountShaders;i++)
		{
			uint32_t hash = iniFile.GetUInt("ShaderHash" + std::to_string(i), vertexHashesCategory);
			if(hash!=UINT_MAX)
			{
				_vertexShaderHashes.emplace(hash);
			}
		}

		amountShaders = iniFile.GetInt("AmountHashes", pixelHashesCategory);
		for(int i = 0;i< amountShaders;i++)
		{
			uint32_t hash = iniFile.GetUInt("ShaderHash" + std::to_string(i), pixelHashesCategory);
			if(hash!=UINT_MAX)
			{
				_pixelShaderHashes.emplace(hash);
			}
		}

		int amountConstants = iniFile.GetInt("AmountConstants", constantsCategory);
		for (int i = 0; i < amountConstants; i++)
		{
			uint32_t offset = iniFile.GetUInt("Offset" + std::to_string(i), constantsCategory);
			string varName = iniFile.GetString("Variable" + std::to_string(i), constantsCategory);
			if (offset != UINT_MAX && varName.size() > 0)
			{
				_varOffsetMapping.emplace(varName, offset);
			}
		}

		_name = iniFile.GetValue("Name", sectionRoot);
		if(_name.size()<=0)
		{
			_name = "Default";
		}
		const uint32_t toggleKeyValue = iniFile.GetUInt("ToggleKey", sectionRoot);
		if(toggleKeyValue == UINT_MAX)
		{
			_keyData.setKey(VK_CAPITAL, false, false, false);
		}
		else
		{
			_keyData.setKeyFromIniFile(toggleKeyValue);
		}

		_isActive = iniFile.GetBool("Active", sectionRoot);

		std::string techniques = iniFile.GetString("Techniques", sectionRoot);
		if (techniques.size() > 0) {
			std::stringstream ss(techniques);

			while (ss.good())
			{
				string substr;
				getline(ss, substr, ',');
				_preferredTechniques.insert(substr);
			}
		}

		_allowAllTechniques = iniFile.GetBool("AllowAllTechniques", sectionRoot);

		const int32_t historyIndex = iniFile.GetInt("HistoryIndex", sectionRoot);
		if(historyIndex > INT_MIN)
		{
			_historyIndex = historyIndex;
		}
		else
		{
			_historyIndex = 0;
		}

		_isProvidingTextureBinding = iniFile.GetBool("ProvideTextureBinding", sectionRoot);
		_textureBindingName = iniFile.GetString("TextureBindingName", sectionRoot);

		_extractConstants = iniFile.GetBool("ExtractConstants", sectionRoot);
	}

}
