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

// Headers del synth
#include "synth/constants.h"
#include "synth/oscillator.h"
#include "synth/envelope.h"
#include "synth/lfo.h"
#include "synth/filter.h"
#include "synth/effects.h"
#include "synth/fm_synth.h"
#include "synth/waveform_buffer.h"
#include "synth/voice.h"

// Headers de GUI
#include "gui/gui_utils.h"
#include "gui/lfo_dropdown.h"
#include "gui/presets.h"

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

// GUI params (Init preset: pure sine wave)
float guiRatio1 = 1.0f, guiRatio2 = 1.0f, guiRatio3 = 1.0f, guiRatio4 = 1.0f;
float guiIndex1 = 0.0f, guiIndex2 = 0.0f, guiIndex3 = 0.0f, guiIndex4 = 0.0f;
float guiAttack = 0.01f, guiDecay = 0.1f, guiSustain = 1.0f, guiRelease = 0.2f;
float guiChorus = 0.0f, guiReverb = 0.0f;
int guiFilterType = 0;
float guiFilterCutoff = 2000.0f, guiFilterQ = 0.707f;
int guiAlgorithm = 0;
int currentOctave = 5;
std::vector<int> activeNotes;

// LFO params
float guiLfo1Rate = 2.0f, guiLfo1Depth = 0.0f;
float guiLfo2Rate = 4.0f, guiLfo2Depth = 0.0f;
int guiLfo1Target = LFO_OFF, guiLfo2Target = LFO_OFF;
bool lfo1DropdownOpen = false, lfo2DropdownOpen = false;
std::unique_ptr<LFO> lfo1, lfo2;

// Mod Envelope params
float guiModAttack = 0.01f, guiModDecay = 0.3f, guiModSustain = 0.0f, guiModRelease = 0.2f;
float guiModAmount = 0.0f;
int guiModEnvTarget = MODENV_OFF;
bool modEnvDropdownOpen = false;

// Preset system
Preset presets[NUM_PRESETS];
int currentPreset = 0;

void saveToPreset(int idx) {
    if (idx < 0 || idx >= NUM_PRESETS) return;
    presets[idx].ratio1 = guiRatio1; presets[idx].ratio2 = guiRatio2;
    presets[idx].ratio3 = guiRatio3; presets[idx].ratio4 = guiRatio4;
    presets[idx].index1 = guiIndex1; presets[idx].index2 = guiIndex2;
    presets[idx].index3 = guiIndex3; presets[idx].index4 = guiIndex4;
    presets[idx].attack = guiAttack; presets[idx].decay = guiDecay;
    presets[idx].sustain = guiSustain; presets[idx].release = guiRelease;
    presets[idx].algorithm = guiAlgorithm;
    presets[idx].filterCutoff = guiFilterCutoff; presets[idx].filterQ = guiFilterQ;
    presets[idx].filterType = guiFilterType;
    presets[idx].chorus = guiChorus; presets[idx].reverb = guiReverb;
    presets[idx].lfo1Rate = guiLfo1Rate; presets[idx].lfo1Depth = guiLfo1Depth;
    presets[idx].lfo2Rate = guiLfo2Rate; presets[idx].lfo2Depth = guiLfo2Depth;
    presets[idx].lfo1Target = guiLfo1Target; presets[idx].lfo2Target = guiLfo2Target;
    presets[idx].modAttack = guiModAttack; presets[idx].modDecay = guiModDecay;
    presets[idx].modSustain = guiModSustain; presets[idx].modRelease = guiModRelease;
    presets[idx].modAmount = guiModAmount;
    presets[idx].modEnvTarget = guiModEnvTarget;
}

void loadFromPreset(int idx) {
    if (idx < 0 || idx >= NUM_PRESETS) return;
    guiRatio1 = presets[idx].ratio1; guiRatio2 = presets[idx].ratio2;
    guiRatio3 = presets[idx].ratio3; guiRatio4 = presets[idx].ratio4;
    guiIndex1 = presets[idx].index1; guiIndex2 = presets[idx].index2;
    guiIndex3 = presets[idx].index3; guiIndex4 = presets[idx].index4;
    guiAttack = presets[idx].attack; guiDecay = presets[idx].decay;
    guiSustain = presets[idx].sustain; guiRelease = presets[idx].release;
    guiAlgorithm = presets[idx].algorithm;
    guiFilterCutoff = presets[idx].filterCutoff; guiFilterQ = presets[idx].filterQ;
    guiFilterType = presets[idx].filterType;
    guiChorus = presets[idx].chorus; guiReverb = presets[idx].reverb;
    guiLfo1Rate = presets[idx].lfo1Rate; guiLfo1Depth = presets[idx].lfo1Depth;
    guiLfo2Rate = presets[idx].lfo2Rate; guiLfo2Depth = presets[idx].lfo2Depth;
    guiLfo1Target = presets[idx].lfo1Target; guiLfo2Target = presets[idx].lfo2Target;
    guiModAttack = presets[idx].modAttack; guiModDecay = presets[idx].modDecay;
    guiModSustain = presets[idx].modSustain; guiModRelease = presets[idx].modRelease;
    guiModAmount = presets[idx].modAmount;
    guiModEnvTarget = presets[idx].modEnvTarget;
}

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

bool isNoteActive(int note) {
    for (int v = 0; v < NUM_VOICES; v++) {
        if (voices[v].note == note) return true;
    }
    return false;
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
// Main
// ============================================================================

int main() {
    srand((unsigned int)time(NULL));
    initPresets(presets);

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
    lfo1 = std::make_unique<LFO>(60.0);
    lfo2 = std::make_unique<LFO>(60.0);

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

    const int screenWidth = 650;
    const int screenHeight = 520;

    InitWindow(screenWidth, screenHeight, "FM Synth - 4 Op / 16 Voices");
    SetTargetFPS(60);

    // Mapeo de teclado
    const int whiteKeyMapping[] = { KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON };
    const int whiteKeyNotes[] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16 };
    const int numWhiteKeysMap = 10;

    const int blackKeyMapping[] = { KEY_W, KEY_E, KEY_T, KEY_Y, KEY_U, KEY_O, KEY_P };
    const int blackKeyNotes[] = { 1, 3, 6, 8, 10, 13, 15 };
    const int numBlackKeysMap = 7;

    // Constantes de layout
    const int ROW1_Y = 40;
    const int ROW_H = 120;

    while (!WindowShouldClose()) {
        // Process LFOs
        float lfo1Val = lfo1->process(guiLfo1Rate, guiLfo1Depth);
        float lfo2Val = lfo2->process(guiLfo2Rate, guiLfo2Depth);

        // Apply LFO modulation
        float modRatio1 = applyLfoMod(guiRatio1, LFO_RATIO1, guiLfo1Target, lfo1Val, 0.5f, 8.0f);
        modRatio1 = applyLfoMod(modRatio1, LFO_RATIO1, guiLfo2Target, lfo2Val, 0.5f, 8.0f);
        float modRatio2 = applyLfoMod(guiRatio2, LFO_RATIO2, guiLfo1Target, lfo1Val, 0.5f, 8.0f);
        modRatio2 = applyLfoMod(modRatio2, LFO_RATIO2, guiLfo2Target, lfo2Val, 0.5f, 8.0f);
        float modRatio3 = applyLfoMod(guiRatio3, LFO_RATIO3, guiLfo1Target, lfo1Val, 0.5f, 8.0f);
        modRatio3 = applyLfoMod(modRatio3, LFO_RATIO3, guiLfo2Target, lfo2Val, 0.5f, 8.0f);
        float modRatio4 = applyLfoMod(guiRatio4, LFO_RATIO4, guiLfo1Target, lfo1Val, 0.5f, 8.0f);
        modRatio4 = applyLfoMod(modRatio4, LFO_RATIO4, guiLfo2Target, lfo2Val, 0.5f, 8.0f);

        float modIndex1 = applyLfoMod(guiIndex1, LFO_INDEX1, guiLfo1Target, lfo1Val, 0.0f, 10.0f);
        modIndex1 = applyLfoMod(modIndex1, LFO_INDEX1, guiLfo2Target, lfo2Val, 0.0f, 10.0f);
        float modIndex2 = applyLfoMod(guiIndex2, LFO_INDEX2, guiLfo1Target, lfo1Val, 0.0f, 10.0f);
        modIndex2 = applyLfoMod(modIndex2, LFO_INDEX2, guiLfo2Target, lfo2Val, 0.0f, 10.0f);
        float modIndex3 = applyLfoMod(guiIndex3, LFO_INDEX3, guiLfo1Target, lfo1Val, 0.0f, 10.0f);
        modIndex3 = applyLfoMod(modIndex3, LFO_INDEX3, guiLfo2Target, lfo2Val, 0.0f, 10.0f);
        float modIndex4 = applyLfoMod(guiIndex4, LFO_INDEX4, guiLfo1Target, lfo1Val, 0.0f, 10.0f);
        modIndex4 = applyLfoMod(modIndex4, LFO_INDEX4, guiLfo2Target, lfo2Val, 0.0f, 10.0f);

        float modFilterCut = applyLfoMod(guiFilterCutoff, LFO_FILTER_CUT, guiLfo1Target, lfo1Val, 100.0f, 8000.0f);
        modFilterCut = applyLfoMod(modFilterCut, LFO_FILTER_CUT, guiLfo2Target, lfo2Val, 100.0f, 8000.0f);
        float modFilterQ = applyLfoMod(guiFilterQ, LFO_FILTER_Q, guiLfo1Target, lfo1Val, 0.5f, 8.0f);
        modFilterQ = applyLfoMod(modFilterQ, LFO_FILTER_Q, guiLfo2Target, lfo2Val, 0.5f, 8.0f);

        float modChorus = applyLfoMod(guiChorus, LFO_CHORUS, guiLfo1Target, lfo1Val, 0.0f, 1.0f);
        modChorus = applyLfoMod(modChorus, LFO_CHORUS, guiLfo2Target, lfo2Val, 0.0f, 1.0f);
        float modReverb = applyLfoMod(guiReverb, LFO_REVERB, guiLfo1Target, lfo1Val, 0.0f, 1.0f);
        modReverb = applyLfoMod(modReverb, LFO_REVERB, guiLfo2Target, lfo2Val, 0.0f, 1.0f);

        // Apply Mod Envelope modulation (basado en guiModAmount)
        float modEnvValue = guiModAmount;  // El amount actua como multiplicador
        if (guiModEnvTarget == MODENV_INDEX1) {
            modIndex1 = std::max(0.0f, std::min(10.0f, modIndex1 + modEnvValue * 5.0f));
        } else if (guiModEnvTarget == MODENV_INDEX2) {
            modIndex2 = std::max(0.0f, std::min(10.0f, modIndex2 + modEnvValue * 5.0f));
        } else if (guiModEnvTarget == MODENV_INDEX3) {
            modIndex3 = std::max(0.0f, std::min(10.0f, modIndex3 + modEnvValue * 5.0f));
        } else if (guiModEnvTarget == MODENV_INDEX4) {
            modIndex4 = std::max(0.0f, std::min(10.0f, modIndex4 + modEnvValue * 5.0f));
        } else if (guiModEnvTarget == MODENV_FILTER_CUT) {
            modFilterCut = std::max(100.0f, std::min(8000.0f, modFilterCut + modEnvValue * 4000.0f));
        }

        // Update voices
        for (int v = 0; v < NUM_VOICES; v++) {
            voices[v].synth->setRatio1(modRatio1);
            voices[v].synth->setRatio2(modRatio2);
            voices[v].synth->setRatio3(modRatio3);
            voices[v].synth->setRatio4(modRatio4);
            voices[v].synth->setIndex1(modIndex1);
            voices[v].synth->setIndex2(modIndex2);
            voices[v].synth->setIndex3(modIndex3);
            voices[v].synth->setIndex4(modIndex4);
            voices[v].synth->setAlgorithm(guiAlgorithm);
            voices[v].synth->setAttack(guiAttack);
            voices[v].synth->setDecay(guiDecay);
            voices[v].synth->setSustain(guiSustain);
            voices[v].synth->setRelease(guiRelease);
        }

        chorusMix.store(modChorus);
        reverbMix.store(modReverb);
        filterType.store(guiFilterType);
        if (guiFilterType == FILTER_LOWPASS) {
            filterL->setLowPass(modFilterCut, modFilterQ);
            filterR->setLowPass(modFilterCut, modFilterQ);
        } else if (guiFilterType == FILTER_HIGHPASS) {
            filterL->setHighPass(modFilterCut, modFilterQ);
            filterR->setHighPass(modFilterCut, modFilterQ);
        }

        // Keyboard input
        std::vector<int> currentKeys;
        for (int i = 0; i < numWhiteKeysMap; i++) {
            if (IsKeyDown(whiteKeyMapping[i])) {
                currentKeys.push_back(12 * currentOctave + whiteKeyNotes[i]);
            }
        }
        for (int i = 0; i < numBlackKeysMap; i++) {
            if (IsKeyDown(blackKeyMapping[i])) {
                currentKeys.push_back(12 * currentOctave + blackKeyNotes[i]);
            }
        }

        if (IsKeyPressed(KEY_Z) && currentOctave > 0) currentOctave--;
        if (IsKeyPressed(KEY_X) && currentOctave < 8) currentOctave++;

        int pianoNote = -1;

        BeginDrawing();
        ClearBackground(Color{25, 25, 35, 255});

        // ==================== HEADER ====================
        DrawText("FM SYNTH", 15, 10, 20, WHITE);
        DrawLine(15, 32, screenWidth - 15, 32, Color{50, 50, 60, 255});

        // ==================== FILA 1: OPERADORES + ENVOLVENTES ====================
        Color op1Color = Color{180, 100, 60, 255};
        Color op2Color = Color{60, 120, 180, 255};
        Color op3Color = Color{100, 180, 100, 255};
        Color op4Color = Color{180, 100, 180, 255};

        DrawOperatorPanel(15, ROW1_Y, "OP1", &guiRatio1, &guiIndex1, op1Color, true, "FB");
        DrawOperatorPanel(90, ROW1_Y, "OP2", &guiRatio2, &guiIndex2, op2Color, false, "I");
        DrawOperatorPanel(165, ROW1_Y, "OP3", &guiRatio3, &guiIndex3, op3Color, false, "I");
        DrawOperatorPanel(240, ROW1_Y, "OP4", &guiRatio4, &guiIndex4, op4Color, false, "I");

        // ADSR Panel
        DrawADSRPanel(320, ROW1_Y, &guiAttack, &guiDecay, &guiSustain, &guiRelease, "ADSR", Color{200, 180, 100, 255});

        // ==================== FILA 2: FILTER + LFO1 + LFO2 + FX + MOD ENV ====================
        int row2Y = ROW1_Y + 120;
        int panelH = 140;

        // Filter Panel (alineado con OP1)
        {
            int px = 15, py = row2Y, pw = 70;
            DrawRectangle(px, py, pw, panelH, Color{35, 35, 45, 255});
            DrawRectangleLines(px, py, pw, panelH, Color{200, 100, 150, 255});
            DrawText("FILTER", px + 14, py + 4, 10, Color{200, 100, 150, 255});

            const char* fn[] = {"OFF", "LP", "HP"};
            for (int i = 0; i < 3; i++) {
                int btnX = px + 5 + i * 21, btnY = py + 18;
                bool sel = (guiFilterType == i);
                DrawRectangle(btnX, btnY, 19, 14, sel ? Color{200, 100, 150, 255} : Color{45, 45, 55, 255});
                DrawRectangleLines(btnX, btnY, 19, 14, sel ? WHITE : DARKGRAY);
                int tw = MeasureText(fn[i], 8);
                DrawText(fn[i], btnX + (19 - tw) / 2, btnY + 3, 8, sel ? WHITE : GRAY);
                Vector2 m = GetMousePosition();
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && m.x >= btnX && m.x <= btnX + 19 && m.y >= btnY && m.y <= btnY + 14) {
                    guiFilterType = i;
                }
            }
            DrawVerticalSlider(px + 3, py + 38, 70, "Cut", &guiFilterCutoff, 100.0f, 8000.0f, Color{200, 100, 150, 255});
            DrawVerticalSlider(px + 36, py + 38, 70, "Res", &guiFilterQ, 0.5f, 8.0f, Color{200, 100, 150, 255});
        }

        // LFO 1 Panel - perillas verticales con dropdown abajo
        int lfo1PanelX = 90, lfo1PanelY = row2Y;
        {
            int pw = 70;
            Color lfo1Color = Color{100, 180, 180, 255};
            DrawRectangle(lfo1PanelX, lfo1PanelY, pw, panelH, Color{35, 35, 45, 255});
            DrawRectangleLines(lfo1PanelX, lfo1PanelY, pw, panelH, lfo1Color);
            DrawText("LFO 1", lfo1PanelX + 20, lfo1PanelY + 4, 10, lfo1Color);

            // Knobs uno encima del otro con mas espacio
            DrawKnob(lfo1PanelX + 35, lfo1PanelY + 38, 13, "Rate", &guiLfo1Rate, 0.1f, 20.0f, lfo1Color);
            DrawKnob(lfo1PanelX + 35, lfo1PanelY + 90, 13, "Depth", &guiLfo1Depth, 0.0f, 1.0f, lfo1Color);
            DrawLfoDropdown(lfo1PanelX + 8, lfo1PanelY + 120, 54, &guiLfo1Target, &lfo1DropdownOpen, "");
        }

        // LFO 2 Panel
        int lfo2PanelX = 165, lfo2PanelY = row2Y;
        {
            int pw = 70;
            Color lfo2Color = Color{180, 140, 100, 255};
            DrawRectangle(lfo2PanelX, lfo2PanelY, pw, panelH, Color{35, 35, 45, 255});
            DrawRectangleLines(lfo2PanelX, lfo2PanelY, pw, panelH, lfo2Color);
            DrawText("LFO 2", lfo2PanelX + 20, lfo2PanelY + 4, 10, lfo2Color);

            DrawKnob(lfo2PanelX + 35, lfo2PanelY + 38, 13, "Rate", &guiLfo2Rate, 0.1f, 20.0f, lfo2Color);
            DrawKnob(lfo2PanelX + 35, lfo2PanelY + 90, 13, "Depth", &guiLfo2Depth, 0.0f, 1.0f, lfo2Color);
            DrawLfoDropdown(lfo2PanelX + 8, lfo2PanelY + 120, 54, &guiLfo2Target, &lfo2DropdownOpen, "");
        }

        // FX Panel (alineado con OP4)
        {
            int px = 240, py = row2Y, pw = 70;
            DrawRectangle(px, py, pw, panelH, Color{35, 35, 45, 255});
            DrawRectangleLines(px, py, pw, panelH, Color{150, 100, 180, 255});
            DrawText("FX", px + 28, py + 4, 10, Color{150, 100, 180, 255});

            DrawVerticalSlider(px + 3, py + 20, 85, "Cho", &guiChorus, 0.0f, 1.0f, Color{100, 180, 220, 255});
            DrawVerticalSlider(px + 36, py + 20, 85, "Rev", &guiReverb, 0.0f, 1.0f, Color{220, 150, 100, 255});
        }

        // MOD ENV Panel - con grafico de envolvente
        int modEnvPanelX = 320, modEnvPanelY = row2Y;
        {
            Color modEnvColor = Color{180, 120, 180, 255};
            DrawRectangle(modEnvPanelX, modEnvPanelY, 130, panelH, Color{35, 35, 45, 255});
            DrawRectangleLines(modEnvPanelX, modEnvPanelY, 130, panelH, modEnvColor);
            DrawText("MOD ENV", modEnvPanelX + 40, modEnvPanelY + 4, 10, modEnvColor);

            DrawVerticalSlider(modEnvPanelX + 3, modEnvPanelY + 18, 50, "A", &guiModAttack, 0.001f, 2.0f, modEnvColor);
            DrawVerticalSlider(modEnvPanelX + 28, modEnvPanelY + 18, 50, "D", &guiModDecay, 0.001f, 2.0f, modEnvColor);
            DrawVerticalSlider(modEnvPanelX + 53, modEnvPanelY + 18, 50, "S", &guiModSustain, 0.0f, 1.0f, modEnvColor);
            DrawVerticalSlider(modEnvPanelX + 78, modEnvPanelY + 18, 50, "R", &guiModRelease, 0.001f, 3.0f, modEnvColor);
            DrawVerticalSlider(modEnvPanelX + 103, modEnvPanelY + 18, 50, "Amt", &guiModAmount, -1.0f, 1.0f, Color{220, 180, 100, 255});

            // Grafico de envolvente
            int graphX = modEnvPanelX + 5;
            int graphY = modEnvPanelY + 95;
            int graphW = 58;
            int graphH = 22;
            DrawRectangle(graphX, graphY, graphW, graphH, Color{25, 25, 35, 255});
            DrawRectangleLines(graphX, graphY, graphW, graphH, Color{60, 60, 80, 255});

            float totalTime = guiModAttack + guiModDecay + 0.2f + guiModRelease;
            float scale = graphW / totalTime;
            int attackEnd = graphX + (int)(guiModAttack * scale);
            int decayEnd = attackEnd + (int)(guiModDecay * scale);
            int sustainEnd = decayEnd + (int)(0.2f * scale);
            int releaseEnd = sustainEnd + (int)(guiModRelease * scale);
            if (releaseEnd > graphX + graphW) releaseEnd = graphX + graphW;
            int baseY = graphY + graphH - 2;
            int peakY = graphY + 2;
            int sustainY = graphY + graphH - 2 - (int)(guiModSustain * (graphH - 4));

            DrawLine(graphX, baseY, attackEnd, peakY, modEnvColor);
            DrawLine(attackEnd, peakY, decayEnd, sustainY, modEnvColor);
            DrawLine(decayEnd, sustainY, sustainEnd, sustainY, modEnvColor);
            DrawLine(sustainEnd, sustainY, releaseEnd, baseY, modEnvColor);

            // Target dropdown
            DrawModEnvDropdown(modEnvPanelX + 68, modEnvPanelY + 95, 57, &guiModEnvTarget, &modEnvDropdownOpen, "", modEnvColor);
        }

        // ==================== ALGORITHM + RANDOMIZE (centrado entre modulos y waveform) ====================
        int modulesEndX = 450;  // Donde terminan los modulos (MOD ENV)
        int waveformEndX = screenWidth - 15;  // Donde termina el waveform
        int algPanelW = 150;
        int rightBlockX = modulesEndX + (waveformEndX - modulesEndX - algPanelW) / 2 + 5;

        // ALGORITHM Panel (altura para alinearse con row2)
        {
            int px = rightBlockX, py = ROW1_Y, pw = 150, ph = 260;
            DrawRectangle(px, py, pw, ph, Color{30, 30, 40, 255});
            DrawRectangleLines(px, py, pw, ph, Color{80, 80, 100, 255});
            DrawText("ALGORITHM", px + 14, py + 6, 10, Color{80, 80, 100, 255});

            // 3 columnas x 2 filas
            for (int i = 0; i < ALG_COUNT; i++) {
                int col = i % 3, row = i / 3;
                int btnX = px + 5 + col * 47, btnY = py + 22 + row * 22;
                bool sel = (guiAlgorithm == i);
                DrawRectangle(btnX, btnY, 44, 18, sel ? Color{80, 80, 180, 255} : Color{45, 45, 55, 255});
                DrawRectangleLines(btnX, btnY, 44, 18, sel ? WHITE : DARKGRAY);
                int tw = MeasureText(algorithmNames[i], 8);
                DrawText(algorithmNames[i], btnX + (44 - tw) / 2, btnY + 5, 8, sel ? WHITE : GRAY);
                Vector2 m = GetMousePosition();
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && m.x >= btnX && m.x <= btnX + 44 && m.y >= btnY && m.y <= btnY + 18) {
                    guiAlgorithm = i;
                }
            }
            DrawAlgorithmDiagram(px + 35, py + 75, guiAlgorithm);

            // RANDOM Button dentro del panel
            int btnX = px + 5, btnY = py + 130, btnW = 140, btnH = 24;
            Vector2 mouse = GetMousePosition();
            bool hover = mouse.x >= btnX && mouse.x <= btnX + btnW && mouse.y >= btnY && mouse.y <= btnY + btnH;
            if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                guiRatio1 = 0.5f + (rand() % 150) / 20.0f;
                guiRatio2 = 0.5f + (rand() % 150) / 20.0f;
                guiRatio3 = 0.5f + (rand() % 150) / 20.0f;
                guiRatio4 = 0.5f + (rand() % 150) / 20.0f;
                guiIndex1 = (rand() % 50) / 10.0f;
                guiIndex2 = (rand() % 100) / 10.0f;
                guiIndex3 = (rand() % 100) / 10.0f;
                guiIndex4 = (rand() % 100) / 10.0f;
                guiAttack = 0.001f + (rand() % 500) / 1000.0f;
                guiDecay = 0.01f + (rand() % 500) / 500.0f;
                guiSustain = 0.2f + (rand() % 80) / 100.0f;
                guiRelease = 0.05f + (rand() % 200) / 100.0f;
                guiAlgorithm = rand() % ALG_COUNT;
            }
            DrawRectangle(btnX, btnY, btnW, btnH, hover ? Color{70, 130, 70, 255} : Color{50, 90, 50, 255});
            DrawRectangleLines(btnX, btnY, btnW, btnH, Color{80, 150, 80, 255});
            DrawText("RANDOMIZE", btnX + 40, btnY + 6, 10, WHITE);

            // Preset info area
            DrawText("Use Z/X for octave", px + 10, py + 165, 8, Color{70, 70, 90, 255});
            DrawText("Keys: A-; for notes", px + 10, py + 180, 8, Color{70, 70, 90, 255});
            DrawText("W,E,T,Y,U,O,P: sharps", px + 10, py + 195, 8, Color{70, 70, 90, 255});

            // Voices status in algorithm panel
            DrawText("VOICES", px + 10, py + 215, 9, Color{100, 180, 100, 255});
            for (int v = 0; v < NUM_VOICES; v++) {
                int row = v / 8, col = v % 8;
                int cx = px + 15 + col * 16;
                int cy = py + 232 + row * 14;
                bool active = voices[v].synth->isActive();
                DrawCircle(cx, cy, 4, active ? Color{100, 200, 100, 255} : Color{40, 40, 50, 255});
            }
        }

        // ==================== WAVEFORM ====================
        int waveformY = row2Y + panelH + 5;  // Despues de row2 panels
        {
            static float waveData[WAVEFORM_SIZE];
            for (int i = 0; i < WAVEFORM_SIZE; i++) {
                waveData[i] = waveformBuffer->read(i);
            }
            DrawWaveform(15, waveformY, screenWidth - 30, 30, waveData, WAVEFORM_SIZE, 0);
        }

        // ==================== ZONA INFERIOR: PRESETS + KEYBOARD + OCTAVE ====================
        int bottomY = waveformY + 37;

        // PRESETS Panel (izquierda del teclado)
        int bottomH = screenHeight - bottomY - 5;
        {
            int px = 15, py = bottomY, pw = 70, ph = bottomH;
            DrawRectangle(px, py, pw, ph, Color{35, 35, 45, 255});
            DrawRectangleLines(px, py, pw, ph, Color{100, 180, 100, 255});
            DrawText("PRESETS", px + 10, py + 4, 9, Color{100, 180, 100, 255});

            for (int i = 0; i < NUM_PRESETS; i++) {
                int btnX = px + 3, btnY = py + 16 + i * 11;
                bool sel = (i == currentPreset);
                DrawRectangle(btnX, btnY, 64, 10, sel ? Color{70, 120, 70, 255} : Color{40, 40, 50, 255});
                DrawRectangleLines(btnX, btnY, 64, 10, sel ? Color{100, 180, 100, 255} : Color{50, 50, 60, 255});
                int tw = MeasureText(presets[i].name, 8);
                DrawText(presets[i].name, btnX + (64 - tw) / 2, btnY + 1, 8, sel ? WHITE : GRAY);
                Vector2 m = GetMousePosition();
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && m.x >= btnX && m.x <= btnX + 64 && m.y >= btnY && m.y <= btnY + 10) {
                    currentPreset = i;
                    loadFromPreset(i);
                }
            }

            // SAVE/LOAD buttons
            int saveBtnY = py + ph - 14;
            {
                Vector2 m = GetMousePosition();
                bool h = m.x >= px + 3 && m.x <= px + 34 && m.y >= saveBtnY && m.y <= saveBtnY + 12;
                DrawRectangle(px + 3, saveBtnY, 31, 12, h ? Color{90, 70, 40, 255} : Color{60, 50, 35, 255});
                DrawRectangleLines(px + 3, saveBtnY, 31, 12, Color{150, 120, 60, 255});
                DrawText("SAVE", px + 6, saveBtnY + 2, 8, WHITE);
                if (h && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) saveToPreset(currentPreset);
            }
            {
                Vector2 m = GetMousePosition();
                bool h = m.x >= px + 36 && m.x <= px + 67 && m.y >= saveBtnY && m.y <= saveBtnY + 12;
                DrawRectangle(px + 36, saveBtnY, 31, 12, h ? Color{50, 80, 50, 255} : Color{40, 60, 40, 255});
                DrawRectangleLines(px + 36, saveBtnY, 31, 12, Color{80, 140, 80, 255});
                DrawText("LOAD", px + 39, saveBtnY + 2, 8, WHITE);
                if (h && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) loadFromPreset(currentPreset);
            }
        }

        // KEYBOARD - 10 teclas blancas que ocupan todo el ancho disponible
        {
            int keyboardX = 90;
            int keyboardY = bottomY;
            int keyboardEndX = screenWidth - 10;  // Hasta el borde derecho
            int numWhiteKeys = 10;
            int whiteW = (keyboardEndX - keyboardX) / numWhiteKeys;
            int whiteH = bottomH;
            int blackW = whiteW * 2 / 3;
            int blackH = whiteH * 3 / 5;
            int baseMidi = 12 * currentOctave;

            // Dibujar teclas blancas
            for (int i = 0; i < numWhiteKeys; i++) {
                int x = keyboardX + i * whiteW;
                int midiNote = baseMidi + whiteKeyNotes[i];
                bool pressed = isNoteActive(midiNote);

                Vector2 m = GetMousePosition();
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && m.x >= x && m.x < x + whiteW && m.y >= keyboardY && m.y < keyboardY + whiteH) {
                    bool onBlack = false;
                    int blackPos[] = {0, 1, 3, 4, 5, 7, 8};
                    for (int j = 0; j < 7; j++) {
                        int bx = keyboardX + blackPos[j] * whiteW + whiteW - blackW / 2;
                        if (m.x >= bx && m.x < bx + blackW && m.y < keyboardY + blackH) {
                            onBlack = true;
                            break;
                        }
                    }
                    if (!onBlack) {
                        pianoNote = midiNote;
                        pressed = true;
                    }
                }

                Color c = pressed ? Color{100, 100, 255, 255} : RAYWHITE;
                DrawRectangle(x, keyboardY, whiteW - 2, whiteH, c);
                DrawRectangleLines(x, keyboardY, whiteW - 2, whiteH, DARKGRAY);

                const char* keyLabels[] = {"A", "S", "D", "F", "G", "H", "J", "K", "L", ";"};
                int tw = MeasureText(keyLabels[i], 14);
                DrawText(keyLabels[i], x + (whiteW - tw) / 2 - 1, keyboardY + whiteH - 20, 14, Color{100, 100, 100, 255});
            }

            // Dibujar teclas negras (7 teclas negras para las 10 blancas)
            int blackPositions[] = {0, 1, 3, 4, 5, 7, 8};
            const char* blackLabels[] = {"W", "E", "T", "Y", "U", "O", "P"};
            for (int i = 0; i < 7; i++) {
                int x = keyboardX + blackPositions[i] * whiteW + whiteW - blackW / 2;
                int midiNote = baseMidi + blackKeyNotes[i];
                bool pressed = isNoteActive(midiNote);

                Vector2 m = GetMousePosition();
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && m.x >= x && m.x < x + blackW && m.y >= keyboardY && m.y < keyboardY + blackH) {
                    pianoNote = midiNote;
                    pressed = true;
                }

                Color c = pressed ? Color{80, 80, 200, 255} : Color{25, 25, 25, 255};
                DrawRectangle(x, keyboardY, blackW, blackH, c);
                DrawRectangleLines(x, keyboardY, blackW, blackH, Color{50, 50, 50, 255});
                int tw = MeasureText(blackLabels[i], 10);
                DrawText(blackLabels[i], x + (blackW - tw) / 2, keyboardY + blackH - 14, 10, Color{80, 80, 80, 255});
            }

            // Octave indicator on keyboard
            char octLabel[12];
            snprintf(octLabel, 12, "Oct %d", currentOctave);
            DrawRectangle(keyboardX + 2, keyboardY + 2, 45, 16, Color{40, 40, 60, 200});
            DrawText(octLabel, keyboardX + 6, keyboardY + 4, 12, Color{150, 180, 220, 255});
        }

        // Note handling
        if (pianoNote >= 0) currentKeys.push_back(pianoNote);

        for (int note : activeNotes) {
            bool still = false;
            for (int k : currentKeys) if (k == note) { still = true; break; }
            if (!still) voiceNoteOff(note);
        }

        for (int note : currentKeys) {
            bool was = false;
            for (int a : activeNotes) if (a == note) { was = true; break; }
            if (!was) voiceNoteOn(note, midiToFreq(note));
        }

        activeNotes = currentKeys;

        // Dropdown lists (al final para estar encima de todo)
        DrawLfoDropdownList(lfo1PanelX + 8, lfo1PanelY + 120, 54, &guiLfo1Target, &lfo1DropdownOpen);
        DrawLfoDropdownList(lfo2PanelX + 8, lfo2PanelY + 120, 54, &guiLfo2Target, &lfo2DropdownOpen);
        DrawModEnvDropdownList(modEnvPanelX + 68, modEnvPanelY + 95, 57, &guiModEnvTarget, &modEnvDropdownOpen, Color{180, 120, 180, 255});

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
