#pragma once
#include <cmath>
#include "constants.h"

enum LFOTarget {
    LFO_OFF = 0,
    LFO_RATIO1, LFO_RATIO2, LFO_RATIO3, LFO_RATIO4,
    LFO_INDEX1, LFO_INDEX2, LFO_INDEX3, LFO_INDEX4,
    LFO_FILTER_CUT, LFO_FILTER_Q,
    LFO_CHORUS, LFO_REVERB,
    LFO_TARGET_COUNT
};

inline const char* lfoTargetNames[] = {
    "OFF", "Ratio1", "Ratio2", "Ratio3", "Ratio4",
    "Index1", "Index2", "Index3", "Index4",
    "Filter", "Res", "Chorus", "Reverb"
};

// Mod Envelope targets
enum ModEnvTarget {
    MODENV_OFF = 0,
    MODENV_INDEX1, MODENV_INDEX2, MODENV_INDEX3, MODENV_INDEX4,
    MODENV_FILTER_CUT,
    MODENV_TARGET_COUNT
};

inline const char* modEnvTargetNames[] = {
    "OFF", "Idx1", "Idx2", "Idx3", "Idx4", "Filter"
};

class LFO {
private:
    double phase;
    double sampleRate;

public:
    LFO(double sr) : phase(0.0), sampleRate(sr) {}

    double process(double rate, double depth) {
        double output = std::sin(phase * TWO_PI) * depth;
        phase += rate / sampleRate;
        if (phase >= 1.0) phase -= 1.0;
        return output;
    }

    void reset() { phase = 0.0; }
};

// Helper para aplicar modulacion LFO
inline float applyLfoMod(float baseValue, int target, int lfoTarget, float lfoValue, float minVal, float maxVal) {
    if (target != lfoTarget || lfoTarget == LFO_OFF) return baseValue;
    float range = maxVal - minVal;
    return std::max(minVal, std::min(maxVal, baseValue + lfoValue * range * 0.5f));
}
