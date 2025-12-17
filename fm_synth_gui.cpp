#include <iostream>
#include <cmath>
#include <vector>
#include <memory>
#include <atomic>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <rtaudio/RtAudio.h>
#include "raylib.h"

// Constantes
const double TWO_PI = 6.28318530717958647692;
const double SAMPLE_RATE = 44100.0;
const int WAVEFORM_SIZE = 512;

// ============================================================================
// Clase Oscillator
// ============================================================================

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

// ============================================================================
// ADSR Envelope
// ============================================================================

enum EnvelopeState {
    ENV_IDLE,
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE
};

class ADSREnvelope {
private:
    double attackTime;
    double decayTime;
    double sustainLevel;
    double releaseTime;

    EnvelopeState state;
    double currentLevel;
    double sampleRate;

    double attackIncrement;
    double decayIncrement;
    double releaseIncrement;
    double releaseStartLevel;

public:
    ADSREnvelope(double sr)
        : attackTime(0.01),
          decayTime(0.1),
          sustainLevel(0.7),
          releaseTime(0.3),
          state(ENV_IDLE),
          currentLevel(0.0),
          sampleRate(sr),
          releaseStartLevel(0.0) {
        updateIncrements();
    }

    void noteOn() {
        state = ENV_ATTACK;
    }

    void noteOff() {
        if (state != ENV_IDLE) {
            releaseStartLevel = currentLevel;
            releaseIncrement = releaseTime > 0.0 ? releaseStartLevel / (releaseTime * sampleRate) : releaseStartLevel;
            state = ENV_RELEASE;
        }
    }

    double process() {
        switch (state) {
            case ENV_IDLE:
                currentLevel = 0.0;
                break;

            case ENV_ATTACK:
                currentLevel += attackIncrement;
                if (currentLevel >= 1.0) {
                    currentLevel = 1.0;
                    state = ENV_DECAY;
                }
                break;

            case ENV_DECAY:
                currentLevel -= decayIncrement;
                if (currentLevel <= sustainLevel) {
                    currentLevel = sustainLevel;
                    state = ENV_SUSTAIN;
                }
                break;

            case ENV_SUSTAIN:
                currentLevel = sustainLevel;
                break;

            case ENV_RELEASE:
                currentLevel -= releaseIncrement;
                if (currentLevel <= 0.0) {
                    currentLevel = 0.0;
                    state = ENV_IDLE;
                }
                break;
        }

        return currentLevel;
    }

    bool isActive() const {
        return state != ENV_IDLE;
    }

    EnvelopeState getState() const { return state; }
    double getLevel() const { return currentLevel; }

    void setAttack(double seconds) {
        attackTime = std::max(0.001, seconds);
        updateIncrements();
    }

    void setDecay(double seconds) {
        decayTime = std::max(0.001, seconds);
        updateIncrements();
    }

    void setSustain(double level) {
        sustainLevel = std::max(0.0, std::min(1.0, level));
        updateIncrements();
    }

    void setRelease(double seconds) {
        releaseTime = std::max(0.001, seconds);
        updateIncrements();
    }

    double getAttack() const { return attackTime; }
    double getDecay() const { return decayTime; }
    double getSustain() const { return sustainLevel; }
    double getRelease() const { return releaseTime; }

private:
    void updateIncrements() {
        attackIncrement = 1.0 / (attackTime * sampleRate);
        decayIncrement = (1.0 - sustainLevel) / (decayTime * sampleRate);
    }
};

// ============================================================================
// Algoritmos FM de 4 operadores
// ============================================================================

enum FMAlgorithm {
    ALG_STACK = 0,      // 4 -> 3 -> 2 -> 1 (serie completa)
    ALG_TWIN,           // (4->3) + (2) -> 1
    ALG_BRANCH,         // 4 -> 3 -> 1, 4 -> 2 -> 1
    ALG_PARALLEL,       // (2 + 3 + 4) -> 1
    ALG_DUAL_CARRIER,   // 4 -> 3, 2 -> 1 (dos carriers)
    ALG_TRIPLE,         // 4 -> (1, 2, 3) todos carriers
    ALG_COUNT
};

const char* algorithmNames[] = {
    "Stack",
    "Twin",
    "Branch",
    "Parallel",
    "Dual",
    "Triple"
};

// ============================================================================
// FMSynth con 4 operadores + ADSR
// ============================================================================

class FMSynth {
private:
    Oscillator op1, op2, op3, op4;
    ADSREnvelope envelope;

    std::atomic<double> ratio1, ratio2, ratio3, ratio4;
    std::atomic<double> index1, index2, index3, index4;

    double prevSample1;

    std::atomic<int> algorithm;
    double amplitude;
    std::atomic<bool> noteActive;
    std::atomic<double> currentFrequency;
    double sampleRate;

public:
    FMSynth(double freq, double sr)
        : op1(freq, sr),
          op2(freq * 2.0, sr),
          op3(freq * 3.0, sr),
          op4(freq * 4.0, sr),
          envelope(sr),
          ratio1(1.0),
          ratio2(2.0),
          ratio3(3.0),
          ratio4(4.0),
          index1(0.0),
          index2(2.0),
          index3(1.5),
          index4(1.0),
          prevSample1(0.0),
          algorithm(ALG_STACK),
          amplitude(0.3),
          noteActive(false),
          currentFrequency(freq),
          sampleRate(sr) {
    }

    double process() {
        if (!envelope.isActive()) return 0.0;

        double out1, out2, out3, out4;
        double idx1 = index1.load();
        double idx2 = index2.load();
        double idx3 = index3.load();
        double idx4 = index4.load();

        double feedback = idx1 * prevSample1;
        double envLevel = envelope.process();

        switch (algorithm.load()) {
            case ALG_STACK:
                // 4 -> 3 -> 2 -> 1
                out4 = op4.process();
                out3 = op3.process(idx4 * out4);
                out2 = op2.process(idx3 * out3);
                out1 = op1.process(idx2 * out2 + feedback);
                break;

            case ALG_TWIN:
                // (4->3) + 2 -> 1
                out4 = op4.process();
                out3 = op3.process(idx4 * out4);
                out2 = op2.process();
                out1 = op1.process(idx3 * out3 + idx2 * out2 + feedback);
                break;

            case ALG_BRANCH:
                // 4 -> 3 -> 1, 4 -> 2 -> 1
                out4 = op4.process();
                out3 = op3.process(idx4 * out4);
                out2 = op2.process(idx4 * out4);
                out1 = op1.process(idx3 * out3 + idx2 * out2 + feedback);
                break;

            case ALG_PARALLEL:
                // (2 + 3 + 4) -> 1
                out2 = op2.process();
                out3 = op3.process();
                out4 = op4.process();
                out1 = op1.process(idx2 * out2 + idx3 * out3 + idx4 * out4 + feedback);
                break;

            case ALG_DUAL_CARRIER:
                // 4 -> 3 (carrier), 2 -> 1 (carrier)
                out4 = op4.process();
                out3 = op3.process(idx4 * out4);
                out2 = op2.process();
                out1 = op1.process(idx2 * out2 + feedback);
                prevSample1 = out1;
                return (out1 + out3 * 0.7) * amplitude * envLevel * 0.7;

            case ALG_TRIPLE:
                // 4 -> (1, 2, 3) todos carriers
                out4 = op4.process();
                out1 = op1.process(idx4 * out4 + feedback);
                out2 = op2.process(idx4 * out4);
                out3 = op3.process(idx4 * out4);
                prevSample1 = out1;
                return (out1 + out2 * 0.6 + out3 * 0.4) * amplitude * envLevel * 0.5;

            default:
                return 0.0;
        }

        prevSample1 = out1;
        return out1 * amplitude * envLevel;
    }

    void noteOn(double freq) {
        currentFrequency.store(freq);
        op1.setFrequency(freq * ratio1.load());
        op2.setFrequency(freq * ratio2.load());
        op3.setFrequency(freq * ratio3.load());
        op4.setFrequency(freq * ratio4.load());
        op1.reset();
        op2.reset();
        op3.reset();
        op4.reset();
        prevSample1 = 0.0;
        noteActive.store(true);
        envelope.noteOn();
    }

    void noteOff() {
        noteActive.store(false);
        envelope.noteOff();
    }

    bool isActive() const { return envelope.isActive(); }

    // Setters
    void setRatio1(double r) { ratio1.store(r); if (noteActive.load()) op1.setFrequency(currentFrequency.load() * r); }
    void setRatio2(double r) { ratio2.store(r); if (noteActive.load()) op2.setFrequency(currentFrequency.load() * r); }
    void setRatio3(double r) { ratio3.store(r); if (noteActive.load()) op3.setFrequency(currentFrequency.load() * r); }
    void setRatio4(double r) { ratio4.store(r); if (noteActive.load()) op4.setFrequency(currentFrequency.load() * r); }

    void setIndex1(double i) { index1.store(i); }
    void setIndex2(double i) { index2.store(i); }
    void setIndex3(double i) { index3.store(i); }
    void setIndex4(double i) { index4.store(i); }
    void setAlgorithm(int alg) { algorithm.store(alg); }

    void setAttack(double t) { envelope.setAttack(t); }
    void setDecay(double t) { envelope.setDecay(t); }
    void setSustain(double l) { envelope.setSustain(l); }
    void setRelease(double t) { envelope.setRelease(t); }

    // Getters
    double getRatio1() const { return ratio1.load(); }
    double getRatio2() const { return ratio2.load(); }
    double getRatio3() const { return ratio3.load(); }
    double getRatio4() const { return ratio4.load(); }
    double getIndex1() const { return index1.load(); }
    double getIndex2() const { return index2.load(); }
    double getIndex3() const { return index3.load(); }
    double getIndex4() const { return index4.load(); }
    int getAlgorithm() const { return algorithm.load(); }
    double getCurrentFrequency() const { return currentFrequency.load(); }

    double getEnvelopeLevel() const { return envelope.getLevel(); }
    EnvelopeState getEnvelopeState() const { return envelope.getState(); }
};

// ============================================================================
// Buffer circular para waveform
// ============================================================================

class WaveformBuffer {
private:
    std::vector<float> buffer;
    std::atomic<int> writeIndex;
    int size;

public:
    WaveformBuffer(int bufferSize) : size(bufferSize), writeIndex(0) {
        buffer.resize(size, 0.0f);
    }

    void write(float sample) {
        buffer[writeIndex.load()] = sample;
        writeIndex.store((writeIndex.load() + 1) % size);
    }

    float read(int index) const {
        int readIdx = (writeIndex.load() + index) % size;
        return buffer[readIdx];
    }

    int getSize() const { return size; }
};

// ============================================================================
// Chorus - Estilo Juno 106
// ============================================================================

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

// ============================================================================
// Reverb - Atmosférica
// ============================================================================

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

// ============================================================================
// Filtro - LP / HP
// ============================================================================

enum FilterType {
    FILTER_OFF = 0,
    FILTER_LOWPASS,
    FILTER_HIGHPASS
};

class Filter {
private:
    double y1, y2, x1, x2;
    double a0, a1, a2, b1, b2;
    double sampleRate;

public:
    Filter(double sr) : sampleRate(sr), y1(0), y2(0), x1(0), x2(0),
                        a0(1), a1(0), a2(0), b1(0), b2(0) {}

    void setLowPass(double cutoff, double q) {
        double w0 = 2.0 * 3.14159265 * cutoff / sampleRate;
        double cosw0 = std::cos(w0);
        double sinw0 = std::sin(w0);
        double alpha = sinw0 / (2.0 * q);

        double b0 = (1.0 - cosw0) / 2.0;
        double b1_t = 1.0 - cosw0;
        double b2_t = (1.0 - cosw0) / 2.0;
        double a0_t = 1.0 + alpha;
        double a1_t = -2.0 * cosw0;
        double a2_t = 1.0 - alpha;

        a0 = b0 / a0_t; a1 = b1_t / a0_t; a2 = b2_t / a0_t;
        b1 = a1_t / a0_t; b2 = a2_t / a0_t;
    }

    void setHighPass(double cutoff, double q) {
        double w0 = 2.0 * 3.14159265 * cutoff / sampleRate;
        double cosw0 = std::cos(w0);
        double sinw0 = std::sin(w0);
        double alpha = sinw0 / (2.0 * q);

        double b0 = (1.0 + cosw0) / 2.0;
        double b1_t = -(1.0 + cosw0);
        double b2_t = (1.0 + cosw0) / 2.0;
        double a0_t = 1.0 + alpha;
        double a1_t = -2.0 * cosw0;
        double a2_t = 1.0 - alpha;

        a0 = b0 / a0_t; a1 = b1_t / a0_t; a2 = b2_t / a0_t;
        b1 = a1_t / a0_t; b2 = a2_t / a0_t;
    }

    double process(double input) {
        double output = a0 * input + a1 * x1 + a2 * x2 - b1 * y1 - b2 * y2;
        x2 = x1; x1 = input;
        y2 = y1; y1 = output;
        return output;
    }

    void reset() { y1 = y2 = x1 = x2 = 0.0; }
};

// ============================================================================
// Polifonía - 6 voces
// ============================================================================

const int NUM_VOICES = 6;

struct Voice {
    std::unique_ptr<FMSynth> synth;
    int note;

    Voice() : note(-1) {}
};

// ============================================================================
// Variables globales
// ============================================================================

Voice voices[NUM_VOICES];
std::unique_ptr<WaveformBuffer> waveformBuffer;
std::unique_ptr<JunoChorus> chorus;
std::unique_ptr<AtmosphericReverb> reverbL;
std::unique_ptr<AtmosphericReverb> reverbR;
std::unique_ptr<Filter> filterL;
std::unique_ptr<Filter> filterR;

std::atomic<double> chorusMix(0.0);
std::atomic<double> reverbMix(0.0);
std::atomic<int> filterType(FILTER_OFF);
std::atomic<double> filterCutoff(2000.0);
std::atomic<double> filterQ(0.707);

// GUI params
float guiRatio1 = 1.0f, guiRatio2 = 2.0f, guiRatio3 = 3.0f, guiRatio4 = 4.0f;
float guiIndex1 = 0.0f, guiIndex2 = 2.0f, guiIndex3 = 1.5f, guiIndex4 = 1.0f;
float guiAttack = 0.01f, guiDecay = 0.1f, guiSustain = 0.7f, guiRelease = 0.3f;
float guiChorus = 0.0f, guiReverb = 0.0f;
int guiFilterType = 0;
float guiFilterCutoff = 2000.0f, guiFilterQ = 0.707f;
int guiAlgorithm = 0;
int currentOctave = 4;
std::vector<int> activeNotes;

// ============================================================================
// Voice management
// ============================================================================

int findFreeVoice() {
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices[i].note == -1 && !voices[i].synth->isActive()) return i;
    }
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices[i].note == -1) return i;
    }
    return 0;
}

int findVoiceWithNote(int note) {
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices[i].note == note) return i;
    }
    return -1;
}

void voiceNoteOn(int note, double freq) {
    int existing = findVoiceWithNote(note);
    if (existing >= 0) return;

    int v = findFreeVoice();
    voices[v].synth->noteOn(freq);
    voices[v].note = note;
}

void voiceNoteOff(int note) {
    int v = findVoiceWithNote(note);
    if (v >= 0) {
        voices[v].synth->noteOff();
        voices[v].note = -1;
    }
}

// ============================================================================
// Audio callback
// ============================================================================

int audioCallback(void *outputBuffer, void *inputBuffer, unsigned int nFrames,
                  double streamTime, RtAudioStreamStatus status, void *userData) {

    double *buffer = (double *)outputBuffer;
    double chMix = chorusMix.load();
    double rvMix = reverbMix.load();
    int fType = filterType.load();

    for (unsigned int i = 0; i < nFrames; i++) {
        double sample = 0.0;

        for (int v = 0; v < NUM_VOICES; v++) {
            sample += voices[v].synth->process();
        }

        sample *= 0.4;

        if (fType != FILTER_OFF) {
            sample = filterL->process(sample);
        }

        double outL, outR;
        chorus->process(sample, outL, outR, chMix);

        outL = reverbL->process(outL, rvMix);
        outR = reverbR->process(outR, rvMix);

        outL = std::tanh(outL);
        outR = std::tanh(outR);

        waveformBuffer->write((float)((outL + outR) * 0.5));
        *buffer++ = outL;
        *buffer++ = outR;
    }

    return 0;
}

// ============================================================================
// Utility
// ============================================================================

double midiToFreq(int midiNote) {
    return 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
}

// ============================================================================
// GUI Drawing functions
// ============================================================================

void DrawVerticalSlider(int x, int y, int height, const char* label, float* value, float minVal, float maxVal, Color trackColor) {
    int labelWidth = MeasureText(label, 10);
    DrawText(label, x + 15 - labelWidth / 2, y, 10, WHITE);

    int trackX = x + 10;
    int trackY = y + 14;
    int trackWidth = 10;
    DrawRectangle(trackX, trackY, trackWidth, height, Color{40, 40, 50, 255});
    DrawRectangleLines(trackX, trackY, trackWidth, height, DARKGRAY);

    float normalized = (*value - minVal) / (maxVal - minVal);
    int fillHeight = (int)(normalized * height);
    DrawRectangle(trackX + 1, trackY + height - fillHeight, trackWidth - 2, fillHeight, trackColor);

    int handleY = trackY + height - (int)(normalized * height) - 3;
    DrawRectangle(trackX - 2, handleY, trackWidth + 4, 6, RAYWHITE);

    char valueText[16];
    snprintf(valueText, sizeof(valueText), "%.2f", *value);
    int valWidth = MeasureText(valueText, 9);
    DrawText(valueText, x + 15 - valWidth / 2, trackY + height + 4, 9, GRAY);

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse = GetMousePosition();
        if (mouse.x >= trackX - 5 && mouse.x <= trackX + trackWidth + 5 &&
            mouse.y >= trackY && mouse.y <= trackY + height) {
            float newNormalized = 1.0f - (mouse.y - trackY) / (float)height;
            newNormalized = fmaxf(0.0f, fminf(1.0f, newNormalized));
            *value = minVal + newNormalized * (maxVal - minVal);
        }
    }
}

void DrawOperatorPanel(int x, int y, const char* name, float* ratio, float* index,
                       Color color, bool isCarrier, const char* indexLabel) {
    int panelWidth = 70;
    int panelHeight = 115;

    DrawRectangle(x, y, panelWidth, panelHeight, Color{35, 35, 45, 255});
    DrawRectangleLines(x, y, panelWidth, panelHeight, color);

    int nameWidth = MeasureText(name, 12);
    DrawText(name, x + (panelWidth - nameWidth) / 2, y + 4, 12, color);

    const char* typeLabel = isCarrier ? "[C]" : "[M]";
    int typeWidth = MeasureText(typeLabel, 9);
    DrawText(typeLabel, x + (panelWidth - typeWidth) / 2, y + 18, 9, isCarrier ? Color{180, 100, 60, 255} : Color{60, 120, 180, 255});

    DrawVerticalSlider(x + 3, y + 30, 55, "R", ratio, 0.5f, 8.0f, color);
    DrawVerticalSlider(x + 36, y + 30, 55, indexLabel, index, 0.0f, 10.0f, color);
}

void DrawADSRPanel(int x, int y, float* a, float* d, float* s, float* r) {
    int panelWidth = 160;
    int panelHeight = 115;

    DrawRectangle(x, y, panelWidth, panelHeight, Color{35, 35, 45, 255});
    DrawRectangleLines(x, y, panelWidth, panelHeight, Color{200, 180, 100, 255});
    DrawText("ENVELOPE", x + 50, y + 4, 11, Color{200, 180, 100, 255});

    // ADSR sliders
    DrawVerticalSlider(x + 5, y + 20, 50, "A", a, 0.001f, 2.0f, Color{100, 200, 100, 255});
    DrawVerticalSlider(x + 40, y + 20, 50, "D", d, 0.001f, 2.0f, Color{200, 200, 100, 255});
    DrawVerticalSlider(x + 75, y + 20, 50, "S", s, 0.0f, 1.0f, Color{100, 150, 200, 255});
    DrawVerticalSlider(x + 110, y + 20, 50, "R", r, 0.001f, 3.0f, Color{200, 100, 100, 255});

    // ADSR Graph
    int graphX = x + 5;
    int graphY = y + 85;
    int graphW = 150;
    int graphH = 25;

    DrawRectangle(graphX, graphY, graphW, graphH, Color{25, 25, 35, 255});
    DrawRectangleLines(graphX, graphY, graphW, graphH, Color{60, 60, 80, 255});

    // Calculate ADSR points
    float totalTime = *a + *d + 0.3f + *r; // 0.3s for sustain display
    float scale = graphW / totalTime;

    int attackEnd = graphX + (int)(*a * scale);
    int decayEnd = attackEnd + (int)(*d * scale);
    int sustainEnd = decayEnd + (int)(0.3f * scale);
    int releaseEnd = sustainEnd + (int)(*r * scale);
    if (releaseEnd > graphX + graphW) releaseEnd = graphX + graphW;

    int baseY = graphY + graphH - 2;
    int peakY = graphY + 2;
    int sustainY = graphY + graphH - 2 - (int)((*s) * (graphH - 4));

    // Draw envelope shape
    DrawLine(graphX, baseY, attackEnd, peakY, Color{100, 200, 100, 255});  // Attack
    DrawLine(attackEnd, peakY, decayEnd, sustainY, Color{200, 200, 100, 255});  // Decay
    DrawLine(decayEnd, sustainY, sustainEnd, sustainY, Color{100, 150, 200, 255});  // Sustain
    DrawLine(sustainEnd, sustainY, releaseEnd, baseY, Color{200, 100, 100, 255});  // Release
}

void DrawAlgorithmDiagram(int x, int y, int algorithm) {
    int boxW = 22, boxH = 16;
    Color opColor = Color{60, 120, 180, 255};
    Color carrierColor = Color{180, 100, 60, 255};

    DrawRectangle(x - 5, y - 5, 105, 50, Color{30, 30, 40, 255});

    switch (algorithm) {
        case ALG_STACK:
            // 4 -> 3 -> 2 -> 1
            for (int i = 0; i < 4; i++) {
                int bx = x + i * 26;
                Color c = (i == 3) ? carrierColor : opColor;
                DrawRectangle(bx, y + 12, boxW, boxH, c);
                char num[2] = {(char)('4' - i), 0};
                DrawText(num, bx + 8, y + 14, 12, WHITE);
                if (i < 3) DrawText(">", bx + 23, y + 14, 10, GRAY);
            }
            break;

        case ALG_TWIN:
            DrawRectangle(x, y + 2, boxW, boxH, opColor);
            DrawText("4", x + 8, y + 4, 12, WHITE);
            DrawText(">", x + 23, y + 4, 10, GRAY);
            DrawRectangle(x + 28, y + 2, boxW, boxH, opColor);
            DrawText("3", x + 36, y + 4, 12, WHITE);
            DrawRectangle(x + 28, y + 22, boxW, boxH, opColor);
            DrawText("2", x + 36, y + 24, 12, WHITE);
            DrawText(">", x + 55, y + 12, 10, GRAY);
            DrawRectangle(x + 65, y + 12, boxW, boxH, carrierColor);
            DrawText("1", x + 73, y + 14, 12, WHITE);
            break;

        case ALG_BRANCH:
            DrawRectangle(x, y + 12, boxW, boxH, opColor);
            DrawText("4", x + 8, y + 14, 12, WHITE);
            DrawLine(x + 24, y + 18, x + 35, y + 8, GRAY);
            DrawLine(x + 24, y + 22, x + 35, y + 32, GRAY);
            DrawRectangle(x + 38, y, boxW, boxH, opColor);
            DrawText("3", x + 46, y + 2, 12, WHITE);
            DrawRectangle(x + 38, y + 24, boxW, boxH, opColor);
            DrawText("2", x + 46, y + 26, 12, WHITE);
            DrawText(">", x + 62, y + 12, 10, GRAY);
            DrawRectangle(x + 72, y + 12, boxW, boxH, carrierColor);
            DrawText("1", x + 80, y + 14, 12, WHITE);
            break;

        case ALG_PARALLEL:
            for (int i = 0; i < 3; i++) {
                DrawRectangle(x, y + i * 14, boxW, boxH - 2, opColor);
                char num[2] = {(char)('4' - i), 0};
                DrawText(num, x + 8, y + i * 14, 10, WHITE);
            }
            DrawText(">", x + 28, y + 14, 10, GRAY);
            DrawRectangle(x + 40, y + 12, boxW, boxH, carrierColor);
            DrawText("1", x + 48, y + 14, 12, WHITE);
            break;

        case ALG_DUAL_CARRIER:
            DrawRectangle(x, y + 2, boxW, boxH, opColor);
            DrawText("4", x + 8, y + 4, 12, WHITE);
            DrawText(">", x + 23, y + 4, 10, GRAY);
            DrawRectangle(x + 30, y + 2, boxW, boxH, carrierColor);
            DrawText("3", x + 38, y + 4, 12, WHITE);
            DrawRectangle(x + 60, y + 2, boxW, boxH, opColor);
            DrawText("2", x + 68, y + 4, 12, WHITE);
            DrawText(">", x + 83, y + 4, 10, GRAY);
            DrawRectangle(x + 60, y + 22, boxW, boxH, carrierColor);
            DrawText("1", x + 68, y + 24, 12, WHITE);
            break;

        case ALG_TRIPLE:
            DrawRectangle(x, y + 12, boxW, boxH, opColor);
            DrawText("4", x + 8, y + 14, 12, WHITE);
            DrawLine(x + 24, y + 16, x + 35, y + 6, GRAY);
            DrawLine(x + 24, y + 20, x + 35, y + 20, GRAY);
            DrawLine(x + 24, y + 24, x + 35, y + 34, GRAY);
            DrawRectangle(x + 38, y, boxW, boxH, carrierColor);
            DrawText("1", x + 46, y + 2, 12, WHITE);
            DrawRectangle(x + 38, y + 16, boxW, boxH, carrierColor);
            DrawText("2", x + 46, y + 18, 12, WHITE);
            DrawRectangle(x + 38, y + 32, boxW, boxH, carrierColor);
            DrawText("3", x + 46, y + 34, 12, WHITE);
            break;
    }
}

void DrawWaveform(int x, int y, int width, int height) {
    DrawRectangle(x, y, width, height, Color{20, 20, 30, 255});
    DrawRectangleLines(x, y, width, height, DARKGRAY);

    int centerY = y + height / 2;
    DrawLine(x, centerY, x + width, centerY, Color{60, 60, 80, 255});

    int bufferSize = waveformBuffer->getSize();
    float xStep = (float)width / bufferSize;

    for (int i = 0; i < bufferSize - 1; i++) {
        float sample1 = waveformBuffer->read(i);
        float sample2 = waveformBuffer->read(i + 1);

        int y1 = centerY - (int)(sample1 * height * 0.4f);
        int y2 = centerY - (int)(sample2 * height * 0.4f);

        DrawLine(x + (int)(i * xStep), y1, x + (int)((i + 1) * xStep), y2, GREEN);
    }
}

void DrawPianoKey(int x, int y, int width, int height, bool isBlack, bool isPressed, int noteIndex) {
    Color keyColor;
    if (isPressed) {
        keyColor = isBlack ? Color{80, 80, 200, 255} : Color{100, 100, 255, 255};
    } else {
        keyColor = isBlack ? Color{30, 30, 30, 255} : RAYWHITE;
    }

    DrawRectangle(x, y, width, height, keyColor);
    DrawRectangleLines(x, y, width, height, DARKGRAY);
}

bool isNoteActive(int note) {
    for (int v = 0; v < NUM_VOICES; v++) {
        if (voices[v].note == note) return true;
    }
    return false;
}

void DrawPiano(int x, int y, int* pressedNote) {
    const int whiteKeyWidth = 28;
    const int whiteKeyHeight = 70;
    const int blackKeyWidth = 18;
    const int blackKeyHeight = 42;

    const int numWhiteKeys = 15;
    const int whiteNotes[] = {0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23, 24};

    const int numBlackKeys = 10;
    const int blackNotes[] = {1, 3, 6, 8, 10, 13, 15, 18, 20, 22};
    const int blackPositions[] = {0, 1, 3, 4, 5, 7, 8, 10, 11, 12};

    int baseMidi = 12 * currentOctave;

    Vector2 mouse = GetMousePosition();
    bool mouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);

    *pressedNote = -1;

    for (int i = 0; i < numWhiteKeys; i++) {
        int keyX = x + i * whiteKeyWidth;
        int midiNote = baseMidi + whiteNotes[i];
        bool isPressed = isNoteActive(midiNote);

        if (mouseDown && mouse.x >= keyX && mouse.x < keyX + whiteKeyWidth &&
            mouse.y >= y && mouse.y < y + whiteKeyHeight) {
            bool onBlackKey = false;
            for (int j = 0; j < numBlackKeys; j++) {
                int bkX = x + blackPositions[j] * whiteKeyWidth + whiteKeyWidth - blackKeyWidth / 2;
                if (mouse.x >= bkX && mouse.x < bkX + blackKeyWidth &&
                    mouse.y >= y && mouse.y < y + blackKeyHeight) {
                    onBlackKey = true;
                    break;
                }
            }
            if (!onBlackKey) {
                *pressedNote = midiNote;
                isPressed = true;
            }
        }

        DrawPianoKey(keyX, y, whiteKeyWidth - 2, whiteKeyHeight, false, isPressed, i);

        if (whiteNotes[i] % 12 == 0) {
            char noteLabel[8];
            snprintf(noteLabel, sizeof(noteLabel), "C%d", (baseMidi + whiteNotes[i]) / 12);
            int textWidth = MeasureText(noteLabel, 9);
            DrawText(noteLabel, keyX + (whiteKeyWidth - textWidth) / 2, y + whiteKeyHeight - 12, 9, BLACK);
        }
    }

    for (int i = 0; i < numBlackKeys; i++) {
        int keyX = x + blackPositions[i] * whiteKeyWidth + whiteKeyWidth - blackKeyWidth / 2;
        int midiNote = baseMidi + blackNotes[i];
        bool isPressed = isNoteActive(midiNote);

        if (mouseDown && mouse.x >= keyX && mouse.x < keyX + blackKeyWidth &&
            mouse.y >= y && mouse.y < y + blackKeyHeight) {
            *pressedNote = midiNote;
            isPressed = true;
        }

        DrawPianoKey(keyX, y, blackKeyWidth, blackKeyHeight, true, isPressed, i);
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    srand((unsigned int)time(NULL));

    for (int i = 0; i < NUM_VOICES; i++) {
        voices[i].synth = std::make_unique<FMSynth>(440.0, SAMPLE_RATE);
        voices[i].note = -1;
    }
    waveformBuffer = std::make_unique<WaveformBuffer>(WAVEFORM_SIZE);

    chorus = std::make_unique<JunoChorus>(SAMPLE_RATE);
    reverbL = std::make_unique<AtmosphericReverb>(SAMPLE_RATE);
    reverbR = std::make_unique<AtmosphericReverb>(SAMPLE_RATE);
    filterL = std::make_unique<Filter>(SAMPLE_RATE);
    filterR = std::make_unique<Filter>(SAMPLE_RATE);

    RtAudio dac;

    if (dac.getDeviceCount() < 1) {
        std::cout << "No audio devices found!" << std::endl;
        return 1;
    }

    RtAudio::StreamParameters parameters;
    parameters.deviceId = dac.getDefaultOutputDevice();
    parameters.nChannels = 2;
    parameters.firstChannel = 0;

    unsigned int bufferFrames = 256;

    RtAudioErrorType result = dac.openStream(&parameters, NULL, RTAUDIO_FLOAT64,
                  (unsigned int)SAMPLE_RATE, &bufferFrames,
                  &audioCallback, nullptr);

    if (result != RTAUDIO_NO_ERROR) {
        std::cout << "Error opening audio stream" << std::endl;
        return 1;
    }

    result = dac.startStream();
    if (result != RTAUDIO_NO_ERROR) {
        std::cout << "Error starting audio stream" << std::endl;
        return 1;
    }

    const int screenWidth = 1050;
    const int screenHeight = 430;

    InitWindow(screenWidth, screenHeight, "FM Synth - 4 Operators / 6 Voices / ADSR");
    SetTargetFPS(60);

    const int whiteKeyMapping[] = { KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON };
    const int whiteKeyNotes[] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16 };
    const int numWhiteKeysMap = 10;

    const int blackKeyMapping[] = { KEY_W, KEY_E, KEY_T, KEY_Y, KEY_U, KEY_O, KEY_P };
    const int blackKeyNotes[] = { 1, 3, 6, 8, 10, 13, 15 };
    const int numBlackKeysMap = 7;

    while (!WindowShouldClose()) {
        // Update all voices
        for (int v = 0; v < NUM_VOICES; v++) {
            voices[v].synth->setRatio1(guiRatio1);
            voices[v].synth->setRatio2(guiRatio2);
            voices[v].synth->setRatio3(guiRatio3);
            voices[v].synth->setRatio4(guiRatio4);
            voices[v].synth->setIndex1(guiIndex1);
            voices[v].synth->setIndex2(guiIndex2);
            voices[v].synth->setIndex3(guiIndex3);
            voices[v].synth->setIndex4(guiIndex4);
            voices[v].synth->setAlgorithm(guiAlgorithm);
            voices[v].synth->setAttack(guiAttack);
            voices[v].synth->setDecay(guiDecay);
            voices[v].synth->setSustain(guiSustain);
            voices[v].synth->setRelease(guiRelease);
        }

        chorusMix.store(guiChorus);
        reverbMix.store(guiReverb);

        filterType.store(guiFilterType);
        if (guiFilterType == FILTER_LOWPASS) {
            filterL->setLowPass(guiFilterCutoff, guiFilterQ);
            filterR->setLowPass(guiFilterCutoff, guiFilterQ);
        } else if (guiFilterType == FILTER_HIGHPASS) {
            filterL->setHighPass(guiFilterCutoff, guiFilterQ);
            filterR->setHighPass(guiFilterCutoff, guiFilterQ);
        }

        std::vector<int> currentKeys;
        for (int i = 0; i < numWhiteKeysMap; i++) {
            if (IsKeyDown(whiteKeyMapping[i])) {
                int note = 12 * currentOctave + whiteKeyNotes[i];
                currentKeys.push_back(note);
            }
        }
        for (int i = 0; i < numBlackKeysMap; i++) {
            if (IsKeyDown(blackKeyMapping[i])) {
                int note = 12 * currentOctave + blackKeyNotes[i];
                currentKeys.push_back(note);
            }
        }

        if (IsKeyPressed(KEY_Z) && currentOctave > 0) currentOctave--;
        if (IsKeyPressed(KEY_X) && currentOctave < 8) currentOctave++;

        int pianoNote = -1;

        BeginDrawing();
        ClearBackground(Color{25, 25, 35, 255});

        // HEADER
        DrawText("FM SYNTH", 20, 10, 26, WHITE);
        DrawText("4 Op / 6 Voices / ADSR", screenWidth - 180, 15, 12, GRAY);
        DrawLine(20, 40, screenWidth - 20, 40, DARKGRAY);

        // ROW 1: 4 OPERATORS
        Color op1Color = Color{180, 100, 60, 255};
        Color op2Color = Color{60, 120, 180, 255};
        Color op3Color = Color{100, 180, 100, 255};
        Color op4Color = Color{180, 100, 180, 255};

        DrawOperatorPanel(20, 50, "OP 1", &guiRatio1, &guiIndex1, op1Color, true, "FB");
        DrawOperatorPanel(100, 50, "OP 2", &guiRatio2, &guiIndex2, op2Color, false, "I");
        DrawOperatorPanel(180, 50, "OP 3", &guiRatio3, &guiIndex3, op3Color, false, "I");
        DrawOperatorPanel(260, 50, "OP 4", &guiRatio4, &guiIndex4, op4Color, false, "I");

        // ADSR Panel
        DrawADSRPanel(345, 50, &guiAttack, &guiDecay, &guiSustain, &guiRelease);

        // ALGORITHM Panel
        DrawRectangle(520, 50, 160, 115, Color{30, 30, 40, 255});
        DrawRectangleLines(520, 50, 160, 115, Color{80, 80, 100, 255});
        DrawText("ALGORITHM", 565, 55, 10, Color{80, 80, 100, 255});

        // Algorithm buttons (2x3 grid)
        for (int i = 0; i < ALG_COUNT; i++) {
            int col = i % 3;
            int row = i / 3;
            int btnX = 528 + col * 50;
            int btnY = 70 + row * 24;
            bool isSelected = (guiAlgorithm == i);
            Color btnColor = isSelected ? Color{80, 80, 180, 255} : Color{50, 50, 60, 255};
            DrawRectangle(btnX, btnY, 46, 20, btnColor);
            DrawRectangleLines(btnX, btnY, 46, 20, isSelected ? WHITE : DARKGRAY);
            int tw = MeasureText(algorithmNames[i], 9);
            DrawText(algorithmNames[i], btnX + (46 - tw) / 2, btnY + 5, 9, isSelected ? WHITE : GRAY);

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mouse = GetMousePosition();
                if (mouse.x >= btnX && mouse.x <= btnX + 46 && mouse.y >= btnY && mouse.y <= btnY + 20) {
                    guiAlgorithm = i;
                }
            }
        }

        DrawAlgorithmDiagram(535, 118, guiAlgorithm);

        // FILTER Panel
        DrawRectangle(695, 50, 110, 115, Color{35, 35, 45, 255});
        DrawRectangleLines(695, 50, 110, 115, Color{200, 100, 150, 255});
        DrawText("FILTER", 730, 55, 10, Color{200, 100, 150, 255});

        const char* filterNames[] = {"OFF", "LP", "HP"};
        for (int i = 0; i < 3; i++) {
            int btnX = 703 + i * 34;
            int btnY = 70;
            bool isSelected = (guiFilterType == i);
            Color btnColor = isSelected ? Color{200, 100, 150, 255} : Color{50, 50, 60, 255};
            DrawRectangle(btnX, btnY, 30, 18, btnColor);
            DrawRectangleLines(btnX, btnY, 30, 18, isSelected ? WHITE : DARKGRAY);
            int tw = MeasureText(filterNames[i], 10);
            DrawText(filterNames[i], btnX + (30 - tw) / 2, btnY + 4, 10, isSelected ? WHITE : GRAY);

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mouse = GetMousePosition();
                if (mouse.x >= btnX && mouse.x <= btnX + 30 && mouse.y >= btnY && mouse.y <= btnY + 18) {
                    guiFilterType = i;
                }
            }
        }

        DrawVerticalSlider(705, 95, 45, "Cut", &guiFilterCutoff, 100.0f, 8000.0f, Color{200, 100, 150, 255});
        DrawVerticalSlider(755, 95, 45, "Res", &guiFilterQ, 0.5f, 8.0f, Color{200, 100, 150, 255});

        // EFFECTS Panel
        DrawRectangle(820, 50, 90, 115, Color{35, 35, 45, 255});
        DrawRectangleLines(820, 50, 90, 115, Color{150, 100, 180, 255});
        DrawText("EFFECTS", 840, 55, 10, Color{150, 100, 180, 255});

        DrawVerticalSlider(830, 72, 55, "Cho", &guiChorus, 0.0f, 1.0f, Color{100, 180, 220, 255});
        DrawVerticalSlider(865, 72, 55, "Rev", &guiReverb, 0.0f, 1.0f, Color{220, 150, 100, 255});

        // RANDOMIZE Button
        {
            int btnX = 925, btnY = 50, btnW = 100, btnH = 30;
            bool hover = false;
            Vector2 mouse = GetMousePosition();
            if (mouse.x >= btnX && mouse.x <= btnX + btnW && mouse.y >= btnY && mouse.y <= btnY + btnH) {
                hover = true;
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    guiRatio1 = 0.5f + (float)(rand() % 150) / 20.0f;
                    guiRatio2 = 0.5f + (float)(rand() % 150) / 20.0f;
                    guiRatio3 = 0.5f + (float)(rand() % 150) / 20.0f;
                    guiRatio4 = 0.5f + (float)(rand() % 150) / 20.0f;
                    guiIndex1 = (float)(rand() % 50) / 10.0f;
                    guiIndex2 = (float)(rand() % 100) / 10.0f;
                    guiIndex3 = (float)(rand() % 100) / 10.0f;
                    guiIndex4 = (float)(rand() % 100) / 10.0f;
                    guiAttack = 0.001f + (float)(rand() % 500) / 1000.0f;
                    guiDecay = 0.01f + (float)(rand() % 500) / 500.0f;
                    guiSustain = 0.2f + (float)(rand() % 80) / 100.0f;
                    guiRelease = 0.05f + (float)(rand() % 200) / 100.0f;
                    guiAlgorithm = rand() % ALG_COUNT;
                }
            }
            Color btnColor = hover ? Color{80, 180, 80, 255} : Color{50, 120, 50, 255};
            DrawRectangle(btnX, btnY, btnW, btnH, btnColor);
            DrawRectangleLines(btnX, btnY, btnW, btnH, Color{100, 200, 100, 255});
            int tw = MeasureText("RANDOMIZE", 11);
            DrawText("RANDOMIZE", btnX + (btnW - tw) / 2, btnY + 9, 11, WHITE);
        }

        // ROW 2: WAVEFORM
        DrawLine(20, 175, screenWidth - 20, 175, DARKGRAY);
        DrawText("WAVEFORM", 20, 182, 10, GRAY);
        DrawWaveform(20, 195, screenWidth - 40, 80);

        // ROW 3: KEYBOARD
        DrawLine(20, 285, screenWidth - 20, 285, DARKGRAY);
        DrawText("KEYBOARD", 20, 292, 10, GRAY);

        // Octave indicator
        {
            DrawText("OCT", 80, 310, 9, Color{100, 100, 120, 255});
            char octStr[8];
            snprintf(octStr, sizeof(octStr), "%d", currentOctave);
            DrawRectangle(105, 306, 22, 18, Color{40, 40, 50, 255});
            DrawRectangleLines(105, 306, 22, 18, Color{80, 80, 100, 255});
            int octW = MeasureText(octStr, 12);
            DrawText(octStr, 116 - octW / 2, 309, 12, WHITE);
            DrawText("Z/X", 90, 328, 8, Color{60, 60, 80, 255});
        }

        // Voices indicator
        {
            int voiceX = screenWidth - 130;
            DrawText("VOICES", voiceX, 310, 9, Color{100, 100, 120, 255});
            for (int v = 0; v < NUM_VOICES; v++) {
                int cx = voiceX + 5 + v * 14;
                int cy = 330;
                bool active = voices[v].synth->isActive();
                Color dotColor = active ? Color{100, 200, 100, 255} : Color{50, 50, 60, 255};
                DrawCircle(cx, cy, 4, dotColor);
                DrawCircleLines(cx, cy, 4, Color{80, 80, 100, 255});
            }
        }

        // Piano
        int pianoX = (screenWidth - 15 * 28) / 2;
        DrawPiano(pianoX, 305, &pianoNote);

        if (pianoNote >= 0) {
            currentKeys.push_back(pianoNote);
        }

        // Note offs
        for (int note : activeNotes) {
            bool stillActive = false;
            for (int k : currentKeys) {
                if (k == note) { stillActive = true; break; }
            }
            if (!stillActive) {
                voiceNoteOff(note);
            }
        }

        // Note ons
        for (int note : currentKeys) {
            bool wasActive = false;
            for (int a : activeNotes) {
                if (a == note) { wasActive = true; break; }
            }
            if (!wasActive) {
                double freq = midiToFreq(note);
                voiceNoteOn(note, freq);
            }
        }

        activeNotes = currentKeys;

        // FOOTER (compacto)
        DrawText("Play with keyboard or mouse  |  Z/X: octave  |  ESC: exit", 20, 410, 9, Color{60, 60, 80, 255});

        EndDrawing();
    }

    CloseWindow();

    if (dac.isStreamRunning()) {
        dac.stopStream();
    }
    if (dac.isStreamOpen()) {
        dac.closeStream();
    }

    return 0;
}
