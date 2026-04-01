#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    /**
    * Initializes the sound system.
    */
    void init_sound(void);

    /**
   * Plays a WAV file from SPIFFS.
   */
    void play_sound(const char *filename);

    /**
   * Checks whether audio is currently playing.
   * @return true if a sound is currently being played, false otherwise.
   */
    bool is_sound_playing(void);

    /**
   *  Stops the currently playing sound.
   * Immediately halts playback and clears the audio queue.
   */
    void stop_sound(void);

#ifdef __cplusplus
}
#endif