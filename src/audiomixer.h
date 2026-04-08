/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 Huy Chau
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AUDIOMIXER_H
#define AUDIOMIXER_H

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <map>
#include <string>
#include <vector>

#include "gamesettings.h"

class AudioMixer final
{
public:
    void PlayMusic(const char *track);
    void PlaySFX(const char *sfx);
    void PauseMusic(bool enable = false);
    void MuteAll(bool enable = false);
    bool IsHalted() { return haltedMixer; };

    AudioMixer(const AudioMixer& obj) = delete;
    void Dispose();
    static AudioMixer* Instance();
private:
    std::map<std::string, MIX_Audio *> sfxFiles;
    MIX_Audio* GetSFX(const char *);

    AudioMixer();
    ~AudioMixer();
    static AudioMixer* ptrInstance;
    GameSettings* gameSettings;

    bool mixerEnabled = true, haltedMixer = false;
    MIX_Mixer* mixer = nullptr;
    MIX_Track* musicTrack = nullptr;
    MIX_Audio* curMusicAudio = nullptr;
    std::vector<MIX_Track*> sfxTracks;
};

#endif // AUDIOMIXER_H