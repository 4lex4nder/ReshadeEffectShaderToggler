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
#include "KeyData.h"

namespace ShaderToggler
{
	KeyData::KeyData(): _keyCode(0), _shiftRequired(false), _altRequired(false), _ctrlRequired(false)
	{
	}


	void KeyData::setKeyFromIniFile(uint32_t newKeyValue)
	{
		if(newKeyValue==0)
		{
			return;
		}
		_keyCode = ((newKeyValue >> 24) & 0xFF);;
		_altRequired = ((newKeyValue >> 16) & 0xFF) == 0x01;
		_ctrlRequired = ((newKeyValue >> 8) & 0xFF) == 0x01;
		_shiftRequired = (newKeyValue & 0xFF) == 0x01;
		setKeyAsString();
	}

	void KeyData::setKey(uint8_t newKeyValue, bool shiftRequired, bool altRequired, bool ctrlRequired)
	{
		if(newKeyValue==0)
		{
			return;
		}
		_keyCode = newKeyValue;
		_ctrlRequired = ctrlRequired;
		_shiftRequired = shiftRequired;
		_altRequired = altRequired;
		setKeyAsString();
	}


	uint32_t KeyData::getKeyForIniFile() const
	{
		return (_keyCode & 0xFF) << 24 | ((_altRequired ? 1 : 0) << 16) | ((_ctrlRequired ? 1 : 0) << 8) | ((_shiftRequired ? 1 : 0));
	}


	void KeyData::clear()
	{
		_altRequired = false;
		_ctrlRequired = false;
		_shiftRequired = false;
		_keyCode = 0;
		setKeyAsString();
	}


	void KeyData::resetKey()
	{
		_altRequired = false;
		_ctrlRequired = false;
		_shiftRequired = false;
		_keyCode = 0;
		_keyAsString = "";
	}


	void KeyData::collectKeysPressed(const reshade::api::effect_runtime* runtime)
	{
		// keys below 7 aren't interesting.
		for(int i=7;i<256;i++)
		{
			switch(i)
			{
				case VK_MENU:
				case VK_CONTROL:
				case VK_SHIFT:
					break;
				default:
					if(runtime->is_key_down(i))
					{
						_keyCode = i;
						_altRequired = runtime->is_key_down(VK_MENU);
						_ctrlRequired = runtime->is_key_down(VK_CONTROL);
						_shiftRequired = runtime->is_key_down(VK_SHIFT);
						setKeyAsString();
					}
			}
		}
	}


	bool KeyData::isKeyPressed(const reshade::api::effect_runtime* runtime)
	{
		bool toReturn = runtime->is_key_pressed(_keyCode);
		if(_altRequired)
		{
			toReturn &= runtime->is_key_down(VK_MENU);
		}
		if(_shiftRequired)
		{
			toReturn &= runtime->is_key_down(VK_SHIFT);
		}
		if(_ctrlRequired)
		{
			toReturn &= runtime->is_key_down(VK_CONTROL);
		}
		return toReturn;
	}


	std::string KeyData::vkCodeToString(uint8_t vkCode)
	{
		// from ReShade
		static const char *keyboard_keys[256] = {
			"", "Left Mouse", "Right Mouse", "Cancel", "Middle Mouse", "X1 Mouse", "X2 Mouse", "", "Backspace", "Tab", "", "", "Clear", "Enter", "", "",
			"Shift", "Control", "Alt", "Pause", "Caps Lock", "", "", "", "", "", "", "Escape", "", "", "", "",
			"Space", "Page Up", "Page Down", "End", "Home", "Left Arrow", "Up Arrow", "Right Arrow", "Down Arrow", "Select", "", "", "Print Screen", "Insert", "Delete", "Help",
			"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "", "", "", "", "", "",
			"", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
			"P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "Left Windows", "Right Windows", "Apps", "", "Sleep",
			"Numpad 0", "Numpad 1", "Numpad 2", "Numpad 3", "Numpad 4", "Numpad 5", "Numpad 6", "Numpad 7", "Numpad 8", "Numpad 9", "Numpad *", "Numpad +", "", "Numpad -", "Numpad Decimal", "Numpad /",
			"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "F13", "F14", "F15", "F16",
			"F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", "", "", "", "", "", "", "", "",
			"Num Lock", "Scroll Lock", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
			"Left Shift", "Right Shift", "Left Control", "Right Control", "Left Menu", "Right Menu", "Browser Back", "Browser Forward", "Browser Refresh", "Browser Stop", "Browser Search", "Browser Favorites", "Browser Home", "Volume Mute", "Volume Down", "Volume Up",
			"Next Track", "Previous Track", "Media Stop", "Media Play/Pause", "Mail", "Media Select", "Launch App 1", "Launch App 2", "", "", "OEM ;", "OEM +", "OEM ,", "OEM -", "OEM .", "OEM /",
			"OEM ~", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
			"", "", "", "", "", "", "", "", "", "", "", "OEM [", "OEM \\", "OEM ]", "OEM '", "OEM 8",
			"", "", "OEM <", "", "", "", "", "", "", "", "", "", "", "", "", "",
			"", "", "", "", "", "", "Attn", "CrSel", "ExSel", "Erase EOF", "Play", "Zoom", "", "PA1", "OEM Clear", ""
		};

		return keyboard_keys[vkCode];
	}


	void KeyData::setKeyAsString()
	{
		if (!_altRequired && !_ctrlRequired && !_shiftRequired && (_keyCode <= 0))
		{
			// empty
			_keyAsString = "Press a key";
			return;
		}
		_keyAsString.clear();
		if (_altRequired)
		{
			_keyAsString.append("Alt + ");
		}
		if (_ctrlRequired)
		{
			_keyAsString.append("Ctrl + ");
		}
		if (_shiftRequired)
		{
			_keyAsString.append("Shift + ");
		}
		if (_keyCode > 0)
		{
			_keyAsString.append(vkCodeToString(_keyCode));
		}

	}
}
