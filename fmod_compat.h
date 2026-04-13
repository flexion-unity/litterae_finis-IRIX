///////////////////////////////////////////////
// Minimal FMOD API compatibility layer
// Implements the subset used by this demo via SDL2_mixer.
//
// Timing: FSOUND_Stream_GetTime uses SDL_GetTicks() anchored to
// when playback starts. Audio and clock advance at the same rate
// so sync stays accurate throughout the demo's ~3 minute runtime.
///////////////////////////////////////////////
#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>

#define FSOUND_MPEGACCURATE  0
#define FSOUND_LOOP_OFF      0
#define FSOUND_ALL          -1

typedef Mix_Music FSOUND_STREAM;

// Internal playback state
static Mix_Music *_fmod_music    = NULL;
static Uint32     _fmod_start_ms = 0;
static int        _fmod_playing  = 0;

inline int FSOUND_Init(int rate, int /*maxchannels*/, unsigned int /*flags*/)
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL audio init failed: %s\n", SDL_GetError());
        return 0;
    }
    if (Mix_OpenAudio(rate, MIX_DEFAULT_FORMAT, 2, 4096) < 0) {
        fprintf(stderr, "SDL_mixer init failed: %s\n", Mix_GetError());
        return 0;
    }
    int flags = Mix_Init(MIX_INIT_MP3);
    if (!(flags & MIX_INIT_MP3)) {
        fprintf(stderr, "SDL_mixer MP3 support unavailable: %s\n", Mix_GetError());
        // Not fatal — Mix_LoadMUS may still handle it via other decoders
    }
    return 1;
}

inline FSOUND_STREAM *FSOUND_Stream_Open(const char *filename,
                                         int /*mode*/, int /*offset*/, int /*length*/)
{
    Mix_Music *m = Mix_LoadMUS(filename);
    if (!m)
        fprintf(stderr, "Could not open audio file '%s': %s\n", filename, Mix_GetError());
    return m;
}

inline int FSOUND_Stream_SetMode(FSOUND_STREAM * /*stream*/, int /*mode*/)
{
    return 1; // FSOUND_LOOP_OFF is already the default (Mix_PlayMusic loops=0)
}

inline int FSOUND_Stream_Play(int /*channel*/, FSOUND_STREAM *stream)
{
    if (!stream) return 0;
    _fmod_music    = stream;
    _fmod_playing  = 1;
    _fmod_start_ms = SDL_GetTicks();
    if (Mix_PlayMusic(stream, 0) < 0) {
        fprintf(stderr, "Mix_PlayMusic failed: %s\n", Mix_GetError());
        return 0;
    }
    return 1;
}

inline int FSOUND_SetVolume(int /*channel*/, int vol)
{
    // FMOD: 0-255; SDL_mixer: 0-MIX_MAX_VOLUME (128)
    Mix_VolumeMusic(vol / 2);
    return 1;
}

inline unsigned int FSOUND_Stream_GetTime(FSOUND_STREAM * /*stream*/)
{
    if (!_fmod_playing) return 0;
    return SDL_GetTicks() - _fmod_start_ms;
}

inline int FSOUND_Stream_SetTime(FSOUND_STREAM *stream, unsigned int ms)
{
    if (stream)
        Mix_SetMusicPosition((double)ms / 1000.0);
    // Adjust our clock anchor so GetTime is consistent with the seek
    _fmod_start_ms = SDL_GetTicks() - ms;
    return 1;
}
