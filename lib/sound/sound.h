#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    // Initialiseert SPIFFS, I2C en start de audiotaak
    void init_sound(void);

    // Simpele functie om het geluid te starten (geen interrupt nodig!)
    void trigger_sound(void);

#ifdef __cplusplus
}
#endif