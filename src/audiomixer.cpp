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

#include "audiomixer.h"
#include "platform.h"

const struct MusicFile
{
    const char *id;
    const char *file;
} musicFiles[3] = {
    { "intro", "/snd/introzik.ogg" },
    { "main1p", "/snd/frozen-mainzik-1p.ogg" },
    { "main2p", "/snd/frozen-mainzik-2p.ogg" },
};

AudioMixer *AudioMixer::ptrInstance = NULL;

AudioMixer *AudioMixer::Instance()
{
    if(ptrInstance == NULL)
        ptrInstance = new AudioMixer();
    return ptrInstance;
}

AudioMixer::AudioMixer()
{
    gameSettings = GameSettings::Instance();

    int freq = gameSettings->useClassicAudio() ? 22050 : 44100;
    SDL_AudioSpec spec = {SDL_AUDIO_S16, 2, freq};

    mixer = MIX_CreateMixer(&spec);
    if (!mixer) {
        SDL_LogError(1, "Could not open audio mixer! Music will be disabled. (%s)", SDL_GetError());
        mixerEnabled = false;
        return;
    }

    musicTrack = MIX_CreateTrack(mixer);
}

MIX_Audio* AudioMixer::GetSFX(const char *sfx){
    std::string key(sfx);
    auto it = sfxFiles.find(key);
    if(it == sfxFiles.end()) {
        char rel[128];
        snprintf(rel, sizeof(rel), "/snd/%s.ogg", sfx);
        MIX_Audio* audio = MIX_LoadAudio(mixer, ASSET(rel).c_str(), true);
        sfxFiles[key] = audio;
        return audio;
    }

    return it->second;
}

AudioMixer::~AudioMixer(){
}

void AudioMixer::Dispose(){
    // Clean up SFX tracks
    for (MIX_Track* t : sfxTracks) {
        MIX_StopTrack(t, 0);
        MIX_DestroyTrack(t);
    }
    sfxTracks.clear();

    // Clean up cached SFX audio
    for (auto& pair : sfxFiles) {
        if (pair.second) MIX_DestroyAudio(pair.second);
    }
    sfxFiles.clear();

    // Clean up music
    if (musicTrack) {
        MIX_StopTrack(musicTrack, 0);
        MIX_DestroyTrack(musicTrack);
        musicTrack = nullptr;
    }
    if (curMusicAudio) {
        MIX_DestroyAudio(curMusicAudio);
        curMusicAudio = nullptr;
    }

    if (mixer) {
        MIX_DestroyMixer(mixer);
        mixer = nullptr;
    }

    MIX_Quit();
    this->~AudioMixer();
}

void AudioMixer::PlayMusic(const char *track)
{
    if(mixerEnabled == false || !gameSettings->canPlayMusic() || haltedMixer == true) return;

    // Stop current music
    if (musicTrack) {
#ifdef __WASM_PORT__
        MIX_StopTrack(musicTrack, 0);
#else
        if (MIX_TrackPlaying(musicTrack)) {
            MIX_StopTrack(musicTrack, 500);
            SDL_Delay(600);
        }
#endif
    }

    // Free previous music audio
    if (curMusicAudio) {
        MIX_DestroyAudio(curMusicAudio);
        curMusicAudio = nullptr;
    }

    std::string path;
    for (const MusicFile &musFile: musicFiles)
    {
        if (0 == strcmp(track, musFile.id))
        {
            path = ASSET(musFile.file);
            break;
        }
    }

    curMusicAudio = MIX_LoadAudio(mixer, path.c_str(), false);
    if(curMusicAudio && musicTrack) {
        MIX_SetTrackAudio(musicTrack, curMusicAudio);
        MIX_SetTrackLoops(musicTrack, -1);
        MIX_PlayTrack(musicTrack, 0);
    }
}

void AudioMixer::PlaySFX(const char *sfx)
{
    if(mixerEnabled == false || gameSettings->canPlaySFX() == false || haltedMixer == true) return;

    MIX_Audio* audio = GetSFX(sfx);
    if (!audio) {
        SDL_LogError(1, "Could not load SFX '%s': %s", sfx, SDL_GetError());
        return;
    }

    // Reuse a finished SFX track, or create a new one
    MIX_Track* track = nullptr;
    for (MIX_Track* t : sfxTracks) {
        if (!MIX_TrackPlaying(t)) {
            track = t;
            break;
        }
    }
    if (!track) {
        track = MIX_CreateTrack(mixer);
        if (!track) {
            SDL_LogError(1, "Could not create SFX track: %s", SDL_GetError());
            return;
        }
        sfxTracks.push_back(track);
    }

    MIX_SetTrackAudio(track, audio);
    MIX_SetTrackLoops(track, 0);
    MIX_PlayTrack(track, 0);
}

void AudioMixer::PauseMusic(bool enable){
    if (!musicTrack) return;
    if (enable == true) MIX_ResumeTrack(musicTrack);
    else MIX_PauseTrack(musicTrack);
}

void AudioMixer::MuteAll(bool enable){
    if(enable == true) {
        haltedMixer = false;
    } else {
        if (mixer) MIX_StopAllTracks(mixer, 0);
        haltedMixer = true;
    }
}