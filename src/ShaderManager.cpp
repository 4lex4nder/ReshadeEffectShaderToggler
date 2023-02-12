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

#include "ShaderManager.h"

using namespace reshade::api;

namespace ShaderToggler
{
    ShaderManager::ShaderManager() : _activeHuntedShaderHash(0)
    {
    }


    void ShaderManager::addHashHandlePair(uint32_t shaderHash, uint64_t pipelineHandle)
    {
        if (pipelineHandle > 0 && shaderHash > 0)
        {
            std::unique_lock lock(_hashHandlesMutex);
            _handleToShaderHash[pipelineHandle] = shaderHash;
            _shaderHashes.emplace(shaderHash);
        }
    }


    void ShaderManager::removeHandle(uint64_t handle)
    {
        std::unique_lock ulock(_hashHandlesMutex);
        if (_handleToShaderHash.contains(handle))
        {
            const auto& it = _handleToShaderHash.find(handle);
            const auto& shaderHash = it->second;
            _handleToShaderHash.erase(handle);
            _collectedActiveShaderHashes.erase(shaderHash);
            _shaderHashes.erase(shaderHash);
        }
    }


    void ShaderManager::startHuntingMode(const std::unordered_set<uint32_t> currentMarkedHashes)
    {
        // copy the currently marked hashes (from the active group) to the set of marked hashes.
        {
            std::unique_lock lock(_markedShaderHashMutex);
            _markedShaderHashes.clear();
            for (const auto hash : currentMarkedHashes)
            {
                _markedShaderHashes.emplace(hash);
            }
        }

        // switch on hunting mode
        _isInHuntingMode = true;
        _activeHuntedShaderIndex = -1;
        _activeHuntedShaderHash = 0;
        {
            std::unique_lock lock(_collectedActiveHandlesMutex);
            _collectedActiveShaderHashes.clear();			// clear it so we start with a clean slate
        }
    }


    void ShaderManager::stopHuntingMode()
    {
        _isInHuntingMode = false;
        _activeHuntedShaderIndex = -1;
        _activeHuntedShaderHash = 0;
        {
            std::unique_lock lock(_markedShaderHashMutex);
            _markedShaderHashes.clear();
        }
    }


    void ShaderManager::setActiveHuntedShaderHandle()
    {
        if (_activeHuntedShaderIndex < 0 || _collectedActiveShaderHashes.size() <= 0 || _activeHuntedShaderIndex >= _collectedActiveShaderHashes.size())
        {
            _activeHuntedShaderHash = 0;
            return;
        }

        // no lock needed, collecting phase is over
        auto it = _collectedActiveShaderHashes.begin();
        std::advance(it, _activeHuntedShaderIndex);
        _activeHuntedShaderHash = *it;
    }


    void ShaderManager::huntNextShader(bool ctrlPressed)
    {
        if (!_isInHuntingMode)
        {
            return;
        }
        if (_collectedActiveShaderHashes.size() <= 0)
        {
            return;
        }
        if (ctrlPressed)
        {
            if (_markedShaderHashes.size() == 0 || (_markedShaderHashes.size() == 1 && _markedShaderHashes.contains(_activeHuntedShaderHash)))
            {
                // optimization: if the current active shader is part of marked shader hashes and there is
                // just 1 marked, then we can also stop. We then don't need to do anything so we can return
                // also if there are no marked shaders, we won't find a next, so return now too.
                return;
            }

            // we have marked shaders, find the next one in collected active shader hashes that's part of this set.
            auto it = _collectedActiveShaderHashes.begin();
            int index = _activeHuntedShaderIndex + 1;
            std::advance(it, index);
            bool foundHash = false;
            uint32_t hash = 0;
            while (index != _activeHuntedShaderIndex)
            {
                if (it == _collectedActiveShaderHashes.end())
                {
                    it = _collectedActiveShaderHashes.begin();
                    index = 0;
                }
                hash = *it;
                if (_markedShaderHashes.contains(hash))
                {
                    // found one
                    foundHash = true;
                    break;
                }
                ++it;
                index++;
            }
            if (foundHash)
            {
                _activeHuntedShaderIndex = index;
                _activeHuntedShaderHash = hash;
            }
            // always done
            return;
        }
        if (_activeHuntedShaderIndex < _collectedActiveShaderHashes.size() - 1)
        {
            _activeHuntedShaderIndex++;
        }
        else
        {
            _activeHuntedShaderIndex = 0;
        }
        setActiveHuntedShaderHandle();
    }


    void ShaderManager::huntPreviousShader(bool ctrlPressed)
    {
        if (!_isInHuntingMode)
        {
            return;
        }
        if (_collectedActiveShaderHashes.size() <= 0)
        {
            return;
        }
        if (ctrlPressed)
        {
            if (_markedShaderHashes.size() == 0 || (_markedShaderHashes.size() == 1 && _markedShaderHashes.contains(_activeHuntedShaderHash)))
            {
                // optimization: if the current active shader is part of marked shader hashes and there is
                // just 1 marked, then we can also stop. We then don't need to do anything so we can return
                // also if there are no marked shaders, we won't find a next, so return now too.
                return;
            }
            // we have marked shaders, find the next one in collected active shader hashes that's part of this set.
            auto it = _collectedActiveShaderHashes.begin();
            int index = _activeHuntedShaderIndex - 1;
            std::advance(it, index);
            bool foundHash = false;
            uint32_t hash = 0;
            while (index != _activeHuntedShaderIndex)
            {
                if (it == _collectedActiveShaderHashes.begin())
                {
                    it = _collectedActiveShaderHashes.end();
                    --it;
                    index = _collectedActiveShaderHashes.size() - 1;
                }
                hash = *it;
                if (_markedShaderHashes.contains(hash))
                {
                    // found one
                    foundHash = true;
                    break;
                }
                --it;
                index--;
            }
            if (foundHash)
            {
                _activeHuntedShaderIndex = index;
                _activeHuntedShaderHash = hash;
            }
            // always done
            return;

        }
        if (_activeHuntedShaderIndex <= 0)
        {
            _activeHuntedShaderIndex = _collectedActiveShaderHashes.size() - 1;
        }
        else
        {
            --_activeHuntedShaderIndex;
        }
        setActiveHuntedShaderHandle();
    }


    bool ShaderManager::isBlockedShader(uint32_t shaderHash)
    {
        bool toReturn = false;
        if (_isInHuntingMode)
        {
            // get the shader hash bound to this pipeline handle
            toReturn |= shaderHash <= 0 ? false : _activeHuntedShaderHash == shaderHash;
        }
        if (_hideMarkedShaders)
        {
            // check if the shader hash is part of the toggle group
            toReturn |= _markedShaderHashes.contains(shaderHash);
        }

        return toReturn;
    }


    void ShaderManager::addActivePipelineHandle(uint64_t handle)
    {
        // get the shader hash bound to this pipeline handle
        const auto shaderHash = getShaderHash(handle);
        if (shaderHash > 0)
        {
            std::unique_lock lock(_collectedActiveHandlesMutex);
            _collectedActiveShaderHashes.emplace(shaderHash);
        }
    }


    void ShaderManager::toggleMarkOnHuntedShader()
    {
        if (_activeHuntedShaderHash <= 0)
        {
            return;
        }
        std::unique_lock lock(_markedShaderHashMutex);
        if (_markedShaderHashes.contains(_activeHuntedShaderHash))
        {
            // remove it
            _markedShaderHashes.erase(_activeHuntedShaderHash);
        }
        else
        {
            // add it
            _markedShaderHashes.emplace(_activeHuntedShaderHash);
        }
    }


    uint32_t ShaderManager::getShaderHash(uint64_t handle)
    {
        if (!_handleToShaderHash.contains(handle))
        {
            return 0;
        }
        return _handleToShaderHash.at(handle);
    }

    //vector<uint32_t> ShaderManager::getHuntedShaders()
    //{
    //	vector<uint32_t> vector;
    //	vector.reserve(_collectedActiveShaderHashes.size());
    //	vector.insert(vector.end(), _collectedActiveShaderHashes.begin(), _collectedActiveShaderHashes.end());
    //
    //	return vector;
    //}
    //
    //void ShaderManager::markHuntedShader(uint32_t shader)
    //{
    //	std::unique_lock lock(_markedShaderHashMutex);
    //	_markedShaderHashes.emplace(shader);
    //}
    //
    //void ShaderManager::unmarkHuntedShader(uint32_t shader)
    //{
    //	std::unique_lock lock(_markedShaderHashMutex);
    //	_markedShaderHashes.erase(shader);
    //}
    //
    //bool ShaderManager::isMarkedShader(uint32_t shader)
    //{
    //	return _markedShaderHashes.contains(shader);
    //}
    //
    //void ShaderManager::setActiveHuntedShader(uint32_t shader)
    //{
    //	if(_collectedActiveShaderHashes.contains(shader))
    //		_activeHuntedShaderHash = shader;
    //}
}
