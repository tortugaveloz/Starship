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
    if (!gAudio3DEnabled) {
        return;
    }
    
    // Set listener position
    Audio3D_SetListenerPosition(camX, camY, camZ);
    
    // Calculate forward direction from eye to at point
    float fwdX = atX - camX;
    float fwdY = atY - camY;
    float fwdZ = atZ - camZ;
    
    // Normalize forward vector
    float fwdLen = sqrtf(fwdX * fwdX + fwdY * fwdY + fwdZ * fwdZ);
    if (fwdLen > 0.0001f) {
        fwdX /= fwdLen;
        fwdY /= fwdLen;
        fwdZ /= fwdLen;
    } else {
        // Default forward direction
        fwdX = 0.0f;
        fwdY = 0.0f;
        fwdZ = -1.0f;
    }
    
    // Set listener orientation
    Audio3D_SetListenerOrientation(fwdX, fwdY, fwdZ, upX, upY, upZ);
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
}
