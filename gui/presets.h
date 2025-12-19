#pragma once
#include <cstdio>
#include "../synth/lfo.h"

const int NUM_PRESETS = 8;

struct Preset {
    char name[16];
    float ratio1, ratio2, ratio3, ratio4;
    float index1, index2, index3, index4;
    float attack, decay, sustain, release;
    int algorithm;
    float filterCutoff, filterQ;
    int filterType;
    float chorus, reverb;
    float lfo1Rate, lfo1Depth, lfo2Rate, lfo2Depth;
    int lfo1Target, lfo2Target;
    float modAttack, modDecay, modSustain, modRelease, modAmount;
    int modEnvTarget;
};

inline void initPresets(Preset* presets) {
    const char* presetNames[] = {"Init", "Bass", "Lead", "Pad", "Keys", "Bell", "Brass", "Strings"};

    // Default values for all presets
    for (int i = 0; i < NUM_PRESETS; i++) {
        snprintf(presets[i].name, 16, "%s", presetNames[i]);
        presets[i].ratio1 = 1.0f; presets[i].ratio2 = 1.0f;
        presets[i].ratio3 = 1.0f; presets[i].ratio4 = 1.0f;
        presets[i].index1 = 0.0f; presets[i].index2 = 0.0f;
        presets[i].index3 = 0.0f; presets[i].index4 = 0.0f;
        presets[i].attack = 0.01f; presets[i].decay = 0.1f;
        presets[i].sustain = 0.7f; presets[i].release = 0.3f;
        presets[i].algorithm = 0;
        presets[i].filterCutoff = 2000.0f; presets[i].filterQ = 0.707f;
        presets[i].filterType = 0;
        presets[i].chorus = 0.0f; presets[i].reverb = 0.0f;
        presets[i].lfo1Rate = 2.0f; presets[i].lfo1Depth = 0.0f;
        presets[i].lfo2Rate = 4.0f; presets[i].lfo2Depth = 0.0f;
        presets[i].lfo1Target = LFO_OFF; presets[i].lfo2Target = LFO_OFF;
        presets[i].modAttack = 0.01f; presets[i].modDecay = 0.3f;
        presets[i].modSustain = 0.0f; presets[i].modRelease = 0.2f;
        presets[i].modAmount = 0.0f;
        presets[i].modEnvTarget = MODENV_OFF;
    }

    // 0: Init - pure sine wave
    presets[0].ratio1 = 1.0f; presets[0].ratio2 = 1.0f;
    presets[0].ratio3 = 1.0f; presets[0].ratio4 = 1.0f;
    presets[0].index1 = 0.0f; presets[0].index2 = 0.0f;
    presets[0].index3 = 0.0f; presets[0].index4 = 0.0f;
    presets[0].attack = 0.01f; presets[0].decay = 0.1f;
    presets[0].sustain = 1.0f; presets[0].release = 0.2f;
    presets[0].algorithm = 0;

    // 1: Bass - Solid Bass (DX7 style)
    presets[1].ratio1 = 1.0f; presets[1].ratio2 = 1.0f;
    presets[1].ratio3 = 1.0f; presets[1].ratio4 = 1.0f;
    presets[1].index1 = 0.8f; presets[1].index2 = 2.5f;
    presets[1].index3 = 1.5f; presets[1].index4 = 0.5f;
    presets[1].attack = 0.001f; presets[1].decay = 0.15f;
    presets[1].sustain = 0.6f; presets[1].release = 0.1f;
    presets[1].algorithm = 0;
    presets[1].modAttack = 0.001f; presets[1].modDecay = 0.2f;
    presets[1].modSustain = 0.0f; presets[1].modRelease = 0.1f;
    presets[1].modAmount = 0.6f;
    presets[1].modEnvTarget = MODENV_INDEX2;

    // 2: Lead - SynLead
    presets[2].ratio1 = 1.0f; presets[2].ratio2 = 2.0f;
    presets[2].ratio3 = 3.0f; presets[2].ratio4 = 4.0f;
    presets[2].index1 = 0.5f; presets[2].index2 = 3.0f;
    presets[2].index3 = 2.0f; presets[2].index4 = 1.0f;
    presets[2].attack = 0.01f; presets[2].decay = 0.2f;
    presets[2].sustain = 0.8f; presets[2].release = 0.3f;
    presets[2].algorithm = 0;
    presets[2].lfo1Rate = 5.0f; presets[2].lfo1Depth = 0.15f;
    presets[2].lfo1Target = LFO_INDEX2;

    // 3: Pad - Warm Pad
    presets[3].ratio1 = 1.0f; presets[3].ratio2 = 2.0f;
    presets[3].ratio3 = 1.0f; presets[3].ratio4 = 0.5f;
    presets[3].index1 = 0.3f; presets[3].index2 = 1.5f;
    presets[3].index3 = 0.8f; presets[3].index4 = 0.4f;
    presets[3].attack = 0.8f; presets[3].decay = 0.5f;
    presets[3].sustain = 0.7f; presets[3].release = 1.2f;
    presets[3].algorithm = 4;
    presets[3].chorus = 0.4f; presets[3].reverb = 0.3f;
    presets[3].lfo1Rate = 0.8f; presets[3].lfo1Depth = 0.1f;
    presets[3].lfo1Target = LFO_INDEX2;

    // 4: Keys - E.Piano (DX7 classic)
    presets[4].ratio1 = 1.0f; presets[4].ratio2 = 14.0f;
    presets[4].ratio3 = 1.0f; presets[4].ratio4 = 1.0f;
    presets[4].index1 = 0.0f; presets[4].index2 = 1.8f;
    presets[4].index3 = 0.0f; presets[4].index4 = 0.0f;
    presets[4].attack = 0.001f; presets[4].decay = 0.8f;
    presets[4].sustain = 0.2f; presets[4].release = 0.4f;
    presets[4].algorithm = 0;
    presets[4].modAttack = 0.001f; presets[4].modDecay = 0.5f;
    presets[4].modSustain = 0.0f; presets[4].modRelease = 0.2f;
    presets[4].modAmount = 0.8f;
    presets[4].modEnvTarget = MODENV_INDEX2;
    presets[4].reverb = 0.15f;

    // 5: Bell - Tubular Bells
    presets[5].ratio1 = 1.0f; presets[5].ratio2 = 3.5f;
    presets[5].ratio3 = 1.0f; presets[5].ratio4 = 7.0f;
    presets[5].index1 = 0.0f; presets[5].index2 = 2.5f;
    presets[5].index3 = 0.0f; presets[5].index4 = 1.5f;
    presets[5].attack = 0.001f; presets[5].decay = 2.0f;
    presets[5].sustain = 0.0f; presets[5].release = 1.5f;
    presets[5].algorithm = 4;
    presets[5].reverb = 0.4f;

    // 6: Brass - Brass Section
    presets[6].ratio1 = 1.0f; presets[6].ratio2 = 1.0f;
    presets[6].ratio3 = 2.0f; presets[6].ratio4 = 3.0f;
    presets[6].index1 = 0.5f; presets[6].index2 = 2.0f;
    presets[6].index3 = 1.8f; presets[6].index4 = 1.2f;
    presets[6].attack = 0.08f; presets[6].decay = 0.1f;
    presets[6].sustain = 0.9f; presets[6].release = 0.15f;
    presets[6].algorithm = 0;
    presets[6].modAttack = 0.1f; presets[6].modDecay = 0.2f;
    presets[6].modSustain = 0.7f; presets[6].modRelease = 0.1f;
    presets[6].modAmount = 0.5f;
    presets[6].modEnvTarget = MODENV_INDEX2;

    // 7: Strings - String Ensemble
    presets[7].ratio1 = 1.0f; presets[7].ratio2 = 2.0f;
    presets[7].ratio3 = 1.0f; presets[7].ratio4 = 3.0f;
    presets[7].index1 = 0.2f; presets[7].index2 = 1.0f;
    presets[7].index3 = 0.3f; presets[7].index4 = 0.8f;
    presets[7].attack = 0.4f; presets[7].decay = 0.3f;
    presets[7].sustain = 0.8f; presets[7].release = 0.5f;
    presets[7].algorithm = 4;
    presets[7].chorus = 0.5f; presets[7].reverb = 0.25f;
    presets[7].lfo1Rate = 5.5f; presets[7].lfo1Depth = 0.08f;
    presets[7].lfo1Target = LFO_RATIO1;
}
