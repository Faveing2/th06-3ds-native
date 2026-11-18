#include "SoundPlayer.hpp"

#include "FileSystem.hpp"
//#include <filesystem>
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

#define CHANNEL_COUNT 12
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

struct WavHeader {
    char     riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t fileSize;
    char     wave[4] = {'W', 'A', 'V', 'E'};
    char     fmt[4]  = {'f', 'm', 't', ' '};
    uint32_t fmtSize = 16;
    uint16_t format = 1;       // PCM
    uint16_t channels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample = 16;
    char     data[4] = {'d', 'a', 't', 'a'};
    uint32_t dataSize;
} __attribute__((packed));

bool SaveWav(const char* path,const Uint8* audioData,Uint32 audioBytes,Uint32 sampleRate,Uint16 channels){
    WavHeader header{};

    header.channels = channels;
    header.sampleRate = sampleRate;
    header.bitsPerSample = 16;
    header.blockAlign = channels * sizeof(int16_t);
    header.byteRate = sampleRate * header.blockAlign;
    header.dataSize = audioBytes;
    header.fileSize = 36 + audioBytes;

    FILE* file = std::fopen(path, "wb");
    if (!file)
        return false;

    bool success =std::fwrite(&header, sizeof(header), 1, file) == 1 && std::fwrite(audioData, 1, audioBytes, file) == audioBytes;

    std::fclose(file);
    return success;
}

SoundPlayer::SoundPlayer()
{
    // Note: memset of an std::mutex crashes on windows
    // std::memset(this, 0, sizeof(SoundPlayer));
}

ZunResult SoundPlayer::InitializeDSound()
{

    opendspInit();
    opendspAudioInit((1u << 24) - 1);

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

    for(i32 i = 0; i < ARRAY_SIZE_SIGNED(g_SoundBufferIdxVol); i++){
        //We're using a 32 element array to index a 25 element array?
        if(this->LoadSound(i, g_SFXList[g_SoundBufferIdxVol[i].bufferIdx], 1.0f / ZUN_POWF(10.0f, (float)g_SoundBufferIdxVol[i].volume / -2000)) != ZUN_SUCCESS){
            return ZUN_ERROR;
        }
    }

    return ZUN_SUCCESS;
}

// How this works is that opendspAudioLoad only takes in a path, so first this functions checks if the sfx file exists in /th06/data/wav, if not then it loads the sfx data from the .DAT archive then converts it and saves it to the SDCARD so it can be loaded by opendsp
ZunResult SoundPlayer::LoadSound(i32 idx, const char *path, f32 volumeMultiplier)
{
    printf("Loadsound %s\n", path);

    SDL_AudioCVT sampleConversionDesc;
    SDL_AudioSpec wavFormat;

    u8 *wavRawData;
    u8 *wavRawSamples; 
    u8 *wavDataToWrite;
    u32 wavRawSampleByteCount;

    std::string fullPath = "/th06/" + std::string(path);

    wavRawData = (u8 *)FileSystem::OpenPath(fullPath.c_str(), 1);

    if(wavRawData == NULL){
        printf("Could not load %s\n", fullPath);

        // Can't be one call :(
        FileSystem::CreateDir("/th06/data");
        FileSystem::CreateDir("/th06/data/wav");

        wavRawData = (u8 *)FileSystem::OpenPath(path, 0);

        SDL_LoadWAV_RW(SDL_RWFromConstMem(wavRawData, g_LastFileSize),1, &wavFormat, &wavRawSamples, &wavRawSampleByteCount);

        if (SDL_BuildAudioCVT(&sampleConversionDesc, wavFormat.format, wavFormat.channels, wavFormat.freq, AUDIO_S16SYS, 1,
                            44100) == 1)
        {
            sampleConversionDesc.len = wavRawSampleByteCount;
            sampleConversionDesc.buf = new u8[wavRawSampleByteCount * sampleConversionDesc.len_mult];
            std::memcpy(sampleConversionDesc.buf, wavRawSamples, wavRawSampleByteCount);

            SDL_ConvertAudio(&sampleConversionDesc);

            printf("saving file %s", fullPath);
            SaveWav(fullPath.c_str(), sampleConversionDesc.buf, sampleConversionDesc.len_cvt, wavFormat.freq, wavFormat.channels);

            delete[] sampleConversionDesc.buf;
        }
        else
        {
            printf("saving file %s", fullPath);
            SaveWav(fullPath.c_str(), wavRawSamples, sizeof(wavRawSamples), wavFormat.freq, wavFormat.channels);
        }

        //wavRawData = (u8 *)FileSystem::OpenPath(fullPath.c_str(), 1);
    }

    this->sfx_obj[idx].path = fullPath;
    this->sfx_obj[idx].volume = volumeMultiplier;

    printf("Loaded sound %s idx:%i", this->sfx_obj[idx].path.c_str(), idx);
    return ZUN_SUCCESS;
}

ZunResult SoundPlayer::PlayBGM(bool isLooping)
{
    this->mixVol[0] = 1.0f;
    // Make sure the previous BGM is stopped
    if(!(opendspAudioGetState(this->bgm) != OPENDSP_AUDIO_PLAYING)){
        printf("BGM not released, releasing it.\n");
        this->StopBGM();
    }

    printf("PLaying %s\n", this->backgroundMusic.path);
    opendspAudioOpen(&this->bgm, this->backgroundMusic.path, NULL);

    opendspAudioSetLoop(this->bgm, isLooping);
    opendspAudioPlay(this->bgm);

    return ZUN_SUCCESS;
}

void SoundPlayer::PlaySounds()
{
    //printf("PLAY SOUNDS");
}

void SoundPlayer::PlaySoundByIdx(SoundIdx idx)
{

    if(idx == NO_SOUND){
        return;
    }

    SoundEffect& sound = this->sfx_obj[idx];

    if(!sound.isBound){
        const u32 channelIdx = this->sfx_pointer % CHANNEL_COUNT;
        AudioChannel& channel = this->sfx[this->sfx_pointer];

        // If the channel is already in use
        if(channel.stream != nullptr){
            u32 oldSoundIdx = channel.boundTo;

            opendspAudioClose(&channel.stream);

            this->sfx_obj[oldSoundIdx].isBound = false;
            this->sfx_obj[oldSoundIdx].boundTo = 1000;
        }

        opendspAudioOpen(&channel.stream, sound.path.c_str(), NULL);
        opendspAudioSetVolume(channel.stream, sound.volume);

        channel.boundTo = idx;
        sound.boundTo = channelIdx;
        sound.isBound = true;

        this->mixVol[channelIdx] = sound.volume;

        this->sfx_pointer = (channelIdx + 1) % CHANNEL_COUNT;
    }

    AudioChannel& channel = this->sfx[sound.boundTo];
    opendspAudioStop(channel.stream);
    opendspAudioPlay(channel.stream);

    // if(!this->sfx_obj[idx].isBound){

    //     if(this->sfx_pointer >= 12){
    //         this->sfx_pointer = 1;
    //         opendspAudioClose(&this->sfx[1].stream);
    //         //this->sfx_obj[this->sfx[1].boundTo].boundTo = -1;
    //         this->sfx_obj[this->sfx[1].boundTo - 1].isBound = false;
    //         printf("Invalidating %i\n", this->sfx[1].boundTo);
    //     }

    //     this->sfx_obj[idx].boundTo = this->sfx_pointer;

    //     opendspAudioOpen(&this->sfx[this->sfx_obj[idx].boundTo].stream, this->sfx_obj[idx].path.c_str(), NULL);
    //     opendspAudioSetVolume(this->sfx[this->sfx_obj[idx].boundTo].stream, this->sfx_obj[idx].volume);
    //     this->sfx[this->sfx_obj[idx].boundTo].boundTo = idx;
    //     this->sfx_obj[idx].isBound = true;

    //     printf("Bind channel %i to %i\n", this->sfx_pointer, this->sfx[this->sfx_obj[idx].boundTo].boundTo);

    //     this->mixVol[this->sfx_pointer] = this->sfx_obj[idx].volume;

    //     this->sfx_pointer += 1;
    // }

    //printf("Playing sound %i, vol:%f", idx, this->sfx_obj[idx].volume);
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
