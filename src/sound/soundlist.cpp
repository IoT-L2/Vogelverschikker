#include "soundlist.h"
#include "simpelconfig.h"
#include "esp_log.h"
#include <cstdlib>

#define TAG "SOUNDLIST"

extern SimpleConfig audio_cfg;

static const char* SOUND_LIST[] = {
    "tetrismusic.wav",  // SOUND_TETRIS
    "sound.wav",      // SOUND_DEFAULT
};

const char* sound_to_filename(SoundIndex index)
{
    if (index >= SOUND_COUNT) return SOUND_LIST[SOUND_TETRIS];
    return SOUND_LIST[index];
}

SoundIndex sound_get_current()
{
    char filename[64];
    if (!audio_cfg.getLine(0, filename, sizeof(filename))) {
        return SOUND_TETRIS; // default
    }

    // match filename back to enum
    for (int i = 0; i < SOUND_COUNT; i++) {
        if (strcmp(filename, SOUND_LIST[i]) == 0) {
            return (SoundIndex)i;
        }
    }
    return SOUND_TETRIS; // fallback
}

void sound_switch(SoundIndex index)
{
    const char *filename = sound_to_filename(index);
    ESP_LOGI(TAG, "Switching sound to: %s", filename);
    audio_cfg.setLine(0, filename);
}

void sound_cycle()
{
    SoundIndex current = sound_get_current();
    SoundIndex next    = (SoundIndex)((current + 1) % SOUND_COUNT);
    ESP_LOGI(TAG, "Cycling sound: %d -> %d", current, next);
    sound_switch(next);
}