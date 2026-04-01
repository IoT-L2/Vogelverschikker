#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    void init_sound(void);
    void trigger_sound(void);
    void trigger_sound_from_isr(void); // NIEUW: Voor de knop!

#ifdef __cplusplus
}
#endif