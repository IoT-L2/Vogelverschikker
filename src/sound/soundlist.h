#pragma once
#include <stdint.h>

typedef enum : uint8_t {
    SOUND_TETRIS  = 0,
    SOUND_DEFAULT = 1,
    SOUND_ALARM   = 2,
    SOUND_COUNT
} SoundIndex;

const char*  sound_to_filename(SoundIndex index);
SoundIndex   sound_get_current();
void         sound_switch(SoundIndex index);
void         sound_cycle();