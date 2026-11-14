#pragma once

#include <SDL2/SDL.h>
#include <cstdio>
#include "i18n.hpp"
#include "inttypes.hpp"

#define LOGFILEPATH "sdmc:/th06/th06.log"

// static void FlushLogFile()
// {
//     if (logFile) {
//         std::fflush(logFile);
//     }
// }

static void SDLCALL LogToFile(void* userdata, int category, SDL_LogPriority priority, const char* message){

    static int logCount = 0;

    FILE* file = static_cast<FILE*>(userdata);

    if(file) {
        std::fprintf(file, "[category=%d priority=%d] %s\n", category, static_cast<int>(priority), message);
    }

    std::fflush(file);
}

class GameErrorContext;

class GameErrorContext
{
  public:
    char m_Buffer[0x800];
    char *m_BufferEnd;
    i8 m_ShowMessageBox;
    FILE* logFile;

    GameErrorContext()
    {
        logFile = std::fopen(LOGFILEPATH, "w");

        if(logFile){
            SDL_LogSetOutputFunction(LogToFile, logFile);
        }

        m_BufferEnd = m_Buffer;
        m_Buffer[0] = '\0';
        // Required to get some mov eax, [m_Buffer_ptr]
        m_ShowMessageBox = false;
        Log(TH_ERR_LOGGER_START);
    }

    ~GameErrorContext()
    {
    }

    void ResetContext()
    {
        m_BufferEnd = m_Buffer;
        m_BufferEnd[0] = '\0';
        // TODO: check if it should be m_Buffer[0] above.
    }

    void Flush();

    const char *Fatal(const char *fmt, ...);
    const char *Log(const char *fmt, ...);
};

extern GameErrorContext g_GameErrorContext;
