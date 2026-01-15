#include "Audio3DIntegration.h"
#include <libultraship.h>
#include <unordered_map>
#include <cmath>

// Import only the camera variables we need from the game
// (avoiding global.h which has C-only function declarations with 'this' parameter names)
extern "C" {
    // Vec3f structure from sf64math.h
    typedef struct {
        float x, y, z;
    } Vec3f;

    // Camera variables from sf64context.h
    extern Vec3f gPlayCamEye;
    extern Vec3f gPlayCamAt;
}

// Track active 3D sources by SFX ID
static std::unordered_map<uint32_t, Audio3DSourceId> g3DSources;
static bool gAudio3DInitialized = false;
static bool gAudio3DEnabled = false;

// Distance parameters (game units)
static float gReferenceDistance = 500.0f;  // Full volume distance
static float gMaxDistance = 10000.0f;      // Maximum hearing distance
static float gRolloff = 1.0f;              // Rolloff factor

// Cached listener state for 3D calculations
static Vec3f sListenerPos = {0, 0, 0};
static Vec3f sListenerForward = {0, 0, -1};
static Vec3f sListenerRight = {1, 0, 0};
static Vec3f sListenerUp = {0, 1, 0};

// Helper functions for 3D math
static void NormalizeVec3(float* x, float* y, float* z) {
    float len = sqrtf((*x) * (*x) + (*y) * (*y) + (*z) * (*z));
    if (len > 0.0001f) {
        *x /= len;
        *y /= len;
        *z /= len;
    }
}

static void CrossProduct(float ax, float ay, float az, float bx, float by, float bz,
                         float* rx, float* ry, float* rz) {
    *rx = ay * bz - az * by;
    *ry = az * bx - ax * bz;
    *rz = ax * by - ay * bx;
}

void Audio3DIntegration_Init(void) {
    if (gAudio3DInitialized) {
        return;
    }

    gAudio3DInitialized = true;
    
    // Initialize 3D audio system
    if (Audio3D_Init()) {
        gAudio3DEnabled = true;
        
        // Configure distance model
        Audio3D_SetAttenuationModel(AUDIO3D_ATTENUATION_INVERSE_CLAMPED);
        
        // Disable Doppler for now (can be enabled if desired)
        Audio3D_SetDopplerFactor(0.0f);
        
        SPDLOG_INFO("Audio3D: Integration initialized successfully");
    } else {
        gAudio3DEnabled = false;
        SPDLOG_INFO("Audio3D: 3D audio not available, using standard pan-based audio");
    }
}

void Audio3DIntegration_Shutdown(void) {
    if (!gAudio3DInitialized) {
        return;
    }
    
    Audio3DIntegration_StopAll();
    Audio3D_Shutdown();
    gAudio3DInitialized = false;
    gAudio3DEnabled = false;
}

bool Audio3DIntegration_IsEnabled(void) {
    return gAudio3DEnabled;
}

void Audio3DIntegration_UpdateListener(
    float camX, float camY, float camZ,
    float atX, float atY, float atZ,
    float upX, float upY, float upZ
) {
    // Always cache listener state for pan calculations (even if 3D audio API isn't available)
    sListenerPos.x = camX;
    sListenerPos.y = camY;
    sListenerPos.z = camZ;
    
    // Calculate forward direction from eye to at point
    sListenerForward.x = atX - camX;
    sListenerForward.y = atY - camY;
    sListenerForward.z = atZ - camZ;
    NormalizeVec3(&sListenerForward.x, &sListenerForward.y, &sListenerForward.z);
    
    // Store up vector
    sListenerUp.x = upX;
    sListenerUp.y = upY;
    sListenerUp.z = upZ;
    NormalizeVec3(&sListenerUp.x, &sListenerUp.y, &sListenerUp.z);
    
    // Calculate right vector (cross product of up and forward for left-handed game coords)
    CrossProduct(sListenerUp.x, sListenerUp.y, sListenerUp.z,
                 sListenerForward.x, sListenerForward.y, sListenerForward.z,
                 &sListenerRight.x, &sListenerRight.y, &sListenerRight.z);
    NormalizeVec3(&sListenerRight.x, &sListenerRight.y, &sListenerRight.z);
    
    if (!gAudio3DEnabled) {
        return;
    }
    
    // Set listener position in 3D audio API
    Audio3D_SetListenerPosition(camX, camY, camZ);
    Audio3D_SetListenerOrientation(sListenerForward.x, sListenerForward.y, sListenerForward.z, 
                                    upX, upY, upZ);
}

void Audio3DIntegration_UpdateListenerFromGame(void) {
    if (!gAudio3DEnabled) {
        return;
    }
    
    // Use the game's camera position (gPlayCamEye) and target (gPlayCamAt)
    // Default up vector is (0, 1, 0)
    Audio3DIntegration_UpdateListener(
        gPlayCamEye.x, gPlayCamEye.y, gPlayCamEye.z,
        gPlayCamAt.x, gPlayCamAt.y, gPlayCamAt.z,
        0.0f, 1.0f, 0.0f
    );
}

void Audio3DIntegration_Update(void) {
    if (!gAudio3DEnabled) {
        return;
    }
    
    // Update listener from game camera each frame
    Audio3DIntegration_UpdateListenerFromGame();
    
    // Let Audio3D clean up finished one-shot sources
    Audio3D_Update();
}

void Audio3DIntegration_SetDistanceParams(
    float referenceDistance,
    float maxDistance,
    float rolloff
) {
    gReferenceDistance = referenceDistance;
    gMaxDistance = maxDistance;
    gRolloff = rolloff;
    
    // Update existing sources
    if (gAudio3DEnabled) {
        for (const auto& pair : g3DSources) {
            Audio3D_SetSourceReferenceDistance(pair.second, referenceDistance);
            Audio3D_SetSourceMaxDistance(pair.second, maxDistance);
            Audio3D_SetSourceRolloff(pair.second, rolloff);
        }
    }
}

void Audio3DIntegration_OnSfxPlay(
    uint32_t sfxId,
    float posX, float posY, float posZ,
    float volume, float pitch
) {
    if (!gAudio3DEnabled) {
        return;
    }
    
    // Check if we already have a source for this SFX
    auto it = g3DSources.find(sfxId);
    if (it != g3DSources.end()) {
        // Update existing source
        Audio3D_SetSourcePosition(it->second, posX, posY, posZ);
        Audio3D_SetSourceGain(it->second, volume);
        Audio3D_SetSourcePitch(it->second, pitch);
        return;
    }
    
    // Create new 3D source
    Audio3DSourceId sourceId = Audio3D_CreateSource();
    if (sourceId == 0) {
        return;
    }
    
    // Configure source
    Audio3D_SetSourcePosition(sourceId, posX, posY, posZ);
    Audio3D_SetSourceGain(sourceId, volume);
    Audio3D_SetSourcePitch(sourceId, pitch);
    Audio3D_SetSourceReferenceDistance(sourceId, gReferenceDistance);
    Audio3D_SetSourceMaxDistance(sourceId, gMaxDistance);
    Audio3D_SetSourceRolloff(sourceId, gRolloff);
    
    // Track the source
    g3DSources[sfxId] = sourceId;
}

void Audio3DIntegration_UpdateSfxPosition(
    uint32_t sfxId,
    float posX, float posY, float posZ
) {
    if (!gAudio3DEnabled) {
        return;
    }
    
    auto it = g3DSources.find(sfxId);
    if (it != g3DSources.end()) {
        Audio3D_SetSourcePosition(it->second, posX, posY, posZ);
    }
}

void Audio3DIntegration_UpdateSfxProperties(
    uint32_t sfxId,
    float volume, float pitch
) {
    if (!gAudio3DEnabled) {
        return;
    }
    
    auto it = g3DSources.find(sfxId);
    if (it != g3DSources.end()) {
        Audio3D_SetSourceGain(it->second, volume);
        Audio3D_SetSourcePitch(it->second, pitch);
    }
}

void Audio3DIntegration_OnSfxStop(uint32_t sfxId) {
    if (!gAudio3DEnabled) {
        return;
    }
    
    auto it = g3DSources.find(sfxId);
    if (it != g3DSources.end()) {
        Audio3D_DestroySource(it->second);
        g3DSources.erase(it);
    }
}

void Audio3DIntegration_StopAll(void) {
    if (!gAudio3DEnabled) {
        return;
    }
    
    // Destroy all tracked sources
    for (const auto& pair : g3DSources) {
        Audio3D_DestroySource(pair.second);
    }
    g3DSources.clear();
    
    // Also tell Audio3D to destroy any remaining sources
    Audio3D_DestroyAllSources();
}

// ============================================================================
// 3D Pan and Volume Calculations
// These functions compute proper 3D spatialization using listener/source positions
// ============================================================================

/**
 * Calculate pan value based on true 3D position relative to listener.
 * Returns: -1.0 (full left) to +1.0 (full right), 0.0 = center
 */
float Audio3DIntegration_Calculate3DPan(float srcX, float srcY, float srcZ) {
    // Calculate vector from listener to source
    float toSrcX = srcX - sListenerPos.x;
    float toSrcY = srcY - sListenerPos.y;
    float toSrcZ = srcZ - sListenerPos.z;
    
    // Distance for normalization
    float dist = sqrtf(toSrcX * toSrcX + toSrcY * toSrcY + toSrcZ * toSrcZ);
    if (dist < 0.001f) {
        return 0.0f; // Source at listener position = center
    }
    
    // Normalize
    toSrcX /= dist;
    toSrcY /= dist;
    toSrcZ /= dist;
    
    // Calculate dot product with listener's right vector
    // Positive = source is to the right
    // Negative = source is to the left
    float rightDot = toSrcX * sListenerRight.x + 
                     toSrcY * sListenerRight.y + 
                     toSrcZ * sListenerRight.z;
    
    // Clamp to [-1, 1]
    if (rightDot < -1.0f) rightDot = -1.0f;
    if (rightDot > 1.0f) rightDot = 1.0f;
    
    return rightDot;
}

/**
 * Calculate distance from listener to source.
 */
float Audio3DIntegration_CalculateDistance(float srcX, float srcY, float srcZ) {
    float dx = srcX - sListenerPos.x;
    float dy = srcY - sListenerPos.y;
    float dz = srcZ - sListenerPos.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

/**
 * Calculate volume attenuation based on distance.
 * Uses inverse distance attenuation model.
 * Returns: 0.0 to 1.0
 */
float Audio3DIntegration_CalculateAttenuation(float distance) {
    if (distance <= gReferenceDistance) {
        return 1.0f;
    }
    
    if (distance >= gMaxDistance) {
        return 0.0f;
    }
    
    // Inverse distance formula: refDist / (refDist + rolloff * (dist - refDist))
    float attenuation = gReferenceDistance / 
                        (gReferenceDistance + gRolloff * (distance - gReferenceDistance));
    
    return attenuation;
}

/**
 * Calculate front/back factor based on 3D position.
 * Returns: 1.0 (in front) to -1.0 (behind), 0.0 = to the side
 */
float Audio3DIntegration_Calculate3DFrontBack(float srcX, float srcY, float srcZ) {
    float toSrcX = srcX - sListenerPos.x;
    float toSrcY = srcY - sListenerPos.y;
    float toSrcZ = srcZ - sListenerPos.z;
    
    float dist = sqrtf(toSrcX * toSrcX + toSrcY * toSrcY + toSrcZ * toSrcZ);
    if (dist < 0.001f) {
        return 1.0f; // At listener = in front
    }
    
    toSrcX /= dist;
    toSrcY /= dist;
    toSrcZ /= dist;
    
    // Dot product with forward vector
    float forwardDot = toSrcX * sListenerForward.x + 
                       toSrcY * sListenerForward.y + 
                       toSrcZ * sListenerForward.z;
    
    return forwardDot;
}

// C wrapper for use from game code
extern "C" {
    void Audio3D_Integration_Init(void) {
        Audio3DIntegration_Init();
    }
    
    void Audio3D_Integration_Shutdown(void) {
        Audio3DIntegration_Shutdown();
    }
    
    bool Audio3D_Integration_IsEnabled(void) {
        return Audio3DIntegration_IsEnabled();
    }
    
    void Audio3D_Integration_Update(void) {
        Audio3DIntegration_Update();
    }
    
    void Audio3D_Integration_OnSfxPlay(uint32_t sfxId, float posX, float posY, float posZ, float volume, float pitch) {
        Audio3DIntegration_OnSfxPlay(sfxId, posX, posY, posZ, volume, pitch);
    }
    
    void Audio3D_Integration_UpdateSfxPosition(uint32_t sfxId, float posX, float posY, float posZ) {
        Audio3DIntegration_UpdateSfxPosition(sfxId, posX, posY, posZ);
    }
    
    void Audio3D_Integration_OnSfxStop(uint32_t sfxId) {
        Audio3DIntegration_OnSfxStop(sfxId);
    }
    
    /**
     * Get 3D pan value for a sound at the given world position.
     * Returns: -1.0 (full left) to +1.0 (full right)
     */
    float Audio3D_GetPan3D(float srcX, float srcY, float srcZ) {
        return Audio3DIntegration_Calculate3DPan(srcX, srcY, srcZ);
    }
    
    /**
     * Get distance from listener to source.
     */
    float Audio3D_GetDistance(float srcX, float srcY, float srcZ) {
        return Audio3DIntegration_CalculateDistance(srcX, srcY, srcZ);
    }
    
    /**
     * Get volume attenuation based on distance.
     */
    float Audio3D_GetAttenuation(float distance) {
        return Audio3DIntegration_CalculateAttenuation(distance);
    }
    
    /**
     * Get front/back factor (-1 to 1) for surround/reverb calculations.
     */
    float Audio3D_GetFrontBack(float srcX, float srcY, float srcZ) {
        return Audio3DIntegration_Calculate3DFrontBack(srcX, srcY, srcZ);
    }
    
    /**
     * Check if OpenAL should handle this 3D positioned audio.
     * Returns true if OpenAL 3D is active and the position data is valid.
     */
    bool Audio3D_ShouldUseOpenAL(float posX, float posY, float posZ, float distance) {
        // Check if 3D audio is enabled and we have valid position data
        if (!gAudio3DEnabled) {
            return false;
        }
        // Check for valid position (not all zeros which indicates uninitialized)
        if (posX == 0.0f && posY == 0.0f && posZ == 0.0f && distance == 0.0f) {
            return false;
        }
        return true;
    }
    
    /**
     * Get the OpenAL source ID for an SFX ID.
     */
    uint32_t Audio3D_GetSourceForSfx(uint32_t sfxId) {
        if (!gAudio3DEnabled) {
            return 0;
        }
        auto it = g3DSources.find(sfxId);
        if (it != g3DSources.end()) {
            return it->second;
        }
        return 0;
    }
    
    /**
     * Queue audio samples to the OpenAL source for an SFX.
     * This sends the synthesized audio directly to OpenAL for 3D spatialization.
     */
    bool Audio3D_QueueSfxSamples(uint32_t sfxId, const int16_t* samples, uint32_t numSamples,
                                 uint32_t sampleRate, float volume, float pitch) {
        if (!gAudio3DEnabled || samples == nullptr || numSamples == 0) {
            printf("Audio3D_QueueSfxSamples: early return - enabled=%d samples=%p numSamples=%u\n",
                   gAudio3DEnabled, (void*)samples, numSamples);
            return false;
        }
        
        auto it = g3DSources.find(sfxId);
        if (it == g3DSources.end()) {
            printf("Audio3D_QueueSfxSamples: no source for sfxId=0x%08X (have %zu sources)\n", 
                   sfxId, g3DSources.size());
            return false;
        }
        
        Audio3DSourceId sourceId = it->second;
        printf("Audio3D_QueueSfxSamples: sfxId=0x%08X sourceId=%u numSamples=%u rate=%u vol=%.2f pitch=%.2f\n",
               sfxId, sourceId, numSamples, sampleRate, volume, pitch);
        
        // Update volume and pitch
        Audio3D_SetSourceGain(sourceId, volume);
        Audio3D_SetSourcePitch(sourceId, pitch);
        
        // Queue the samples (mono, 16-bit)
        Audio3D_QueueBuffer(sourceId, samples, numSamples * sizeof(int16_t), sampleRate, 1);
        
        // Start playback if not already playing
        Audio3DSourceState state = Audio3D_GetSourceState(sourceId);
        if (state != AUDIO3D_STATE_PLAYING) {
            Audio3D_PlaySource(sourceId);
        }
        
        return true;
    }
}
