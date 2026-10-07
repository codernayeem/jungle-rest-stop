// =============================================================================
//  Audio.h  -  A few real recorded sounds (free CC0 recordings, see
//  assets/sounds/CREDITS.txt), played with the Windows waveOut API.
//    * every sound is a small 44.1 kHz mono WAV file, loaded once at start-up
//    * a small mixer thread adds the playing sounds together and sends them
//      to the sound card
//    * sounds in the world get quieter with distance from the player
// =============================================================================
#pragma once
#include "../core/Math3D.h"

enum Sound {
    SND_CLICK,      // any button / switch
    SND_BEEP,       // keypad beep (ATM, coffee machine ready)
    SND_COIN,       // coin or note inserted
    SND_COIN_DROP,  // change falling into the coin cup
    SND_THUD,       // product hitting the tray
    SND_MOTOR,      // vending spiral turning
    SND_CASH,       // ATM counting out bills
    SND_GRIND,      // coffee grinder
    SND_POUR,       // coffee pouring
    SND_DOOR,       // sliding entrance door
    SND_CAR_DOOR,   // car door opening
    SND_CAR_CLOSE,  // car door closing
    SND_COUNT
};

namespace Audio {
bool init();
void shutdown();
void setListener(const Vec3& pos);
void play(Sound s, float volume = 1.0f);                      // not positioned (player's own actions)
void playAt(Sound s, const Vec3& pos, float volume = 1.0f);   // quieter with distance
// background: birds by day, crickets at night; quieter indoors
void setAmbience(float dayLevel, float nightLevel);
void toggleMute();
bool muted();
}  // namespace Audio
