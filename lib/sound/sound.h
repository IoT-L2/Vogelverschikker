#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    // Initialiseert de I2C, het bestandssysteem en start de taak op Core 1
    void init_sound(void);

    // Roep deze functie aan in je interrupt (ISR) om het geluid te starten
    void trigger_sound_from_isr(void);

#ifdef __cplusplus
}
#endif