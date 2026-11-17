#include "SoundPlayer.hpp"

#include "FileSystem.hpp"
#include "Supervisor.hpp"
#include "i18n.hpp"
#include "utils.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_timer.h>
#include <array>
#include <cmath>
#include <cstring>
#include <new>
#include <vector>

#include <opendsp.h>
#include <opendsp/audio.h>

// This would all be a lot easier with SDL_mixer, but SDL_mixer doesn't permit any way of doing custom
//   loop points that would be accurate to the sample like EoSD needs. So instead we get to read WAVs and
//   mix everything by hand. Yay

#define BACKGROUND_MUSIC_WAV_NUM_CHANNELS 2
#define BACKGROUND_MUSIC_WAV_SAMPLE_RATE 44100
#define BACKGROUND_MUSIC_WAV_BITS_PER_SAMPLE 16
#define BACKGROUND_MUSIC_WAV_BLOCK_ALIGN (BACKGROUND_MUSIC_WAV_BITS_PER_SAMPLE / 8 * BACKGROUND_MUSIC_WAV_NUM_CHANNELS)
#define BACKGROUND_MUSIC_WAV_BYTE_RATE (BACKGROUND_MUSIC_WAV_BLOCK_ALIGN * BACKGROUND_MUSIC_WAV_SAMPLE_RATE)

// DirectSound deals with volume by subtracting a number measured in hundredths of decibels from the source sound.
//   The scale is from 0 (no volume modification) to -10,000 (subtraction of 100 decibels, and basically silent).
//   20 decibels affects wave amplitude by a factor of 10

// I should probably have a function to apply a flat volume increase, or I can just edit these values :)
static const SoundBufferIdxVolume g_SoundBufferIdxVol[32] = {
    {0, -1200}, {0, -1700}, {1, -900}, {1, -1100}, {2, -700},  {3, -300},   {4, -200},   {5, -1400},
    {6, -1400}, {7, -1400}, {8, -700}, {9, -700}, {10, -600}, {11, -900}, {12, -600},  {5, -1200},
    {13, -600}, {14, -600}, {15, -300}, {16, -100}, {17, -800}, {18, -600},  {5, -1500},  {6, -1500},
    {7, -1500}, {19, -000}, {20, -300}, {21, -500}, {22, -000},  {23, -200},  {24, -700}, {25, -700},
};
static const char *const g_SFXList[26] = {
    "data/wav/plst00.wav", "data/wav/enep00.wav",   "data/wav/pldead00.wav", "data/wav/power0.wav",
    "data/wav/power1.wav", "data/wav/tan00.wav",    "data/wav/tan01.wav",    "data/wav/tan02.wav",
    "data/wav/ok00.wav",   "data/wav/cancel00.wav", "data/wav/select00.wav", "data/wav/gun00.wav",
    "data/wav/cat00.wav",  "data/wav/lazer00.wav",  "data/wav/lazer01.wav",  "data/wav/enep01.wav",
    "data/wav/nep00.wav",  "data/wav/damage00.wav", "data/wav/item00.wav",   "data/wav/kira00.wav",
    "data/wav/kira01.wav", "data/wav/kira02.wav",   "data/wav/extend.wav",   "data/wav/timeout.wav",
    "data/wav/graze.wav",  "data/wav/powerup.wav",
};
SoundPlayer g_SoundPlayer;

SoundPlayer::SoundPlayer()
{
    // Note: memset of an std::mutex crashes on windows
    // std::memset(this, 0, sizeof(SoundPlayer));
}

ZunResult SoundPlayer::InitializeDSound()
{

    opendspInit();
    opendspAudioInit((1u << 0) | (1u << 1));

    return ZUN_SUCCESS;
}

ZunResult SoundPlayer::Release(void)
{
    opendspAudioClose(&this->bgm);
    // Do not reset this back to null
    //this->bgm = NULL;

    return ZUN_SUCCESS;
}

void SoundPlayer::StopBGM()
{
    printf("Stopping bgm\n");
    opendspAudioStop(this->bgm);
    Release();
}

void SoundPlayer::FadeOut(f32 seconds)
{

}

ZunResult SoundPlayer::LoadWav(const char *path)
{
    //this->StopBGM();

    if(!(this->bgm == NULL)){
        printf("BGM not released, releasing it.");
        this->StopBGM();
    }

    printf("Loading wav %s\n", path);
    this->backgroundMusic.path = path;

    return ZUN_SUCCESS;
}

ZunResult SoundPlayer::LoadPos(const char *path)
{
    return ZUN_SUCCESS;
}

ZunResult SoundPlayer::InitSoundBuffers()
{
    return ZUN_SUCCESS;
}

ZunResult SoundPlayer::LoadSound(i32 idx, const char *path, f32 volumeMultiplier)
{
    return ZUN_SUCCESS;
}

ZunResult SoundPlayer::PlayBGM(bool isLooping)
{

    printf("PLaying %s\n", this->backgroundMusic.path);
    opendspAudioOpen(&this->bgm, this->backgroundMusic.path, NULL);

    opendspAudioSetLoop(this->bgm, isLooping);
    opendspAudioPlay(this->bgm);

    return ZUN_SUCCESS;
}

void SoundPlayer::PlaySounds()
{

}

void SoundPlayer::PlaySoundByIdx(SoundIdx idx)
{

}

void SoundPlayer::MixAudio(u32 samples)
{

}

// EoSD originally just used this function to manage the streaming of the music WAV file.
//   We also use it to mix and queue audio, since we have to do that manually and doing it
//   in a thread keeps sound running continuously, even if the main thread runs into lag
void SoundPlayer::BackgroundMusicPlayerThread()
{

}
