#include <iostream>
#include <cmath>
#include <vector>
#include <memory>
#include <atomic>
#include <rtaudio/RtAudio.h>

// =====================
// Constantes
// =====================
const double PI = 3.14159265358979323846;
const double SAMPLE_RATE = 44100.0;

// =====================
// Oscilador sinusoidal
// =====================
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

    inline double process(double phaseMod = 0.0) {
        double out = std::sin(phase + phaseMod);
        phase += phaseIncrement;
        if (phase >= 2.0 * PI)
            phase -= 2.0 * PI;
        return out;
    }

    void reset() { phase = 0.0; }

private:
    void updatePhaseIncrement() {
        phaseIncrement = 2.0 * PI * frequency / sampleRate;
    }
};

// =====================
// Envolvente simple (ADSR light)
// =====================
class Envelope {
private:
    double value = 0.0;
    double attack = 0.001;
    double release = 0.9995;
    bool gate = false;

public:
    void noteOn() { gate = true; }
    void noteOff() { gate = false; }

    inline double process() {
        if (gate) {
            value += (1.0 - value) * attack;
        } else {
            value *= release;
        }
        return value;
    }
};

// =====================
// FM Synth mejorado
// =====================
class FMSynth {
private:
    Oscillator carrier;
    Oscillator modulator;

    std::atomic<double> modulationIndex;
    std::atomic<double> modulatorRatio;
    std::atomic<bool> isActive;
    std::atomic<double> currentFrequency;

    // Amplitud base
    double amplitude = 0.4;

    // Envolvente simple
    double env = 0.0;
    double attackCoeff;
    double releaseCoeff;

    // Filtro post-voz
    double lpState = 0.0;
    double lpCoeff;

    // Saturación
    double drive = 1.5;

    static constexpr double referenceFreq = 440.0;

public:
    FMSynth(double freq, double modRatio, double modIndex, double sr)
        : carrier(freq, sr),
          modulator(freq * modRatio, sr),
          modulationIndex(modIndex),
          modulatorRatio(modRatio),
          isActive(false),
          currentFrequency(freq) {

        // Envolvente (valores musicales)
        attackCoeff  = std::exp(-1.0 / (0.005 * sr));  // 5 ms
        releaseCoeff = std::exp(-1.0 / (0.200 * sr));  // 200 ms

        // Filtro LP muy suave
        double cutoff = 12000.0;
        lpCoeff = 1.0 - std::exp(-2.0 * M_PI * cutoff / sr);
    }

    double process() {
        if (!isActive.load()) {
            // Release natural
            env *= releaseCoeff;
            if (env < 1e-5) return 0.0;
        } else {
            // Attack
            env = 1.0 - (1.0 - env) * attackCoeff;
        }

        // ==========================
        // Oversampling 2×
        // ==========================
        double out = 0.0;

        for (int i = 0; i < 2; ++i) {
            // Escalado de índice por pitch
            double freq = currentFrequency.load();
            double pitchScale = referenceFreq / freq;
            double effectiveIndex = modulationIndex.load() * pitchScale;

            // FM
            double mod = modulator.process();
            double phaseMod = effectiveIndex * mod;
            double sig = carrier.process(phaseMod);

            out += sig;
        }

        out *= 0.5;

        // Saturación suave
        out = std::tanh(out * drive);

        // Aplicar envolvente
        out *= env * amplitude;

        // Filtro post-voz
        lpState += lpCoeff * (out - lpState);

        return lpState;
    }

    void noteOn(double freq, double modRatio) {
        currentFrequency.store(freq);
        modulatorRatio.store(modRatio);

        carrier.setFrequency(freq);
        modulator.setFrequency(freq * modRatio);

        carrier.reset();
        modulator.reset();

        env = 0.0;
        isActive.store(true);
    }

    void noteOff() {
        isActive.store(false);
    }

    void setModulationIndex(double index) {
        modulationIndex.store(index);
    }

    void setModulatorRatio(double ratio) {
        modulatorRatio.store(ratio);
        modulator.setFrequency(currentFrequency.load() * ratio);
    }

    double getCurrentFrequency() const {
        return currentFrequency.load();
    }
};
// =====================
// Global
// =====================
std::unique_ptr<FMSynth> synth;

int audioCallback(void *outputBuffer, void *, unsigned int nFrames,
                  double, RtAudioStreamStatus status, void *) {

    double *buffer = (double *)outputBuffer;

    if (status)
        std::cout << "Underflow!" << std::endl;

    for (unsigned int i = 0; i < nFrames; i++) {
        double s = synth->process();
        *buffer++ = s;
        *buffer++ = s;
    }
    return 0;
}

// =====================
// MIDI → Hz
// =====================
double midiToFreq(int note) {
    return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

// =====================
// Main
// =====================
int main() {
    synth = std::make_unique<FMSynth>(440.0, 2.0, 3.0, SAMPLE_RATE);

    RtAudio dac;
    RtAudio::StreamParameters params;
    params.deviceId = dac.getDefaultOutputDevice();
    params.nChannels = 2;

    unsigned int bufferFrames = 256;

    dac.openStream(&params, nullptr, RTAUDIO_FLOAT64,
                   (unsigned int)SAMPLE_RATE, &bufferFrames,
                   &audioCallback, nullptr);

    dac.startStream();

    std::cout << "n <nota> <ratio> <index> | o | q" << std::endl;

    char c;
    while (std::cin >> c) {
        if (c == 'q') break;
        if (c == 'n') {
            int note; double ratio, idx;
            std::cin >> note >> ratio >> idx;
            synth->setModulationIndex(idx);
            synth->noteOn(midiToFreq(note), ratio);
        }
        if (c == 'o') synth->noteOff();
    }

    dac.stopStream();
    dac.closeStream();
}
