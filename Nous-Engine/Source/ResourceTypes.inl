#pragma once

#include "Globals.h"

struct Texture 
{
    uint32 id;
    uint32 width;
    uint32 height;
    uint8 channelCount;
    bool hasTransparency;
    uint32 generation;
    void* internalData;
};