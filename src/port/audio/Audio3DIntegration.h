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

#ifdef __cplusplus
}
#endif
