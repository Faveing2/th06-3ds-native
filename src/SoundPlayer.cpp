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
#include <string>
#include <new>
#include <vector>

#include <opendsp.h>
#include <opendsp/audio.h>
#include <3ds.h>

// This would all be a lot easier with SDL_mixer, but SDL_mixer doesn't permit any way of doing custom
//   loop points that would be accurate to the sample like EoSD needs. So instead we get to read WAVs and
//   mix everything by hand. Yay

#define BACKGROUND_MUSIC_WAV_NUM_CHANNELS 2
#define BACKGROUND_MUSIC_WAV_SAMPLE_RATE 44100
#define BACKGROUND_MUSIC_WAV_BITS_PER_SAMPLE 16
#define BACKGROUND_MUSIC_WAV_BLOCK_ALIGN (BACKGROUND_MUSIC_WAV_BITS_PER_SAMPLE / 8 * BACKGROUND_MUSIC_WAV_NUM_CHANNELS)
#define BACKGROUND_MUSIC_WAV_BYTE_RATE (BACKGROUND_MUSIC_WAV_BLOCK_ALIGN * BACKGROUND_MUSIC_WAV_SAMPLE_RATE)

#define CHANNEL_COUNT 20
// DirectSound deals with volume by subtracting a number measured in hundredths of decibels from the source sound.
//   The scale is from 0 (no volume modification) to -10,000 (subtraction of 100 decibels, and basically silent).
//   20 decibels affects wave amplitude by a factor of 10

// I should probably have a function to apply a flat volume increase, or I can just edit these values :)
static const SoundBufferIdxVolume g_SoundBufferIdxVol[32] = {
    {0, -1500}, {0, -2000}, {1, -1200}, {1, -1400}, {2, -1000},  {3, -500},   {4, -500},   {5, -1700},
    {6, -1700}, {7, -1700}, {8, -1000}, {9, -1000}, {10, -1900}, {11, -1200}, {12, -900},  {5, -1500},
    {13, -900}, {14, -900}, {15, -600}, {16, -400}, {17, -1100}, {18, -900},  {5, -1800},  {6, -1800},
    {7, -1800}, {19, -300}, {20, -600}, {21, -800}, {22, -100},  {23, -500},  {24, -1000}, {25, -1000},
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

    this->backgroundMusicThreadHandle = std::thread(&SoundPlayer::BackgroundMusicPlayerThread, this);

    return ZUN_SUCCESS;
}

ZunResult SoundPlayer::Release(void)
{
    opendspAudioClose(&this->bgm);

    this->terminateFlag = true;
    // Do not reset this back to null
    //this->bgm = NULL;

    return ZUN_SUCCESS;
}

void SoundPlayer::StopBGM()
{
    opendspAudioStop(this->bgm);
    opendspAudioClose(&this->bgm);
   //Release();
}

void SoundPlayer::FadeOut(f32 seconds)
{

}

ZunResult SoundPlayer::LoadWav(const char *path)
{
    this->backgroundMusic.path = path;

    return ZUN_SUCCESS;
}

ZunResult SoundPlayer::LoadPos(const char *path)
{
    return ZUN_SUCCESS;
}

ZunResult SoundPlayer::InitSoundBuffers()
{

    opendspInit();
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspSetClippingMode(NDSP_CLIP_NORMAL);

    std::fill_n(this->soundBuffersToPlay, ARRAY_SIZE(this->soundBuffersToPlay), -1);

    for (int idx = 0; idx < ARRAY_SIZE_SIGNED(g_SoundBufferIdxVol); idx++)
    {
        if (this->LoadSound(idx, g_SFXList[g_SoundBufferIdxVol[idx].bufferIdx],
                            1.0f / ZUN_POWF(10.0f, (float)g_SoundBufferIdxVol[idx].volume / -2000)) != ZUN_SUCCESS)
        {
            g_GameErrorContext.Log(TH_ERR_SOUNDPLAYER_FAILED_TO_LOAD_SOUND_FILE, g_SFXList[idx]);
            return ZUN_ERROR;
        }

        this->soundBuffers[idx].isPlaying = false;
        this->soundBuffers[idx].pos = 0;
    }

    return ZUN_SUCCESS;
}

// How this works is that opendspAudioLoad only takes in a path, so first this functions checks if the sfx file exists in /th06/data/wav, if not then it loads the sfx data from the .DAT archive then converts it and saves it to the SDCARD so it can be loaded by opendsp
ZunResult SoundPlayer::LoadSound(i32 idx, const char *path, f32 volumeMultiplier)
{
    this->soundBufMutex.lock();

    SDL_AudioCVT sampleConversionDesc;
    SDL_AudioSpec wavFormat;

    u8 *wavRawData;
    u8 *wavRawSamples; 
    u8 *wavDataToWrite;
    u32 wavRawSampleByteCount;

    if (this->soundBuffers[idx].samples != NULL)
    {
        linearFree(this->soundBuffers[idx].samples);
        this->soundBuffers[idx].samples = NULL;
    }

    wavRawData = (u8 *)FileSystem::OpenPath(path, 0);

    if(wavRawData == NULL){
        goto fail;
    }

    if(SDL_LoadWAV_RW(SDL_RWFromConstMem(wavRawData, g_LastFileSize),1, &wavFormat, &wavRawSamples, &wavRawSampleByteCount)==NULL){
        goto fail;
    };

    if(SDL_BuildAudioCVT(&sampleConversionDesc, wavFormat.format, wavFormat.channels, wavFormat.freq, AUDIO_S16SYS, 1,22050)==1){
        sampleConversionDesc.len = wavRawSampleByteCount;
        sampleConversionDesc.buf = new u8[wavRawSampleByteCount * sampleConversionDesc.len_mult];
        std::memcpy(sampleConversionDesc.buf, wavRawSamples, wavRawSampleByteCount);

        SDL_ConvertAudio(&sampleConversionDesc);

        this->soundBuffers[idx].vol = volumeMultiplier;
        this->soundBuffers[idx].len = sampleConversionDesc.len_cvt / 2;
        this->soundBuffers[idx].samples = static_cast<i16*>(linearAlloc(this->soundBuffers[idx].len * sizeof(i16)));
        std::memcpy(this->soundBuffers[idx].samples, sampleConversionDesc.buf, sampleConversionDesc.len_cvt);

        delete[] sampleConversionDesc.buf;
    }else{
        this->soundBuffers[idx].vol = volumeMultiplier;
        this->soundBuffers[idx].len = wavRawSampleByteCount / 2;
        this->soundBuffers[idx].samples = static_cast<i16*>(linearAlloc(this->soundBuffers[idx].len * sizeof(i16)));
        std::memcpy(this->soundBuffers[idx].samples, wavRawSamples, wavRawSampleByteCount);
    }

    for (u32 i = 0; i < this->soundBuffers[idx].len; i++)
    {
        this->soundBuffers[idx].samples[i] *= volumeMultiplier;
    }

    

    this->soundBuffers[idx].pos = 0;
    this->soundBuffers[idx].isPlaying = false;

    SDL_FreeWAV(wavRawSamples);

fail:
    this->soundBufMutex.unlock();
    return ZUN_SUCCESS;
}

ZunResult SoundPlayer::PlayBGM(bool isLooping)
{
    // Make sure the previous BGM is stopped
    if(!(opendspAudioGetState(this->bgm) != OPENDSP_AUDIO_PLAYING)){
        this->StopBGM();
    }
    // float mix[12] = {};
    // mix[0] = 1/8;
    // mix[1] = 1/8;
    //opendspAudioSetMix(this->bgm, mix);
    opendspAudioOpen(&this->bgm, this->backgroundMusic.path, NULL);
    opendspAudioSetVolume(this->bgm, 0.125f);

    opendspAudioSetLoop(this->bgm, isLooping);
    opendspAudioPlay(this->bgm);

    return ZUN_SUCCESS;
}

bool SoundPlayer::RestartSound(i32 soundIdx, u8 normalize)
{
    SoundData& sound = this->soundBuffers[soundIdx];

    if (!sound.dspStarted || sound.dspChannel >= CHANNEL_COUNT)
        return false;

    const u32 channel = sound.dspChannel;
    ndspWaveBuf& wave = this->waveBuffers[channel];

    const float gain = 1.0f / std::max<u8>(normalize, 8);

    // Fully reset the channel and its wave queue.
    ndspChnReset(channel);

    ndspChnSetInterp(channel, NDSP_INTERP_NONE);
    ndspChnSetRate(channel, 22050);
    ndspChnSetFormat(channel, NDSP_FORMAT_MONO_PCM16);

    float mix[12] = {};
    mix[0] = gain;
    mix[1] = gain;
    ndspChnSetMix(channel, mix);

    wave.data_vaddr = sound.samples;
    wave.nsamples = sound.len;
    wave.looping = false;

    DSP_FlushDataCache(&wave, sizeof(wave));
    ndspChnWaveBufAdd(channel, &wave);

    sound.dspStarted = true;
    sound.dspFinished = false;

    return true;
}

bool SoundPlayer::PlayBufferAndMix(i32 bufIdx, u8 normalize)
{
    SoundData& sound = this->soundBuffers[bufIdx];

    if (sound.dspStarted) {
        RestartSound(bufIdx, normalize);
        return true;
    }

    const float gain = 1.0f / std::max<u8>(normalize, 8);

    for (u32 channel = 2; channel < CHANNEL_COUNT; ++channel) {
        ndspWaveBuf& wave = this->waveBuffers[channel];

        if (wave.status != NDSP_WBUF_FREE &&
            wave.status != NDSP_WBUF_DONE) {
            continue;
        }

        ndspChnSetInterp(channel, NDSP_INTERP_NONE);
        ndspChnSetRate(channel, 22050);
        ndspChnSetFormat(channel, NDSP_FORMAT_MONO_PCM16);

        float mix[12] = {};
        mix[0] = gain;
        mix[1] = gain;
        ndspChnSetMix(channel, mix);

        wave.data_vaddr = sound.samples;
        wave.nsamples = sound.len;
        wave.status = NDSP_WBUF_FREE;

        DSP_FlushDataCache(&wave, sizeof(wave));
        ndspChnWaveBufAdd(channel, &wave);

        sound.dspChannel = channel;
        sound.dspStarted = true;
        sound.dspFinished = false;

        return true;
    }

    // No DSP channel was available. Keep restartRequested true.
    return false;
}

void SoundPlayer::PlaySounds()
{
    i32 idx;
    i32 sndBufIdx;

    if(!g_Supervisor.cfg.playSounds){
        return;
    }

    this->soundBufMutex.lock();

    for (idx = 0; idx < ARRAY_SIZE_SIGNED(this->soundBuffersToPlay); idx++)
    {
        if (this->soundBuffersToPlay[idx] < 0)
        {
            break;
        }

        sndBufIdx = this->soundBuffersToPlay[idx];
        this->soundBuffersToPlay[idx] = -1;

        if (this->soundBuffers[sndBufIdx].samples == NULL)
        {
            continue;
        }

        this->soundBuffers[sndBufIdx].pos = 0;
        this->soundBuffers[sndBufIdx].isPlaying = true;
        this->soundBuffers[sndBufIdx].restartRequested = true;

    }

    this->soundBufMutex.unlock();

}

void SoundPlayer::PlaySoundByIdx(SoundIdx idx)
{
    u32 i;

    for (i = 0; i < ARRAY_SIZE(this->soundBuffersToPlay); i++)
    {
        if (this->soundBuffersToPlay[i] < 0)
        {
            break;
        }

        if (this->soundBuffersToPlay[i] == idx)
        {
            return;
        }
    }

    if (i >= 3)
    {
        return;
    }

    this->soundBuffersToPlay[i] = idx;

}

void SoundPlayer::UpdateFinishedSounds(){

    for (u32 i = 0; i < ARRAY_SIZE(this->soundBuffers); ++i) {
        SoundData& sound = this->soundBuffers[i];

        if (!sound.dspStarted)
            continue;

        const u32 channel = sound.dspChannel;
        ndspWaveBuf& wave = this->waveBuffers[channel];

        if (wave.status == NDSP_WBUF_DONE) {
            sound.dspStarted = false;
            sound.dspFinished = true;
            sound.dspChannel = 1000;

            this->channelActive[channel] = false;
            wave.status = NDSP_WBUF_FREE;
        }
    }
}

void SoundPlayer::MixAudio(u32 samples)
{
    this->soundBufMutex.lock();

    this->UpdateFinishedSounds();

    u8 playingChannels = 0;

    for (int i = 0; i < ARRAY_SIZE_SIGNED(this->soundBuffers); ++i) {
        if (this->soundBuffers[i].isPlaying)
            ++playingChannels;
    }

    // Start or restart requested sounds before advancing pos.
    for (int i = 0; i < ARRAY_SIZE_SIGNED(this->soundBuffers); ++i) {
        SoundData& sound = this->soundBuffers[i];

        if (!sound.isPlaying || !sound.restartRequested)
            continue;

        if (PlayBufferAndMix(i, playingChannels)) {
            sound.restartRequested = false;
        }
    }

    // Advance the game's logical playback position.
    for (int i = 0; i < ARRAY_SIZE_SIGNED(this->soundBuffers); ++i) {
        SoundData& sound = this->soundBuffers[i];

        if (!sound.isPlaying)
            continue;

        sound.pos += std::min(samples, sound.len - sound.pos);

        if (sound.pos >= sound.len) {
            sound.pos = sound.len;
            sound.isPlaying = false;

            // Leave dspStarted unchanged until NDSP_WBUF_DONE.
        }
    }

    this->soundBufMutex.unlock();
}

// EoSD originally just used this function to manage the streaming of the music WAV file.
//   We also use it to mix and queue audio, since we have to do that manually and doing it
//   in a thread keeps sound running continuously, even if the main thread runs into lag
void SoundPlayer::BackgroundMusicPlayerThread()
{
    u64 lastTick = SDL_GetTicks64();
    i32 targetSamples = 0;
    double fractionalSamples = 0.0;

    while (1)
    {
        const u64 curTicks = SDL_GetTicks64();
        const u64 elapsedMs = curTicks - lastTick;
        lastTick = curTicks;

        fractionalSamples += elapsedMs * 22.05;

        const u32 samplesToAdvance = static_cast<u32>(fractionalSamples);

        fractionalSamples -= samplesToAdvance;

        if (samplesToAdvance > 0) {
            this->MixAudio(samplesToAdvance);
        }

        if(this->terminateFlag){
            return;
        }

        SDL_Delay(5);
    }
}
