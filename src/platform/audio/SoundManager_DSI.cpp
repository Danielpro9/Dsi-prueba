#include "net/minecraft/src/SoundManager.h"
#include "platform/Log.h"

#ifdef DSI_PLATFORM

// No DSi audio backend exists yet -- deferred along with input/controls, not
// an oversight. This is the exact same NO_SOUND stub every other platform's
// SoundManager_*.cpp already falls back to when built with -DNO_SOUND (see
// src/dsi/Makefile), so this file adds no new behaviour, just opts DSi into
// the existing one.

void *SoundManager::sndSystem = nullptr;
bool  SoundManager::loaded    = false;

SoundManager::SoundManager()
    : soundPoolSounds(), soundPoolStreaming(), soundPoolMusic(), soundVolume(0),
      options(nullptr), rand(), ticksBeforeMusic(0)
{
}

void SoundManager::loadSoundSettings(GameSettings *gamesettings) { options = gamesettings; loaded = false; }
void SoundManager::onSoundOptionsChanged() {}
void SoundManager::closeMinecraft() { loaded = false; }
void SoundManager::addSound(const jstring &s, const std::string &file) { soundPoolSounds.addSound(s, file); }
void SoundManager::addStreaming(const jstring &s, const std::string &file) { soundPoolStreaming.addSound(s, file); }
void SoundManager::addMusic(const jstring &s, const std::string &file) { soundPoolMusic.addSound(s, file); }
void SoundManager::playRandomMusicIfReady() {}
bool SoundManager::playMusicFileNow(const std::string &) { return false; }
void SoundManager::setListenerPosition(EntityLiving *, float) {}
void SoundManager::playStreaming(const jstring &, float, float, float, float, float) {}
void SoundManager::playSound(const jstring &, float, float, float, float, float) {}
void SoundManager::playSoundFX(const jstring &, float, float) {}
void SoundManager::tryToSetLibraryAndCodecs() {}

#endif // DSI_PLATFORM
