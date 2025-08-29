#pragma once

//
// TODO(Kevin): Convert all of these to platform-efficient versions
// and remove math.h
//

#include "math.h"

inline int32
RoundReal32ToInt32(real32 Real32)
{
    // TODO(Kevin): Intrinsic?
    if (Real32 > 0.f)
        return (int32)(Real32 + 0.5f);
    else if (Real32 < 0.f)
        return (int32)(Real32 - 0.5f);
    else
        return 0;
}

inline int32
FloorReal32ToInt32(real32 Real32)
{
    int32 Result = (int32)floorf(Real32);
    return Result;
}

inline int32
TruncateReal32ToInt32(real32 Real32)
{
    int32 Result = (int32)Real32;
    return Result;
}

inline real32
Sin(real32 Angle)
{
    real32 Result = sinf(Angle);
    return(Result);
}

inline real32
Cos(real32 Angle)
{
    real32 Result = cosf(Angle);
    return(Result);
}

inline real32
ATan2(real32 Y, real32 X)
{
    real32 Result = atan2f(Y, X);
    return(Result);
}


