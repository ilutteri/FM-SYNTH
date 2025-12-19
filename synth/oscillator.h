#pragma once
#include <cmath>
#include "constants.h"

class Oscillator {
private:
    double phase;
    double phaseIncrement;
    double frequency;
    double sampleRate;

public:
    Oscillator(double freq, double sr)
        : phase(0.0), frequency(freq), sampleRate(sr) {
        updatePhaseIncrement();
    }

    void setFrequency(double freq) {
        frequency = freq;
        updatePhaseIncrement();
    }

    double getFrequency() const { return frequency; }

    double process(double modulation = 0.0) {
        double output = std::sin(phase + modulation);
        phase += phaseIncrement;
        if (phase >= TWO_PI) {
            phase -= TWO_PI;
        }
        return output;
    }

    void reset() {
        phase = 0.0;
    }

private:
    void updatePhaseIncrement() {
        phaseIncrement = TWO_PI * frequency / sampleRate;
    }
};
