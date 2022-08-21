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

#include <reshade_api.hpp>

#include "stdafx.h"

namespace ShaderToggler
{
	/// <summary>
	/// Class which is used to contain keybinding data
	/// </summary>
	class KeyData
	{
	public:
		KeyData();

		/// <summary>
		/// Ini file variant which has ctrl/shift/alt requirements baked in.
		/// </summary>
		/// <param name="newKeyValue"></param>
		void setKeyFromIniFile(uint32_t newKeyValue);
		/// <summary>
		/// Sets the passed in vk keycode as the key to use for 
		/// </summary>
		/// <param name="newKeyValue"></param>
		/// <param name="shiftRequired"></param>
		/// <param name="altRequired"></param>
		/// <param name="ctrlRequired"></param>
		void setKey(uint8_t newKeyValue, bool shiftRequired=false, bool altRequired=false, bool ctrlRequired=false);
		uint32_t getKeyForIniFile() const;
		void clear();
		/// <summary>
		/// Used for when the instance of this class is used to collect temporary keybinding data for editing
		/// </summary>
		/// <param name="runtime"></param>
		void collectKeysPressed(const reshade::api::effect_runtime* runtime);
		/// <summary>
		/// Returns true if the keyboard shortcut defined by this instance is currently pressed down
		/// </summary>
		/// <param name="runtime"></param>
		/// <returns></returns>
		bool isKeyPressed(const reshade::api::effect_runtime* runtime);

		/// <summary>
		/// Returns a usable description for the keyboard shortcut, or 'Press a key' if undefined/empty
		/// </summary>
		/// <returns></returns>
		std::string getKeyAsString() { return _keyAsString;}
		uint8_t getKeyCode() { return _keyCode;}
		bool isValid() { return _keyCode > 0; }
		void resetKey();

	private:
		static std::string vkCodeToString(uint8_t vkCode);

		void setKeyAsString();

		uint8_t _keyCode;
		bool _shiftRequired;
		bool _altRequired;
		bool _ctrlRequired;
		std::string _keyAsString;

	};
}

