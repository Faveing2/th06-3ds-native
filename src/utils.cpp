#ifdef DEBUG
#include <cstdarg>
#include <cstdio>
#endif

#ifdef _3DS_DEBUG
#include <SDL2/SDL_Log.h>
#endif

#include <SDL2/SDL_Log.h>

#include "ZunMath.hpp"
#include "i18n.hpp"
#include "utils.hpp"

namespace utils
{
void DebugPrint(const char *fmt, ...)
{
    //SDL_Log(fmt);
    #ifdef _3DS_DEBUG
    SDL_Log(fmt);
    #endif
}

f32 AddNormalizeAngle(f32 a, f32 b)
{
    i32 i;

    i = 0;
    a += b;
    while (a > ZUN_PI)
    {
        a -= ZUN_2PI;
        if (i++ > 16)
            break;
    }
    while (a < -ZUN_PI)
    {
        a += ZUN_2PI;
        if (i++ > 16)
            break;
    }
    return a;
}

void Rotate(ZunVec3 *outVector, const ZunVec3 *point, f32 angle)
{
    f32 sinOut;
    f32 cosOut;

    sinOut = ZUN_SINF(angle);
    cosOut = ZUN_COSF(angle);
    outVector->x = cosOut * point->x + sinOut * point->y;
    outVector->y = cosOut * point->y - sinOut * point->x;
}

void DebugPrint2(const char *fmt, ...)
{
    //SDL_Log(fmt);
    #ifdef _3DS_DEBUG
    SDL_Log(fmt);
    #endif
}
}; // namespace utils
