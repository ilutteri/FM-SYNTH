#pragma once
#include <vector>
#include <cmath>

// Chorus estilo Juno-106
class JunoChorus {
private:
    static const int MAX_DELAY = 2048;
    std::vector<double> delayLineL;
    std::vector<double> delayLineR;
    int writeIndex;
    double lfoPhase1;
    double lfoPhase2;
    double sampleRate;

    const double lfoRate1 = 0.513;
    const double lfoRate2 = 0.863;
    const double baseDelay = 0.005;
    const double depth = 0.003;

public:
    JunoChorus(double sr) : sampleRate(sr), writeIndex(0), lfoPhase1(0), lfoPhase2(0) {
        delayLineL.resize(MAX_DELAY, 0.0);
        delayLineR.resize(MAX_DELAY, 0.0);
    }

    void process(double input, double& outL, double& outR, double mix) {
        delayLineL[writeIndex] = input;
        delayLineR[writeIndex] = input;

        double lfo1 = std::sin(lfoPhase1 * 2.0 * 3.14159265);
        double lfo2 = std::sin(lfoPhase2 * 2.0 * 3.14159265 + 1.5708);

        double delayTimeL = baseDelay + depth * lfo1;
        double delayTimeR = baseDelay + depth * lfo2;

        double delaySamplesL = delayTimeL * sampleRate;
        double delaySamplesR = delayTimeR * sampleRate;

        double readPosL = writeIndex - delaySamplesL;
        double readPosR = writeIndex - delaySamplesR;
        if (readPosL < 0) readPosL += MAX_DELAY;
        if (readPosR < 0) readPosR += MAX_DELAY;

        int indexL = (int)readPosL;
        int indexL2 = (indexL + 1) % MAX_DELAY;
        double fracL = readPosL - indexL;

        int indexR = (int)readPosR;
        int indexR2 = (indexR + 1) % MAX_DELAY;
        double fracR = readPosR - indexR;

        double wetL = delayLineL[indexL] * (1.0 - fracL) + delayLineL[indexL2] * fracL;
        double wetR = delayLineR[indexR] * (1.0 - fracR) + delayLineR[indexR2] * fracR;

        outL = input * (1.0 - mix) + wetL * mix;
        outR = input * (1.0 - mix) + wetR * mix;

        lfoPhase1 += lfoRate1 / sampleRate;
        lfoPhase2 += lfoRate2 / sampleRate;
        if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0;
        if (lfoPhase2 >= 1.0) lfoPhase2 -= 1.0;

        writeIndex = (writeIndex + 1) % MAX_DELAY;
    }
};

// Reverb atmosferica (Schroeder)
class AtmosphericReverb {
private:
    static const int NUM_COMBS = 4;
    std::vector<std::vector<double>> combBuffers;
    std::vector<int> combDelays;
    std::vector<int> combIndices;
    std::vector<double> combFilters;

    static const int NUM_ALLPASS = 2;
    std::vector<std::vector<double>> allpassBuffers;
    std::vector<int> allpassDelays;
    std::vector<int> allpassIndices;

    double decay;
    double damping;
    double sampleRate;

public:
    AtmosphericReverb(double sr) : sampleRate(sr), decay(0.85), damping(0.3) {
        combDelays = {1687, 1931, 2053, 2251};
        allpassDelays = {547, 331};

        double srRatio = sr / 44100.0;
        for (auto& d : combDelays) d = (int)(d * srRatio);
        for (auto& d : allpassDelays) d = (int)(d * srRatio);

        combBuffers.resize(NUM_COMBS);
        combIndices.resize(NUM_COMBS, 0);
        combFilters.resize(NUM_COMBS, 0.0);
        for (int i = 0; i < NUM_COMBS; i++) {
            combBuffers[i].resize(combDelays[i], 0.0);
        }

        allpassBuffers.resize(NUM_ALLPASS);
        allpassIndices.resize(NUM_ALLPASS, 0);
        for (int i = 0; i < NUM_ALLPASS; i++) {
            allpassBuffers[i].resize(allpassDelays[i], 0.0);
        }
    }

    double process(double input, double mix) {
        double wet = 0.0;

        for (int i = 0; i < NUM_COMBS; i++) {
            int idx = combIndices[i];
            double delayed = combBuffers[i][idx];
            combFilters[i] = delayed * (1.0 - damping) + combFilters[i] * damping;
            double feedback = combFilters[i] * decay;
            combBuffers[i][idx] = input + feedback;
            wet += delayed;
            combIndices[i] = (idx + 1) % combDelays[i];
        }

        wet *= 0.25;

        for (int i = 0; i < NUM_ALLPASS; i++) {
            int idx = allpassIndices[i];
            double delayed = allpassBuffers[i][idx];
            double g = 0.5;
            double output = -g * wet + delayed;
            allpassBuffers[i][idx] = wet + g * output;
            wet = output;
            allpassIndices[i] = (idx + 1) % allpassDelays[i];
        }

        return input * (1.0 - mix) + wet * mix;
    }
};
