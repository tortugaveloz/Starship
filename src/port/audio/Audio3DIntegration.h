#pragma once

/**
 * Audio3D Integration for Starship
 * 
 * This module provides the bridge between Starfox 64's audio system
 * and libultraship's 3D audio capabilities (via OpenAL).
 * 
 * When OpenAL backend is active, 3D spatialization is enabled.
 * For SDL/WASAPI backends, the game continues to use its original
 * pan-based audio positioning.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * Initialize 3D audio system.
 * Called once at game startup.
 */
void Audio3DIntegration_Init(void);

/**
 * Shutdown 3D audio system.
 * Called at game exit.
 */
void Audio3DIntegration_Shutdown(void);

/**
 * Check if 3D audio is available and enabled.
 */
bool Audio3DIntegration_IsEnabled(void);

/**
 * Update listener (camera) position each frame.
 * Call this with the camera/player position and orientation.
 * 
 * @param camX, camY, camZ - Camera world position
 * @param atX, atY, atZ - Camera forward direction
 * @param upX, upY, upZ - Camera up direction
 */
void Audio3DIntegration_UpdateListener(
    float camX, float camY, float camZ,
    float atX, float atY, float atZ,
    float upX, float upY, float upZ
);

/**
 * Update listener from game's camera variables.
 * Convenience function that reads directly from the game's global camera state.
 */
void Audio3DIntegration_UpdateListenerFromGame(void);

/**
 * Main update function - call once per frame.
 * Performs cleanup and maintenance.
 */
void Audio3DIntegration_Update(void);

/**
 * Set distance model and parameters.
 * 
 * @param referenceDistance - Distance at which sounds are at full volume
 * @param maxDistance - Maximum hearing distance
 * @param rolloff - How fast volume drops with distance (1.0 = normal)
 */
void Audio3DIntegration_SetDistanceParams(
    float referenceDistance,
    float maxDistance,
    float rolloff
);

/**
 * Called from audio mixer when a new SFX starts playing with position.
 * This allows us to create a 3D source for the sound.
 * 
 * @param sfxId - Sound effect ID
 * @param posX, posY, posZ - World position of the sound
 * @param volume - Initial volume (0.0-1.0)
 * @param pitch - Pitch multiplier (1.0 = normal)
 */
void Audio3DIntegration_OnSfxPlay(
    uint32_t sfxId,
    float posX, float posY, float posZ,
    float volume, float pitch
);

/**
 * Update position of an existing sound source.
 * Called when a sound's position changes.
 * 
 * @param sfxId - Sound effect ID
 * @param posX, posY, posZ - New world position
 */
void Audio3DIntegration_UpdateSfxPosition(
    uint32_t sfxId,
    float posX, float posY, float posZ
);

/**
 * Update volume/pitch of an existing sound source.
 * 
 * @param sfxId - Sound effect ID
 * @param volume - New volume (0.0-1.0)
 * @param pitch - New pitch multiplier
 */
void Audio3DIntegration_UpdateSfxProperties(
    uint32_t sfxId,
    float volume, float pitch
);

/**
 * Called when a SFX stops playing.
 * 
 * @param sfxId - Sound effect ID
 */
void Audio3DIntegration_OnSfxStop(uint32_t sfxId);

/**
 * Stop all 3D sources.
 */
void Audio3DIntegration_StopAll(void);

// ============================================================================
// 3D Position-Based Audio Calculations
// These functions calculate proper 3D spatialization values that can be used
// by the game's audio mixer for accurate positional audio.
// ============================================================================

/**
 * Get 3D pan value for a sound at the given world position.
 * Uses true 3D math based on listener orientation.
 * 
 * @param srcX, srcY, srcZ - Sound source world position
 * @return Pan value: -1.0 (full left) to +1.0 (full right), 0.0 = center
 */
float Audio3D_GetPan3D(float srcX, float srcY, float srcZ);

/**
 * Get distance from listener to source.
 * 
 * @param srcX, srcY, srcZ - Sound source world position
 * @return Distance in game units
 */
float Audio3D_GetDistance(float srcX, float srcY, float srcZ);

/**
 * Get volume attenuation based on distance.
 * Uses inverse distance attenuation model with configured reference/max distances.
 * 
 * @param distance - Distance from listener to source
 * @return Attenuation: 0.0 (silent) to 1.0 (full volume)
 */
float Audio3D_GetAttenuation(float distance);

/**
 * Get front/back factor for a sound position.
 * Useful for surround sound and reverb calculations.
 * 
 * @param srcX, srcY, srcZ - Sound source world position
 * @return Factor: +1.0 (in front) to -1.0 (behind), 0.0 = to the side
 */
float Audio3D_GetFrontBack(float srcX, float srcY, float srcZ);

// ============================================================================
// C Wrappers for game code integration
// ============================================================================

void Audio3D_Integration_Init(void);
void Audio3D_Integration_Shutdown(void);
bool Audio3D_Integration_IsEnabled(void);
void Audio3D_Integration_Update(void);
void Audio3D_Integration_OnSfxPlay(uint32_t sfxId, float posX, float posY, float posZ, float volume, float pitch);
void Audio3D_Integration_UpdateSfxPosition(uint32_t sfxId, float posX, float posY, float posZ);
void Audio3D_Integration_OnSfxStop(uint32_t sfxId);

/**
 * Check if OpenAL should handle this 3D positioned audio.
 * Returns true if OpenAL 3D is active and can handle the sound.
 * 
 * @param posX, posY, posZ - 3D position of the sound
 * @param distance - Distance from listener
 * @return true if OpenAL should handle this, false to use software mixer
 */
bool Audio3D_ShouldUseOpenAL(float posX, float posY, float posZ, float distance);

/**
 * Get the OpenAL source ID for an SFX ID.
 * Returns 0 if not found or OpenAL 3D is not active.
 * 
 * @param sfxId - Sound effect ID
 * @return OpenAL source ID, or 0 if not found
 */
uint32_t Audio3D_GetSourceForSfx(uint32_t sfxId);

/**
 * Queue audio samples to the OpenAL source for an SFX.
 * Call this instead of aEnvMixer when OpenAL should handle the sound.
 * 
 * @param sfxId - Sound effect ID (used to find the OpenAL source)
 * @param samples - Audio sample data (mono s16)
 * @param numSamples - Number of samples
 * @param sampleRate - Sample rate in Hz (typically 32000)
 * @param volume - Volume multiplier (0.0-1.0)
 * @param pitch - Pitch multiplier (1.0 = normal)
 * @return true if samples were queued, false if source not found
 */
bool Audio3D_QueueSfxSamples(uint32_t sfxId, const int16_t* samples, uint32_t numSamples,
                             uint32_t sampleRate, float volume, float pitch);

#ifdef __cplusplus
}
#endif
