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

#include <map>
#include <reshade_api_device.hpp>
#include <reshade_api_pipeline.hpp>
#include <shared_mutex>
#include <unordered_set>
//#include <boost/unordered/unordered_flat_map.hpp>

#include "CDataFile.h"
#include "ToggleGroup.h"


namespace ShaderToggler
{
    /// <summary>
    /// Class which manages a set of shaders for a given type (pixel, vertex...)
    /// </summary>
    class ShaderManager
    {
    public:
        ShaderManager();

        void addHashHandlePair(uint32_t shaderHash, uint64_t pipelineHandle);
        void removeHandle(uint64_t handle);
        /// <summary>
        /// Switches on the hunting mode for the shader manager. It will copy the passed in hashes to the set of marked hashes. Hunting mode is the mode
        ///	where the user can step through collected active shaders to mark them for assignment to the current edited group.
        /// </summary>
        /// <param name="currentMarkedHashes"></param>
        void startHuntingMode(const std::unordered_set<uint32_t> currentMarkedHashes);
        void stopHuntingMode();
        /// <summary>
        /// Moves to the next shader. If control is pressed as well, it'll step to the next marked shader (if any). If there aren't any shaders in that
        ///	situation, it'll stay on the current shader.
        /// </summary>
        /// <param name="ctrlPressed">If control is pressed as well, it'll step to the next marked shader (if any). If there aren't any shaders in that
        ///	situation, it'll stay on the current shader.</param>
        void huntNextShader(bool ctrlPressed);
        /// <summary>
        /// Moves to the previous shader. If control is pressed as well, it'll step to the previous marked shader (if any). If there aren't any shaders in that
        ///	situation, it'll stay on the current shader.
        /// </summary>
        /// <param name="ctrlPressed">If control is pressed as well, it'll step to the previous marked shader (if any). If there aren't any shaders in that
        ///	situation, it'll stay on the current shader.</param>
        void huntPreviousShader(bool ctrlPressed);
        /// <summary>
        /// Returns true if the shader hash passed in is the currently hunted shader or it's part of the marked shader hashes
        /// </summary>
        /// <param name="shaderHash"></param>
        /// <returns></returns>
        bool isBlockedShader(uint32_t shaderHash);
        /// <summary>
        /// Returns the shader hash for the passed in pipeline handle, if found. 0 otherwise.
        /// </summary>
        /// <param name="handle"></param>
        /// <returns></returns>
        uint32_t getShaderHash(uint64_t handle);
        void addActivePipelineHandle(uint64_t handle);
        void toggleMarkOnHuntedShader();

        size_t getPipelineCount() { return _handleToShaderHash.size(); }
        size_t getShaderCount() { return _shaderHashes.size(); }
        size_t getAmountShaderHashesCollected() { return _collectedActiveShaderHashes.size(); }
        bool isInHuntingMode() { return _isInHuntingMode; }
        uint32_t getActiveHuntedShaderHash() { return _activeHuntedShaderHash; }
        int getActiveHuntedShaderIndex() { return _activeHuntedShaderIndex; }
        void toggleHideMarkedShaders() { _hideMarkedShaders = !_hideMarkedShaders; }

        //vector<uint32_t> getHuntedShaders();
        //void markHuntedShader(uint32_t shader);
        //void unmarkHuntedShader(uint32_t shader);
        //bool isMarkedShader(uint32_t shader);
        //void setActiveHuntedShader(uint32_t shader);

        bool isHuntedShaderMarked()
        {
            std::shared_lock lock(_markedShaderHashMutex);
            return _markedShaderHashes.contains(_activeHuntedShaderHash);
        }

        std::unordered_set<uint32_t> getMarkedShaderHashes()
        {
            std::shared_lock lock(_markedShaderHashMutex);
            return _markedShaderHashes;
        }

        size_t getMarkedShaderCount()
        {
            std::shared_lock lock(_markedShaderHashMutex);
            return _markedShaderHashes.size();
        }

        bool isKnownHandle(uint64_t pipelineHandle)
        {
            std::shared_lock lock(_hashHandlesMutex);
            return _handleToShaderHash.contains(pipelineHandle);
        }

        inline uint32_t safeGetShaderHash(uint64_t pipelineHandle)
        {
            //std::shared_lock lock(_hashHandlesMutex);
            const auto& it = _handleToShaderHash.find(pipelineHandle);

            return it == _handleToShaderHash.end() ? 0 : it->second;
        }

    private:
        void setActiveHuntedShaderHandle();

        std::unordered_set<uint32_t> _shaderHashes;				// all shader hashes added through init pipeline
        std::unordered_map<uint64_t, uint32_t> _handleToShaderHash;		// pipeline handle per shader hash. Handle is removed when a pipeline is destroyed.
        //boost::unordered::unordered_flat_map<uint64_t, uint32_t> _handleToShaderHash;
        std::unordered_set<uint32_t> _collectedActiveShaderHashes;	// shader hashes bound to pipeline handles which were collected during the collection phase after hunting was enabled, which are the pipeline handles active during the last X frames
        std::unordered_set<uint32_t> _markedShaderHashes;		// the hashes for shaders which are currently marked.

        bool _isInHuntingMode = false;
        int _activeHuntedShaderIndex = -1;
        uint32_t _activeHuntedShaderHash;
        std::shared_mutex _collectedActiveHandlesMutex;
        std::shared_mutex _hashHandlesMutex;
        std::shared_mutex _markedShaderHashMutex;
        bool _hideMarkedShaders = false;
    };
}

