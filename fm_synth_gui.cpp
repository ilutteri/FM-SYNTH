#include <iostream>
#include <cmath>
#include <vector>
#include <memory>
#include <atomic>
#include <cstdlib>
#include <ctime>
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
// Algoritmos FM de 3 operadores
// ============================================================================
//
// Algoritmo 1: Serie      Op3 -> Op2 -> Op1 (carrier)
// Algoritmo 2: Paralelo   Op2 + Op3 -> Op1 (carrier)
// Algoritmo 3: Y          Op3 -> Op1, Op3 -> Op2, Op1 + Op2 (carriers)
// Algoritmo 4: Doble      Op3 -> Op2 -> Op1, Op3 -> Op1 (feedback-like)
//
// ============================================================================

enum FMAlgorithm {
    ALG_SERIE = 0,      // Op3 -> Op2 -> Op1
    ALG_PARALELO,       // (Op2 + Op3) -> Op1
    ALG_Y,              // Op3 -> ambos, Op1 + Op2 suenan
    ALG_DOBLE,          // Op3 -> Op2 -> Op1, y Op3 -> Op1 directo
    ALG_COUNT
};

const char* algorithmNames[] = {
    "Serie: 3->2->1",
    "Paralelo: (2+3)->1",
    "Y: 3->(1,2)",
    "Doble: 3->2->1 + 3->1"
};

// ============================================================================
// FMSynth con 3 operadores
// ============================================================================

class FMSynth {
private:
    Oscillator op1;  // Carrier principal
    Oscillator op2;  // Modulador/Carrier
    Oscillator op3;  // Modulador

    std::atomic<double> ratio1;
    std::atomic<double> ratio2;
    std::atomic<double> ratio3;
    std::atomic<double> index1;  // Feedback de Op1 (auto-modulación)
    std::atomic<double> index2;  // Índice de modulación Op2->Op1
    std::atomic<double> index3;  // Índice de modulación Op3

    double prevSample1;  // Para feedback

    std::atomic<int> algorithm;
    double amplitude;
    std::atomic<bool> isActive;
    std::atomic<double> currentFrequency;
    double sampleRate;

public:
    FMSynth(double freq, double sr)
        : op1(freq, sr),
          op2(freq * 2.0, sr),
          op3(freq * 3.0, sr),
          ratio1(1.0),
          ratio2(2.0),
          ratio3(3.0),
          index1(0.0),
          index2(2.0),
          index3(1.5),
          prevSample1(0.0),
          algorithm(ALG_SERIE),
          amplitude(0.3),
          isActive(false),
          currentFrequency(freq),
          sampleRate(sr) {
    }

    double process() {
        if (!isActive.load()) return 0.0;

        double out1, out2, out3;
        double idx1 = index1.load();
        double idx2 = index2.load();
        double idx3 = index3.load();

        // Feedback de Op1
        double feedback = idx1 * prevSample1;

        switch (algorithm.load()) {
            case ALG_SERIE:
                // Op3 -> Op2 -> Op1
                out3 = op3.process();
                out2 = op2.process(idx3 * out3);
                out1 = op1.process(idx2 * out2 + feedback);
                break;

            case ALG_PARALELO:
                // (Op2 + Op3) -> Op1
                out2 = op2.process();
                out3 = op3.process();
                out1 = op1.process(idx2 * out2 + idx3 * out3 + feedback);
                break;

            case ALG_Y:
                // Op3 modula a ambos, Op1 y Op2 son carriers
                out3 = op3.process();
                out1 = op1.process(idx3 * out3 + feedback);
                out2 = op2.process(idx3 * out3);
                prevSample1 = out1;
                return (out1 + out2 * 0.5) * amplitude * 0.7;

            case ALG_DOBLE:
                // Op3 -> Op2 -> Op1, también Op3 -> Op1 directo
                out3 = op3.process();
                out2 = op2.process(idx3 * out3);
                out1 = op1.process(idx2 * out2 + idx3 * out3 * 0.5 + feedback);
                break;

            default:
                return 0.0;
        }

        prevSample1 = out1;
        return out1 * amplitude;
    }

    void noteOn(double freq) {
        currentFrequency.store(freq);
        op1.setFrequency(freq * ratio1.load());
        op2.setFrequency(freq * ratio2.load());
        op3.setFrequency(freq * ratio3.load());
        op1.reset();
        op2.reset();
        op3.reset();
        prevSample1 = 0.0;
        isActive.store(true);
    }

    void noteOff() {
        isActive.store(false);
    }

    // Setters
    void setRatio1(double r) {
        ratio1.store(r);
        if (isActive.load()) {
            op1.setFrequency(currentFrequency.load() * r);
        }
    }

    void setRatio2(double r) {
        ratio2.store(r);
        if (isActive.load()) {
            op2.setFrequency(currentFrequency.load() * r);
        }
    }

    void setRatio3(double r) {
        ratio3.store(r);
        if (isActive.load()) {
            op3.setFrequency(currentFrequency.load() * r);
        }
    }

    void setIndex1(double i) { index1.store(i); }
    void setIndex2(double i) { index2.store(i); }
    void setIndex3(double i) { index3.store(i); }
    void setAlgorithm(int alg) { algorithm.store(alg); }

    // Getters
    double getRatio1() const { return ratio1.load(); }
    double getRatio2() const { return ratio2.load(); }
    double getRatio3() const { return ratio3.load(); }
    double getIndex1() const { return index1.load(); }
    double getIndex2() const { return index2.load(); }
    double getIndex3() const { return index3.load(); }
    int getAlgorithm() const { return algorithm.load(); }
    double getCurrentFrequency() const { return currentFrequency.load(); }
    bool getIsActive() const { return isActive.load(); }
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

    // Parámetros Juno-style (fijos)
    const double lfoRate1 = 0.513;   // Hz - Mode I rate
    const double lfoRate2 = 0.863;   // Hz - Mode II rate
    const double baseDelay = 0.005;  // 5ms base delay
    const double depth = 0.003;      // 3ms modulation depth

public:
    JunoChorus(double sr) : sampleRate(sr), writeIndex(0), lfoPhase1(0), lfoPhase2(0) {
        delayLineL.resize(MAX_DELAY, 0.0);
        delayLineR.resize(MAX_DELAY, 0.0);
    }

    void process(double input, double& outL, double& outR, double mix) {
        // Escribir en delay line
        delayLineL[writeIndex] = input;
        delayLineR[writeIndex] = input;

        // LFOs para modulación (dos LFOs desfasados para stereo)
        double lfo1 = std::sin(lfoPhase1 * 2.0 * 3.14159265);
        double lfo2 = std::sin(lfoPhase2 * 2.0 * 3.14159265 + 1.5708); // 90° desfasado

        // Calcular delay times modulados
        double delayTimeL = baseDelay + depth * lfo1;
        double delayTimeR = baseDelay + depth * lfo2;

        // Convertir a samples
        double delaySamplesL = delayTimeL * sampleRate;
        double delaySamplesR = delayTimeR * sampleRate;

        // Leer con interpolación lineal
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

        // Mix dry/wet
        outL = input * (1.0 - mix) + wetL * mix;
        outR = input * (1.0 - mix) + wetR * mix;

        // Avanzar LFOs
        lfoPhase1 += lfoRate1 / sampleRate;
        lfoPhase2 += lfoRate2 / sampleRate;
        if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0;
        if (lfoPhase2 >= 1.0) lfoPhase2 -= 1.0;

        // Avanzar write index
        writeIndex = (writeIndex + 1) % MAX_DELAY;
    }
};

// ============================================================================
// Reverb - Atmosférica (Schroeder style)
// ============================================================================

class AtmosphericReverb {
private:
    // Comb filters (4 en paralelo)
    static const int NUM_COMBS = 4;
    std::vector<std::vector<double>> combBuffers;
    std::vector<int> combDelays;
    std::vector<int> combIndices;
    std::vector<double> combFilters;  // Lowpass state

    // Allpass filters (2 en serie)
    static const int NUM_ALLPASS = 2;
    std::vector<std::vector<double>> allpassBuffers;
    std::vector<int> allpassDelays;
    std::vector<int> allpassIndices;

    double decay;
    double damping;
    double sampleRate;

public:
    AtmosphericReverb(double sr) : sampleRate(sr), decay(0.85), damping(0.3) {
        // Delays en samples (números primos para evitar resonancias)
        // Largos para reverb atmosférica
        combDelays = {1687, 1931, 2053, 2251};
        allpassDelays = {547, 331};

        // Ajustar para sample rate
        double srRatio = sr / 44100.0;
        for (auto& d : combDelays) d = (int)(d * srRatio);
        for (auto& d : allpassDelays) d = (int)(d * srRatio);

        // Inicializar buffers
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

        // Procesar comb filters en paralelo
        for (int i = 0; i < NUM_COMBS; i++) {
            int idx = combIndices[i];
            double delayed = combBuffers[i][idx];

            // Lowpass filter en el feedback (damping)
            combFilters[i] = delayed * (1.0 - damping) + combFilters[i] * damping;

            // Feedback con decay
            double feedback = combFilters[i] * decay;
            combBuffers[i][idx] = input + feedback;

            wet += delayed;

            combIndices[i] = (idx + 1) % combDelays[i];
        }

        wet *= 0.25;  // Normalizar (4 combs)

        // Procesar allpass filters en serie
        for (int i = 0; i < NUM_ALLPASS; i++) {
            int idx = allpassIndices[i];
            double delayed = allpassBuffers[i][idx];
            double g = 0.5;  // Allpass coefficient

            double output = -g * wet + delayed;
            allpassBuffers[i][idx] = wet + g * output;
            wet = output;

            allpassIndices[i] = (idx + 1) % allpassDelays[i];
        }

        // Mix dry/wet
        return input * (1.0 - mix) + wet * mix;
    }

    void clear() {
        for (auto& buf : combBuffers) std::fill(buf.begin(), buf.end(), 0.0);
        for (auto& buf : allpassBuffers) std::fill(buf.begin(), buf.end(), 0.0);
        std::fill(combFilters.begin(), combFilters.end(), 0.0);
    }
};

// ============================================================================
// Filtro - Low Pass / High Pass
// ============================================================================

enum FilterType {
    FILTER_OFF = 0,
    FILTER_LOWPASS,
    FILTER_HIGHPASS
};

class Filter {
private:
    double y1, y2;  // Previous outputs
    double x1, x2;  // Previous inputs
    double a0, a1, a2, b1, b2;  // Coefficients
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

        a0 = b0 / a0_t;
        a1 = b1_t / a0_t;
        a2 = b2_t / a0_t;
        b1 = a1_t / a0_t;
        b2 = a2_t / a0_t;
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

        a0 = b0 / a0_t;
        a1 = b1_t / a0_t;
        a2 = b2_t / a0_t;
        b1 = a1_t / a0_t;
        b2 = a2_t / a0_t;
    }

    double process(double input) {
        double output = a0 * input + a1 * x1 + a2 * x2 - b1 * y1 - b2 * y2;
        x2 = x1;
        x1 = input;
        y2 = y1;
        y1 = output;
        return output;
    }

    void reset() {
        y1 = y2 = x1 = x2 = 0.0;
    }
};

// ============================================================================
// Polifonía - 6 voces
// ============================================================================

const int NUM_VOICES = 6;

struct Voice {
    std::unique_ptr<FMSynth> synth;
    int note;  // -1 = libre

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

// Parámetros de efectos
std::atomic<double> chorusMix(0.0);
std::atomic<double> reverbMix(0.0);
std::atomic<int> filterType(FILTER_OFF);
std::atomic<double> filterCutoff(2000.0);
std::atomic<double> filterQ(0.707);

// Parámetros de GUI para los 3 operadores
float guiRatio1 = 1.0f;   // Op1 ratio (normalmente 1.0 para carrier)
float guiRatio2 = 2.0f;
float guiRatio3 = 3.0f;
float guiIndex1 = 0.0f;   // Op1 feedback (auto-modulación)
float guiIndex2 = 2.0f;
float guiIndex3 = 1.5f;
float guiChorus = 0.0f;   // Chorus mix (0-1)
float guiReverb = 0.0f;   // Reverb mix (0-1)
int guiFilterType = 0;    // 0=Off, 1=LP, 2=HP
float guiFilterCutoff = 2000.0f;  // Hz
float guiFilterQ = 0.707f;
int guiAlgorithm = 0;
int currentOctave = 4;
std::vector<int> activeNotes;  // Notas actualmente presionadas

// ============================================================================
// Funciones de manejo de voces
// ============================================================================

int findFreeVoice() {
    // Buscar voz libre
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices[i].note == -1) return i;
    }
    // Si no hay libres, robar la primera (voice stealing simple)
    return 0;
}

int findVoiceWithNote(int note) {
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices[i].note == note) return i;
    }
    return -1;
}

void voiceNoteOn(int note, double freq) {
    // Ver si ya está sonando esta nota
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
// Callback de audio
// ============================================================================

int audioCallback(void *outputBuffer, void *inputBuffer, unsigned int nFrames,
                  double streamTime, RtAudioStreamStatus status, void *userData) {

    double *buffer = (double *)outputBuffer;
    double chMix = chorusMix.load();
    double rvMix = reverbMix.load();
    int fType = filterType.load();

    for (unsigned int i = 0; i < nFrames; i++) {
        double sample = 0.0;

        // Mezclar todas las voces
        for (int v = 0; v < NUM_VOICES; v++) {
            sample += voices[v].synth->process();
        }

        // Normalizar para evitar clipping (6 voces)
        sample *= 0.4;

        // Procesar Filtro
        if (fType != FILTER_OFF) {
            sample = filterL->process(sample);
        }

        // Procesar Chorus (genera stereo)
        double outL, outR;
        chorus->process(sample, outL, outR, chMix);

        // Procesar Reverb (instancia separada por canal para stereo real)
        outL = reverbL->process(outL, rvMix);
        outR = reverbR->process(outR, rvMix);

        // Soft clipping para evitar distorsión
        outL = std::tanh(outL);
        outR = std::tanh(outR);

        waveformBuffer->write((float)((outL + outR) * 0.5));
        *buffer++ = outL;
        *buffer++ = outR;
    }

    return 0;
}

// ============================================================================
// Funciones de utilidad
// ============================================================================

double midiToFreq(int midiNote) {
    return 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
}

const char* midiToNoteName(int midiNote) {
    static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return noteNames[midiNote % 12];
}

// ============================================================================
// Dibujo de GUI
// ============================================================================

void DrawVerticalSlider(int x, int y, int height, const char* label, float* value, float minVal, float maxVal, Color trackColor) {
    // Label arriba
    int labelWidth = MeasureText(label, 11);
    DrawText(label, x + 15 - labelWidth / 2, y, 11, WHITE);

    // Track vertical
    int trackX = x + 10;
    int trackY = y + 16;
    int trackWidth = 10;
    DrawRectangle(trackX, trackY, trackWidth, height, Color{40, 40, 50, 255});
    DrawRectangleLines(trackX, trackY, trackWidth, height, DARKGRAY);

    // Fill del track (de abajo hacia arriba)
    float normalized = (*value - minVal) / (maxVal - minVal);
    int fillHeight = (int)(normalized * height);
    DrawRectangle(trackX + 1, trackY + height - fillHeight, trackWidth - 2, fillHeight, trackColor);

    // Handle
    int handleY = trackY + height - (int)(normalized * height) - 4;
    DrawRectangle(trackX - 3, handleY, trackWidth + 6, 8, RAYWHITE);

    // Valor abajo
    char valueText[16];
    snprintf(valueText, sizeof(valueText), "%.1f", *value);
    int valWidth = MeasureText(valueText, 10);
    DrawText(valueText, x + 15 - valWidth / 2, trackY + height + 6, 10, GRAY);

    // Interacción
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
    int panelWidth = 80;
    int panelHeight = 140;

    // Fondo del panel
    DrawRectangle(x, y, panelWidth, panelHeight, Color{35, 35, 45, 255});
    DrawRectangleLines(x, y, panelWidth, panelHeight, color);

    // Nombre del operador
    int nameWidth = MeasureText(name, 14);
    DrawText(name, x + (panelWidth - nameWidth) / 2, y + 5, 14, color);

    // Indicador carrier/mod
    const char* typeLabel = isCarrier ? "[C]" : "[M]";
    int typeWidth = MeasureText(typeLabel, 10);
    DrawText(typeLabel, x + (panelWidth - typeWidth) / 2, y + 22, 10, isCarrier ? Color{180, 100, 60, 255} : Color{60, 120, 180, 255});

    // Sliders verticales
    DrawVerticalSlider(x + 5, y + 38, 70, "Ratio", ratio, 0.5f, 8.0f, color);
    DrawVerticalSlider(x + 42, y + 38, 70, indexLabel, index, 0.0f, 10.0f, color);
}

void DrawAlgorithmSelector(int x, int y, int* selected) {
    DrawText("Algoritmo:", x, y, 12, WHITE);

    int btnWidth = 60;
    int btnHeight = 28;

    // 2x2 grid
    for (int i = 0; i < ALG_COUNT; i++) {
        int col = i % 2;
        int row = i / 2;
        int btnX = x + col * (btnWidth + 4);
        int btnY = y + 18 + row * (btnHeight + 4);
        bool isSelected = (*selected == i);

        Color btnColor = isSelected ? Color{80, 80, 180, 255} : Color{50, 50, 60, 255};
        Color textColor = isSelected ? WHITE : GRAY;

        DrawRectangle(btnX, btnY, btnWidth, btnHeight, btnColor);
        DrawRectangleLines(btnX, btnY, btnWidth, btnHeight, isSelected ? WHITE : DARKGRAY);

        // Nombres cortos
        const char* shortNames[] = {"Serie", "Paral.", "Y", "Doble"};
        int textWidth = MeasureText(shortNames[i], 10);
        DrawText(shortNames[i], btnX + (btnWidth - textWidth) / 2, btnY + 8, 10, textColor);

        // Click detection
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mouse = GetMousePosition();
            if (mouse.x >= btnX && mouse.x <= btnX + btnWidth &&
                mouse.y >= btnY && mouse.y <= btnY + btnHeight) {
                *selected = i;
            }
        }
    }
}

void DrawAlgorithmDiagram(int x, int y, int algorithm) {
    int boxW = 30, boxH = 20;
    Color opColor = Color{60, 120, 180, 255};
    Color carrierColor = Color{180, 100, 60, 255};

    DrawRectangle(x - 5, y - 5, 130, 60, Color{30, 30, 40, 255});

    switch (algorithm) {
        case ALG_SERIE:
            // Op3 -> Op2 -> Op1
            DrawRectangle(x, y + 15, boxW, boxH, opColor);
            DrawText("3", x + 10, y + 18, 14, WHITE);
            DrawText("->", x + 35, y + 18, 14, GRAY);
            DrawRectangle(x + 50, y + 15, boxW, boxH, opColor);
            DrawText("2", x + 60, y + 18, 14, WHITE);
            DrawText("->", x + 85, y + 18, 14, GRAY);
            DrawRectangle(x + 100, y + 15, boxW, boxH, carrierColor);
            DrawText("1", x + 110, y + 18, 14, WHITE);
            break;

        case ALG_PARALELO:
            // (2 + 3) -> 1
            DrawRectangle(x, y + 5, boxW, boxH, opColor);
            DrawText("2", x + 10, y + 8, 14, WHITE);
            DrawRectangle(x, y + 30, boxW, boxH, opColor);
            DrawText("3", x + 10, y + 33, 14, WHITE);
            DrawText("+", x + 38, y + 18, 16, GRAY);
            DrawText("->", x + 55, y + 18, 14, GRAY);
            DrawRectangle(x + 75, y + 15, boxW, boxH, carrierColor);
            DrawText("1", x + 85, y + 18, 14, WHITE);
            break;

        case ALG_Y:
            // 3 -> (1, 2)
            DrawRectangle(x, y + 15, boxW, boxH, opColor);
            DrawText("3", x + 10, y + 18, 14, WHITE);
            DrawLine(x + 35, y + 25, x + 55, y + 12, GRAY);
            DrawLine(x + 35, y + 25, x + 55, y + 38, GRAY);
            DrawRectangle(x + 60, y, boxW, boxH, carrierColor);
            DrawText("1", x + 70, y + 3, 14, WHITE);
            DrawRectangle(x + 60, y + 30, boxW, boxH, carrierColor);
            DrawText("2", x + 70, y + 33, 14, WHITE);
            break;

        case ALG_DOBLE:
            // 3 -> 2 -> 1, 3 -> 1
            DrawRectangle(x, y + 15, boxW, boxH, opColor);
            DrawText("3", x + 10, y + 18, 14, WHITE);
            DrawText("->", x + 35, y + 18, 14, GRAY);
            DrawRectangle(x + 50, y + 15, boxW, boxH, opColor);
            DrawText("2", x + 60, y + 18, 14, WHITE);
            DrawText("->", x + 85, y + 18, 14, GRAY);
            DrawRectangle(x + 100, y + 15, boxW, boxH, carrierColor);
            DrawText("1", x + 110, y + 18, 14, WHITE);
            DrawLine(x + 15, y + 35, x + 115, y + 45, Color{100, 100, 120, 255});
            DrawLine(x + 115, y + 45, x + 115, y + 35, Color{100, 100, 120, 255});
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
    const int whiteKeyWidth = 32;
    const int whiteKeyHeight = 85;
    const int blackKeyWidth = 20;
    const int blackKeyHeight = 50;

    // 2 octavas: 15 teclas blancas (C a C)
    const int numWhiteKeys = 15;
    const int whiteNotes[] = {0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23, 24};

    // Teclas negras para 2 octavas
    const int numBlackKeys = 10;
    const int blackNotes[] = {1, 3, 6, 8, 10, 13, 15, 18, 20, 22};
    // Posiciones relativas de teclas negras (después de cada tecla blanca excepto E y B)
    const int blackPositions[] = {0, 1, 3, 4, 5, 7, 8, 10, 11, 12}; // índice de tecla blanca a la izquierda

    int baseMidi = 12 * currentOctave;

    Vector2 mouse = GetMousePosition();
    bool mouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);

    *pressedNote = -1;

    // Dibujar teclas blancas
    for (int i = 0; i < numWhiteKeys; i++) {
        int keyX = x + i * whiteKeyWidth;
        int midiNote = baseMidi + whiteNotes[i];
        bool isPressed = isNoteActive(midiNote);

        // Detectar click en tecla blanca
        if (mouseDown && mouse.x >= keyX && mouse.x < keyX + whiteKeyWidth &&
            mouse.y >= y && mouse.y < y + whiteKeyHeight) {
            // Verificar que no estemos sobre una tecla negra
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

        // Nombre de nota solo en C
        if (whiteNotes[i] % 12 == 0) {
            char noteLabel[8];
            snprintf(noteLabel, sizeof(noteLabel), "C%d", (baseMidi + whiteNotes[i]) / 12);
            int textWidth = MeasureText(noteLabel, 10);
            DrawText(noteLabel, keyX + (whiteKeyWidth - textWidth) / 2, y + whiteKeyHeight - 14, 10, BLACK);
        }
    }

    // Dibujar teclas negras
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
    // Inicializar semilla para randomize
    srand((unsigned int)time(NULL));

    // Inicializar las 6 voces
    for (int i = 0; i < NUM_VOICES; i++) {
        voices[i].synth = std::make_unique<FMSynth>(440.0, SAMPLE_RATE);
        voices[i].note = -1;
    }
    waveformBuffer = std::make_unique<WaveformBuffer>(WAVEFORM_SIZE);

    // Inicializar efectos
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

    const int screenWidth = 950;
    const int screenHeight = 600;

    InitWindow(screenWidth, screenHeight, "FM Synth - 3 Operators / 6 Voices");
    SetTargetFPS(60);

    // Mapeo de teclas estilo piano:
    // Fila inferior (blancas): A S D F G H J K L ; = C D E F G A B C D E
    // Fila superior (negras):  W E   T Y U   O P   = C# D#  F# G# A#  C# D#
    const int whiteKeyMapping[] = {
        KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON
    };
    const int whiteKeyNotes[] = {
        0, 2, 4, 5, 7, 9, 11, 12, 14, 16  // C D E F G A B C D E
    };
    const int numWhiteKeys = 10;

    const int blackKeyMapping[] = {
        KEY_W, KEY_E, KEY_T, KEY_Y, KEY_U, KEY_O, KEY_P
    };
    const int blackKeyNotes[] = {
        1, 3, 6, 8, 10, 13, 15  // C# D# F# G# A# C# D#
    };
    const int numBlackKeys = 7;

    while (!WindowShouldClose()) {
        // Actualizar parámetros de todas las voces
        for (int v = 0; v < NUM_VOICES; v++) {
            voices[v].synth->setRatio1(guiRatio1);
            voices[v].synth->setRatio2(guiRatio2);
            voices[v].synth->setRatio3(guiRatio3);
            voices[v].synth->setIndex1(guiIndex1);
            voices[v].synth->setIndex2(guiIndex2);
            voices[v].synth->setIndex3(guiIndex3);
            voices[v].synth->setAlgorithm(guiAlgorithm);
        }

        // Actualizar parámetros de efectos
        chorusMix.store(guiChorus);
        reverbMix.store(guiReverb);

        // Actualizar filtro
        filterType.store(guiFilterType);
        if (guiFilterType == FILTER_LOWPASS) {
            filterL->setLowPass(guiFilterCutoff, guiFilterQ);
            filterR->setLowPass(guiFilterCutoff, guiFilterQ);
        } else if (guiFilterType == FILTER_HIGHPASS) {
            filterL->setHighPass(guiFilterCutoff, guiFilterQ);
            filterR->setHighPass(guiFilterCutoff, guiFilterQ);
        }

        // Recolectar todas las teclas presionadas (blancas y negras)
        std::vector<int> currentKeys;
        for (int i = 0; i < numWhiteKeys; i++) {
            if (IsKeyDown(whiteKeyMapping[i])) {
                int note = 12 * currentOctave + whiteKeyNotes[i];
                currentKeys.push_back(note);
            }
        }
        for (int i = 0; i < numBlackKeys; i++) {
            if (IsKeyDown(blackKeyMapping[i])) {
                int note = 12 * currentOctave + blackKeyNotes[i];
                currentKeys.push_back(note);
            }
        }

        if (IsKeyPressed(KEY_Z) && currentOctave > 0) currentOctave--;
        if (IsKeyPressed(KEY_X) && currentOctave < 8) currentOctave++;

        // Recolectar notas del piano (mouse)
        int pianoNote = -1;

        BeginDrawing();
        ClearBackground(Color{25, 25, 35, 255});

        // =====================================================================
        // HEADER
        // =====================================================================
        DrawText("FM SYNTH", 25, 12, 28, WHITE);
        DrawText("3 Op / 6 Voices", screenWidth - 140, 18, 14, GRAY);
        DrawLine(25, 45, screenWidth - 25, 45, DARKGRAY);

        // =====================================================================
        // FILA 1: OPERADORES + ALGORITMO + RANDOMIZE
        // =====================================================================
        Color op1Color = Color{180, 100, 60, 255};   // Naranja (carrier)
        Color op2Color = Color{60, 120, 180, 255};   // Azul (mod)
        Color op3Color = Color{100, 180, 100, 255};  // Verde (mod)

        // Operadores
        DrawOperatorPanel(25, 55, "OP 1", &guiRatio1, &guiIndex1, op1Color, true, "FB");
        DrawOperatorPanel(115, 55, "OP 2", &guiRatio2, &guiIndex2, op2Color, false, "Index");
        DrawOperatorPanel(205, 55, "OP 3", &guiRatio3, &guiIndex3, op3Color, false, "Index");

        // Panel ALGORITMO (más ancho para ver todos los botones)
        DrawRectangle(295, 55, 200, 140, Color{30, 30, 40, 255});
        DrawRectangleLines(295, 55, 200, 140, Color{80, 80, 100, 255});
        DrawText("ALGORITHM", 360, 60, 11, Color{80, 80, 100, 255});

        // Selector de algoritmos en fila horizontal
        const char* algNames[] = {"Serie", "Parallel", "Y", "Double"};
        for (int i = 0; i < 4; i++) {
            int btnX = 305 + i * 46;
            int btnY = 78;
            bool isSelected = (guiAlgorithm == i);
            Color btnColor = isSelected ? Color{80, 80, 180, 255} : Color{50, 50, 60, 255};
            DrawRectangle(btnX, btnY, 42, 22, btnColor);
            DrawRectangleLines(btnX, btnY, 42, 22, isSelected ? WHITE : DARKGRAY);
            int tw = MeasureText(algNames[i], 9);
            DrawText(algNames[i], btnX + (42 - tw) / 2, btnY + 6, 9, isSelected ? WHITE : GRAY);

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mouse = GetMousePosition();
                if (mouse.x >= btnX && mouse.x <= btnX + 42 && mouse.y >= btnY && mouse.y <= btnY + 22) {
                    guiAlgorithm = i;
                }
            }
        }

        // Diagrama del algoritmo
        DrawAlgorithmDiagram(320, 105, guiAlgorithm);

        // Botón RANDOMIZE
        {
            int btnX = 305, btnY = 170, btnW = 180, btnH = 20;
            bool hover = false;
            Vector2 mouse = GetMousePosition();
            if (mouse.x >= btnX && mouse.x <= btnX + btnW && mouse.y >= btnY && mouse.y <= btnY + btnH) {
                hover = true;
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    guiRatio1 = 0.5f + (float)(rand() % 150) / 20.0f;
                    guiRatio2 = 0.5f + (float)(rand() % 150) / 20.0f;
                    guiRatio3 = 0.5f + (float)(rand() % 150) / 20.0f;
                    guiIndex1 = (float)(rand() % 100) / 10.0f;
                    guiIndex2 = (float)(rand() % 100) / 10.0f;
                    guiIndex3 = (float)(rand() % 100) / 10.0f;
                    guiAlgorithm = rand() % 4;
                }
            }
            Color btnColor = hover ? Color{80, 180, 80, 255} : Color{50, 120, 50, 255};
            DrawRectangle(btnX, btnY, btnW, btnH, btnColor);
            DrawRectangleLines(btnX, btnY, btnW, btnH, Color{100, 200, 100, 255});
            int tw = MeasureText("RANDOMIZE", 11);
            DrawText("RANDOMIZE", btnX + (btnW - tw) / 2, btnY + 5, 11, WHITE);
        }

        // =====================================================================
        // FILA 1: FILTRO + EFECTOS (derecha)
        // =====================================================================

        // Panel FILTRO
        DrawRectangle(505, 55, 130, 140, Color{35, 35, 45, 255});
        DrawRectangleLines(505, 55, 130, 140, Color{200, 100, 150, 255});
        DrawText("FILTER", 550, 60, 11, Color{200, 100, 150, 255});

        // Selector de tipo de filtro
        const char* filterNames[] = {"OFF", "LP", "HP"};
        for (int i = 0; i < 3; i++) {
            int btnX = 513 + i * 40;
            int btnY = 78;
            bool isSelected = (guiFilterType == i);
            Color btnColor = isSelected ? Color{200, 100, 150, 255} : Color{50, 50, 60, 255};
            DrawRectangle(btnX, btnY, 36, 20, btnColor);
            DrawRectangleLines(btnX, btnY, 36, 20, isSelected ? WHITE : DARKGRAY);
            int tw = MeasureText(filterNames[i], 11);
            DrawText(filterNames[i], btnX + (36 - tw) / 2, btnY + 5, 11, isSelected ? WHITE : GRAY);

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mouse = GetMousePosition();
                if (mouse.x >= btnX && mouse.x <= btnX + 36 && mouse.y >= btnY && mouse.y <= btnY + 20) {
                    guiFilterType = i;
                }
            }
        }

        // Sliders de filtro
        DrawVerticalSlider(515, 105, 55, "Cutoff", &guiFilterCutoff, 100.0f, 8000.0f, Color{200, 100, 150, 255});
        DrawVerticalSlider(575, 105, 55, "Reso", &guiFilterQ, 0.5f, 8.0f, Color{200, 100, 150, 255});

        // Panel EFECTOS
        DrawRectangle(645, 55, 100, 140, Color{35, 35, 45, 255});
        DrawRectangleLines(645, 55, 100, 140, Color{150, 100, 180, 255});
        DrawText("EFFECTS", 670, 60, 11, Color{150, 100, 180, 255});

        DrawVerticalSlider(655, 80, 65, "Chorus", &guiChorus, 0.0f, 1.0f, Color{100, 180, 220, 255});
        DrawVerticalSlider(700, 80, 65, "Reverb", &guiReverb, 0.0f, 1.0f, Color{220, 150, 100, 255});

        // =====================================================================
        // FILA 2: WAVEFORM
        // =====================================================================
        DrawLine(25, 205, screenWidth - 25, 205, DARKGRAY);
        DrawText("WAVEFORM", 25, 215, 11, GRAY);
        DrawWaveform(25, 230, screenWidth - 50, 90);

        // =====================================================================
        // FILA 3: PIANO + VOICES + OCTAVE
        // =====================================================================
        DrawLine(25, 330, screenWidth - 25, 330, DARKGRAY);
        DrawText("KEYBOARD", 25, 340, 11, GRAY);

        // Octave indicator (pequeño, a la izquierda del piano)
        {
            DrawText("OCT", 130, 375, 10, Color{100, 100, 120, 255});
            char octStr[8];
            snprintf(octStr, sizeof(octStr), "%d", currentOctave);
            DrawRectangle(155, 370, 24, 22, Color{40, 40, 50, 255});
            DrawRectangleLines(155, 370, 24, 22, Color{80, 80, 100, 255});
            int octW = MeasureText(octStr, 14);
            DrawText(octStr, 167 - octW / 2, 374, 14, WHITE);

            // Flechitas Z/X
            DrawText("Z", 135, 398, 9, Color{80, 80, 100, 255});
            DrawText("-", 145, 398, 9, Color{80, 80, 100, 255});
            DrawText("+", 165, 398, 9, Color{80, 80, 100, 255});
            DrawText("X", 175, 398, 9, Color{80, 80, 100, 255});
        }

        // Voices indicator (pequeño, a la derecha del piano)
        {
            int voiceX = 730;
            DrawText("VOICES", voiceX, 375, 10, Color{100, 100, 120, 255});

            // Contar voces activas
            int activeVoiceCount = 0;
            for (int v = 0; v < NUM_VOICES; v++) {
                if (voices[v].note != -1) activeVoiceCount++;
            }

            // 6 círculos pequeños indicando voces
            for (int v = 0; v < NUM_VOICES; v++) {
                int cx = voiceX + 5 + v * 14;
                int cy = 400;
                bool active = (voices[v].note != -1);
                Color dotColor = active ? Color{100, 200, 100, 255} : Color{50, 50, 60, 255};
                DrawCircle(cx, cy, 5, dotColor);
                DrawCircleLines(cx, cy, 5, Color{80, 80, 100, 255});
            }
        }

        // Piano centrado (15 teclas * 32px = 480px, ventana 950px)
        DrawPiano(235, 355, &pianoNote);

        // Agregar nota del piano si está presionada
        if (pianoNote >= 0) {
            currentKeys.push_back(pianoNote);
        }

        // Detectar note-offs (notas que estaban activas pero ya no)
        for (int note : activeNotes) {
            bool stillActive = false;
            for (int k : currentKeys) {
                if (k == note) { stillActive = true; break; }
            }
            if (!stillActive) {
                voiceNoteOff(note);
            }
        }

        // Detectar note-ons (notas nuevas)
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

        // Actualizar lista de notas activas
        activeNotes = currentKeys;

        // =====================================================================
        // FOOTER: Instrucciones
        // =====================================================================
        DrawLine(25, 450, screenWidth - 25, 450, DARKGRAY);

        // Leyenda de colores
        DrawRectangle(25, 465, 14, 14, Color{180, 100, 60, 255});
        DrawText("Carrier", 45, 467, 11, GRAY);
        DrawRectangle(110, 465, 14, 14, Color{60, 120, 180, 255});
        DrawText("Modulator", 130, 467, 11, GRAY);

        // Filtro info
        const char* fTypeStr = guiFilterType == 0 ? "Off" : (guiFilterType == 1 ? "LowPass" : "HighPass");
        char filterInfo[64];
        snprintf(filterInfo, sizeof(filterInfo), "Filter: %s (%.0f Hz)", fTypeStr, guiFilterCutoff);
        DrawText(filterInfo, 230, 467, 11, Color{200, 100, 150, 255});

        // Signal chain
        DrawText("Signal: Synth -> Filter -> Chorus -> Reverb -> Out", 450, 467, 11, Color{100, 100, 120, 255});

        // Controles (sutil)
        DrawText("Play with keyboard or mouse  |  Z/X: octave  |  ESC: exit", 25, 575, 11, Color{80, 80, 100, 255});

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
