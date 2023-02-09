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

#include <format>
#include "AddonUIData.h"

using namespace AddonImGui;

AddonUIData::AddonUIData(ShaderManager* pixelShaderManager, ShaderManager* vertexShaderManager, ConstantHandlerBase* cHandler, atomic_uint32_t* activeCollectorFrameCounter,
    vector<string>* techniques, unordered_map<string, tuple<constant_type, vector<effect_uniform_variable>>>* constants) :
    _pixelShaderManager(pixelShaderManager), _vertexShaderManager(vertexShaderManager), _activeCollectorFrameCounter(activeCollectorFrameCounter),
    _allTechniques(techniques), _constantHandler(cHandler), _constants(constants)
{
    _toggleGroupIdShaderEditing = -1;
    _overlayOpacity = 0.2f;

    _keyBindings[Keybind::PIXEL_SHADER_DOWN] = VK_NUMPAD1;
    _keyBindings[Keybind::PIXEL_SHADER_UP] = VK_NUMPAD2;
    _keyBindings[Keybind::PIXEL_SHADER_MARK] = VK_NUMPAD3;
    _keyBindings[Keybind::PIXEL_SHADER_MARKED_DOWN] = VK_NUMPAD1 | (VK_CONTROL << 8);
    _keyBindings[Keybind::PIXEL_SHADER_MARKED_UP] = VK_NUMPAD2 | (VK_CONTROL << 8);
    _keyBindings[Keybind::VERTEX_SHADER_DOWN] = VK_NUMPAD4;
    _keyBindings[Keybind::VERTEX_SHADER_UP] = VK_NUMPAD5;
    _keyBindings[Keybind::VERTEX_SHADER_MARK] = VK_NUMPAD6;
    _keyBindings[Keybind::VERTEX_SHADER_MARKED_DOWN] = VK_NUMPAD4 | (VK_CONTROL << 8);
    _keyBindings[Keybind::VERTEX_SHADER_MARKED_UP] = VK_NUMPAD5 | (VK_CONTROL << 8);
    _keyBindings[Keybind::HISTORY_DOWN] = VK_NUMPAD7;
    _keyBindings[Keybind::HISTORY_UP] = VK_NUMPAD8;
}


std::unordered_map<int, ToggleGroup>& AddonUIData::GetToggleGroups()
{
    return _toggleGroups;
}

const vector<string>* AddonUIData::GetAllTechniques() const
{
    return _allTechniques;
}


const atomic_int& AddonUIData::GetToggleGroupIdShaderEditing() const
{
    return _toggleGroupIdShaderEditing;
}

void AddonUIData::StopHuntingMode()
{
    _pixelShaderManager->stopHuntingMode();
    _vertexShaderManager->stopHuntingMode();
}


/// <summary>
/// Adds a default group with VK_CAPITAL as toggle key. Only used if there aren't any groups defined in the ini file.
/// </summary>
void AddonUIData::AddDefaultGroup()
{
    ToggleGroup toAdd("Default", ToggleGroup::getNewGroupId());
    toAdd.setToggleKey(0);
    _toggleGroups.emplace(toAdd.getId(), toAdd);
}


/// <summary>
/// Loads the defined hashes and groups from the shaderToggler.ini file.
/// </summary>
void AddonUIData::LoadShaderTogglerIniFile(const std::string& fileName)
{
    // Will assume it's started at the start of the application and therefore no groups are present.

    reshade::log_message(3, std::format("Loading config file from \"{}\"", (_basePath / fileName).string()).c_str());

    CDataFile iniFile;
    if (!iniFile.Load((_basePath / fileName).string()))
    {
        reshade::log_message(3, std::format("Could not find config file at \"{}\"", (_basePath / fileName).string()).c_str());
        // not there
        return;
    }

    _memcpyHookAttempt = iniFile.GetBool("AttemptMemcpyHook", "General");
    _memcpyAssumeUnnested = iniFile.GetBool("MemcpyAssumeUnnestedMap", "General");

    for (uint32_t i = 0; i < ARRAYSIZE(KeybindNames); i++)
    {
        uint32_t keybinding = iniFile.GetUInt(KeybindNames[i], "Keybindings");
        if (keybinding != UINT_MAX)
        {
            _keyBindings[i] = keybinding;
        }
    }

    int groupCounter = 0;
    const int numberOfGroups = iniFile.GetInt("AmountGroups", "General");
    if (numberOfGroups == INT_MIN)
    {
        // old format file?
        AddDefaultGroup();
        groupCounter = -1;	// enforce old format read for pre 1.0 ini file.
    }
    else
    {
        for (int i = 0; i < numberOfGroups; i++)
        {
            ToggleGroup group("", ToggleGroup::getNewGroupId());
            _toggleGroups.emplace(group.getId(), group);
        }
    }
    for (auto& group : _toggleGroups)
    {
        group.second.loadState(iniFile, groupCounter);		// groupCounter is normally 0 or greater. For when the old format is detected, it's -1 (and there's 1 group).
        groupCounter++;
    }
}


/// <summary>
/// Saves the currently known toggle groups with their shader hashes to the shadertoggler.ini file
/// </summary>
void AddonUIData::SaveShaderTogglerIniFile(const std::string& fileName)
{
    // format: first section with # of groups, then per group a section with pixel and vertex shaders, as well as their name and key value.
    // groups are stored with "Group" + group counter, starting with 0.
    CDataFile iniFile;

    iniFile.SetBool("AttemptMemcpyHook", _memcpyHookAttempt, "", "General");
    iniFile.SetBool("MemcpyAssumeUnnestedMap", _memcpyAssumeUnnested, "", "General");

    for (uint32_t i = 0; i < ARRAYSIZE(KeybindNames); i++)
    {
        uint32_t keybinding = iniFile.SetUInt(KeybindNames[i], _keyBindings[i], "", "Keybindings");
    }

    iniFile.SetInt("AmountGroups", static_cast<int>(_toggleGroups.size()), "", "General");

    int groupCounter = 0;
    for (const auto& group : _toggleGroups)
    {
        group.second.saveState(iniFile, groupCounter);
        groupCounter++;
    }
    reshade::log_message(3, std::format("Creating config file at \"{}\"", (_basePath / fileName).string()).c_str());

    iniFile.SetFileName((_basePath / fileName).string());
    iniFile.Save();
}


/// <summary>
/// Function which marks the end of a shader editing cycle for a given toggle group
/// </summary>
/// <param name="acceptCollectedShaderHashes"></param>
/// <param name="groupEditing"></param>
void AddonUIData::EndShaderEditing(bool acceptCollectedShaderHashes, ToggleGroup& groupEditing)
{
    if (acceptCollectedShaderHashes && _toggleGroupIdShaderEditing == groupEditing.getId())
    {
        groupEditing.storeCollectedHashes(_pixelShaderManager->getMarkedShaderHashes(), _vertexShaderManager->getMarkedShaderHashes());
        _pixelShaderManager->stopHuntingMode();
        _vertexShaderManager->stopHuntingMode();
    }
    _toggleGroupIdShaderEditing = -1;
}


/// <summary>
/// Function which marks the start of a shader editing cycle for a given toggle group.
/// </summary>
/// <param name="groupEditing"></param>
void AddonUIData::StartShaderEditing(ToggleGroup& groupEditing)
{
    if (_toggleGroupIdShaderEditing == groupEditing.getId())
    {
        return;
    }
    if (_toggleGroupIdShaderEditing >= 0)
    {
        EndShaderEditing(false, groupEditing);
    }
    _toggleGroupIdShaderEditing = groupEditing.getId();
    *_activeCollectorFrameCounter = _startValueFramecountCollectionPhase;
    _pixelShaderManager->startHuntingMode(groupEditing.getPixelShaderHashes());
    _vertexShaderManager->startHuntingMode(groupEditing.getVertexShaderHashes());

    // after copying them to the managers, we can now clear the group's shader.
    groupEditing.clearHashes();
}


/// <summary>
/// Mark the given shader group for editing of it's effect whitelist
/// </summary>
/// <param name="groupEditing"></param>
void AddonUIData::StartEffectEditing(ToggleGroup& groupEditing)
{
    _toggleGroupIdEffectEditing = groupEditing.getId();
}

/// <summary>
/// End editing of the current shader group's effect whitelist
/// </summary>
/// <param name="groupEditing"></param>
void AddonUIData::EndEffectEditing()
{
    _toggleGroupIdEffectEditing = -1;
}

/// <summary>
/// Mark the given shader group for editing of it's constants
/// </summary>
/// <param name="groupEditing"></param>
void AddonUIData::StartConstantEditing(ToggleGroup& groupEditing)
{
    _toggleGroupIdConstantEditing = groupEditing.getId();
}

/// <summary>
/// End editing of the current shader group's constants
/// </summary>
/// <param name="groupEditing"></param>
void AddonUIData::EndConstantEditing()
{
    _toggleGroupIdConstantEditing = -1;
}

uint32_t AddonUIData::GetKeybinding(Keybind keybind)
{
    return _keyBindings[keybind];
}

void AddonUIData::SetKeybinding(Keybind keybind, uint32_t keys)
{
    _keyBindings[keybind] = keys;
}