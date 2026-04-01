#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    void init_sound(void);
    void play_sound(const char *filename);
    bool is_sound_playing(void);
    void stop_sound(void);

#ifdef __cplusplus
}
#endif