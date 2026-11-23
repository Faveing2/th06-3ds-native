#include "GameErrorContext.hpp"
#include "FileSystem.hpp"
#include <SDL2/SDL_messagebox.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include <3ds.h>

GameErrorContext g_GameErrorContext;

const char *GameErrorContext::Log(const char *fmt, ...)
{
    SDL_Log(fmt);
    // char tmpBuffer[512];
    // size_t tmpBufferSize;
    // va_list args;

    // va_start(args, fmt);
    // std::vsprintf(tmpBuffer, fmt, args);

    // tmpBufferSize = std::strlen(tmpBuffer);

    // if (this->m_BufferEnd + tmpBufferSize < &this->m_Buffer[sizeof(this->m_Buffer) - 1])
    // {
    //     std::strcpy(this->m_BufferEnd, tmpBuffer);

    //     this->m_BufferEnd += tmpBufferSize;
    //     *this->m_BufferEnd = '\0';
    // }

    // va_end(args);

    return fmt;
}

const char *GameErrorContext::Fatal(const char *fmt, ...)
{
    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, fmt);
    errorConf err;
    
    errorInit(&err, ERROR_TEXT, CFG_LANGUAGE_EN);
    
    // Doesn't actually support printf stuff %s, however it'll still work for now
    errorText(&err, fmt);
    
    errorDisp(&err);

    // char tmpBuffer[512];
    // size_t tmpBufferSize;
    // va_list args;

    // va_start(args, fmt);
    // std::vsprintf(tmpBuffer, fmt, args);

    // tmpBufferSize = std::strlen(tmpBuffer);

    // if (this->m_BufferEnd + tmpBufferSize < &this->m_Buffer[sizeof(this->m_Buffer) - 1])
    // {
    //     std::strcpy(this->m_BufferEnd, tmpBuffer);

    //     this->m_BufferEnd += tmpBufferSize;
    //     *this->m_BufferEnd = '\0';
    // }

    // va_end(args);

    // this->m_ShowMessageBox = true;

    return fmt;
}

void GameErrorContext::Flush()
{
    if(this->logFile){
        std::fflush(this->logFile);
    }

    // if (m_BufferEnd != m_Buffer)
    // {
    //     g_GameErrorContext.Log(TH_ERR_LOGGER_END);

    //     if (m_ShowMessageBox)
    //     {
    //         SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "log", m_Buffer, NULL);
    //     }

    //     //logFile = FileSystem::FopenUTF8("/th06/log.txt", "w");

    //     // std::fprintf(logFile, "%s", m_Buffer);
    //     // std::fclose(logFile);
    // }
}
