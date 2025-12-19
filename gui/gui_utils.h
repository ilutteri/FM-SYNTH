#pragma once
#include "raylib.h"
#include <cmath>
#include <cstdio>

// Slider vertical
inline void DrawVerticalSlider(int x, int y, int height, const char* label, float* value, float minVal, float maxVal, Color trackColor) {
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

// Perilla/Knob
inline void DrawKnob(int cx, int cy, int radius, const char* label, float* value, float minVal, float maxVal, Color knobColor) {
    int labelWidth = MeasureText(label, 9);
    DrawText(label, cx - labelWidth / 2, cy - radius - 12, 9, WHITE);

    DrawCircle(cx, cy, radius + 2, Color{30, 30, 40, 255});
    DrawCircle(cx, cy, radius, Color{50, 50, 60, 255});
    DrawCircleLines(cx, cy, radius, knobColor);

    float normalized = (*value - minVal) / (maxVal - minVal);
    float angle = -135.0f + normalized * 270.0f;
    float radians = angle * 3.14159f / 180.0f;

    int lineLen = radius - 4;
    int endX = cx + (int)(std::sin(radians) * lineLen);
    int endY = cy - (int)(std::cos(radians) * lineLen);
    DrawLine(cx, cy, endX, endY, knobColor);
    DrawCircle(endX, endY, 3, knobColor);

    char valueText[16];
    snprintf(valueText, sizeof(valueText), "%.1f", *value);
    int valWidth = MeasureText(valueText, 8);
    DrawText(valueText, cx - valWidth / 2, cy + radius + 4, 8, GRAY);

    static float* activeKnob = nullptr;
    static float dragStartY = 0;
    static float dragStartValue = 0;

    Vector2 mouse = GetMousePosition();
    float dist = std::sqrt((mouse.x - cx) * (mouse.x - cx) + (mouse.y - cy) * (mouse.y - cy));

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && dist <= radius + 5 && activeKnob == nullptr) {
        activeKnob = value;
        dragStartY = mouse.y;
        dragStartValue = *value;
    }

    if (activeKnob == value && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float delta = (dragStartY - mouse.y) / 100.0f;
        float range = maxVal - minVal;
        *value = dragStartValue + delta * range;
        *value = fmaxf(minVal, fminf(maxVal, *value));
    }

    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        activeKnob = nullptr;
    }
}

// Panel de operador FM
inline void DrawOperatorPanel(int x, int y, const char* name, float* ratio, float* index,
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

// Panel ADSR
inline void DrawADSRPanel(int x, int y, float* a, float* d, float* s, float* r, const char* title = "ENVELOPE", Color titleColor = Color{200, 180, 100, 255}) {
    int panelWidth = 130;
    int panelHeight = 115;

    DrawRectangle(x, y, panelWidth, panelHeight, Color{35, 35, 45, 255});
    DrawRectangleLines(x, y, panelWidth, panelHeight, titleColor);

    int tw = MeasureText(title, 10);
    DrawText(title, x + (panelWidth - tw) / 2, y + 4, 10, titleColor);

    DrawVerticalSlider(x + 5, y + 20, 50, "A", a, 0.001f, 2.0f, Color{100, 200, 100, 255});
    DrawVerticalSlider(x + 35, y + 20, 50, "D", d, 0.001f, 2.0f, Color{200, 200, 100, 255});
    DrawVerticalSlider(x + 65, y + 20, 50, "S", s, 0.0f, 1.0f, Color{100, 150, 200, 255});
    DrawVerticalSlider(x + 95, y + 20, 50, "R", r, 0.001f, 3.0f, Color{200, 100, 100, 255});

    // Grafico ADSR
    int graphX = x + 5;
    int graphY = y + 85;
    int graphW = 120;
    int graphH = 25;

    DrawRectangle(graphX, graphY, graphW, graphH, Color{25, 25, 35, 255});
    DrawRectangleLines(graphX, graphY, graphW, graphH, Color{60, 60, 80, 255});

    float totalTime = *a + *d + 0.3f + *r;
    float scale = graphW / totalTime;

    int attackEnd = graphX + (int)(*a * scale);
    int decayEnd = attackEnd + (int)(*d * scale);
    int sustainEnd = decayEnd + (int)(0.3f * scale);
    int releaseEnd = sustainEnd + (int)(*r * scale);
    if (releaseEnd > graphX + graphW) releaseEnd = graphX + graphW;

    int baseY = graphY + graphH - 2;
    int peakY = graphY + 2;
    int sustainY = graphY + graphH - 2 - (int)((*s) * (graphH - 4));

    DrawLine(graphX, baseY, attackEnd, peakY, Color{100, 200, 100, 255});
    DrawLine(attackEnd, peakY, decayEnd, sustainY, Color{200, 200, 100, 255});
    DrawLine(decayEnd, sustainY, sustainEnd, sustainY, Color{100, 150, 200, 255});
    DrawLine(sustainEnd, sustainY, releaseEnd, baseY, Color{200, 100, 100, 255});
}

// Panel Mod Envelope
inline void DrawModEnvPanel(int x, int y, float* a, float* d, float* s, float* r, float* amt) {
    int panelWidth = 130;
    int panelHeight = 115;

    DrawRectangle(x, y, panelWidth, panelHeight, Color{35, 35, 45, 255});
    DrawRectangleLines(x, y, panelWidth, panelHeight, Color{180, 120, 180, 255});
    DrawText("MOD ENV", x + 40, y + 4, 10, Color{180, 120, 180, 255});

    DrawVerticalSlider(x + 3, y + 20, 45, "A", a, 0.001f, 2.0f, Color{180, 120, 180, 255});
    DrawVerticalSlider(x + 28, y + 20, 45, "D", d, 0.001f, 2.0f, Color{180, 120, 180, 255});
    DrawVerticalSlider(x + 53, y + 20, 45, "S", s, 0.0f, 1.0f, Color{180, 120, 180, 255});
    DrawVerticalSlider(x + 78, y + 20, 45, "R", r, 0.001f, 3.0f, Color{180, 120, 180, 255});
    DrawVerticalSlider(x + 103, y + 20, 45, "Amt", amt, -1.0f, 1.0f, Color{220, 180, 100, 255});

    // Grafico
    int graphX = x + 5;
    int graphY = y + 85;
    int graphW = 120;
    int graphH = 25;

    DrawRectangle(graphX, graphY, graphW, graphH, Color{25, 25, 35, 255});
    DrawRectangleLines(graphX, graphY, graphW, graphH, Color{60, 60, 80, 255});

    float totalTime = *a + *d + 0.3f + *r;
    float scale = graphW / totalTime;

    int attackEnd = graphX + (int)(*a * scale);
    int decayEnd = attackEnd + (int)(*d * scale);
    int sustainEnd = decayEnd + (int)(0.3f * scale);
    int releaseEnd = sustainEnd + (int)(*r * scale);
    if (releaseEnd > graphX + graphW) releaseEnd = graphX + graphW;

    int baseY = graphY + graphH - 2;
    int peakY = graphY + 2;
    int sustainY = graphY + graphH - 2 - (int)((*s) * (graphH - 4));

    DrawLine(graphX, baseY, attackEnd, peakY, Color{180, 120, 180, 255});
    DrawLine(attackEnd, peakY, decayEnd, sustainY, Color{180, 120, 180, 255});
    DrawLine(decayEnd, sustainY, sustainEnd, sustainY, Color{180, 120, 180, 255});
    DrawLine(sustainEnd, sustainY, releaseEnd, baseY, Color{180, 120, 180, 255});
}

// Diagrama de algoritmo
inline void DrawAlgorithmDiagram(int x, int y, int algorithm) {
    int boxW = 18, boxH = 14;
    Color opColor = Color{60, 120, 180, 255};
    Color carrierColor = Color{180, 100, 60, 255};

    DrawRectangle(x - 3, y - 3, 80, 44, Color{30, 30, 40, 255});

    switch (algorithm) {
        case 0: // ALG_STACK: 4 -> 3 -> 2 -> 1
            for (int i = 0; i < 4; i++) {
                int bx = x + i * 20;
                Color c = (i == 3) ? carrierColor : opColor;
                DrawRectangle(bx, y + 12, boxW, boxH, c);
                char num[2] = {(char)('4' - i), 0};
                DrawText(num, bx + 5, y + 13, 10, WHITE);
                if (i < 3) DrawText(">", bx + 18, y + 13, 8, GRAY);
            }
            break;

        case 1: // ALG_TWIN
            DrawRectangle(x, y + 2, boxW, boxH, opColor);
            DrawText("4", x + 5, y + 3, 10, WHITE);
            DrawRectangle(x + 22, y + 2, boxW, boxH, opColor);
            DrawText("3", x + 27, y + 3, 10, WHITE);
            DrawRectangle(x + 22, y + 20, boxW, boxH, opColor);
            DrawText("2", x + 27, y + 21, 10, WHITE);
            DrawRectangle(x + 55, y + 12, boxW, boxH, carrierColor);
            DrawText("1", x + 60, y + 13, 10, WHITE);
            break;

        case 2: // ALG_BRANCH
            DrawRectangle(x, y + 12, boxW, boxH, opColor);
            DrawText("4", x + 5, y + 13, 10, WHITE);
            DrawRectangle(x + 28, y, boxW, boxH, opColor);
            DrawText("3", x + 33, y + 1, 10, WHITE);
            DrawRectangle(x + 28, y + 22, boxW, boxH, opColor);
            DrawText("2", x + 33, y + 23, 10, WHITE);
            DrawRectangle(x + 55, y + 12, boxW, boxH, carrierColor);
            DrawText("1", x + 60, y + 13, 10, WHITE);
            break;

        case 3: // ALG_PARALLEL
            for (int i = 0; i < 3; i++) {
                DrawRectangle(x, y + i * 12, boxW, boxH - 2, opColor);
                char num[2] = {(char)('4' - i), 0};
                DrawText(num, x + 5, y + i * 12, 9, WHITE);
            }
            DrawRectangle(x + 35, y + 12, boxW, boxH, carrierColor);
            DrawText("1", x + 40, y + 13, 10, WHITE);
            break;

        case 4: // ALG_DUAL_CARRIER
            DrawRectangle(x, y + 2, boxW, boxH, opColor);
            DrawText("4", x + 5, y + 3, 10, WHITE);
            DrawRectangle(x + 22, y + 2, boxW, boxH, carrierColor);
            DrawText("3", x + 27, y + 3, 10, WHITE);
            DrawRectangle(x + 44, y + 2, boxW, boxH, opColor);
            DrawText("2", x + 49, y + 3, 10, WHITE);
            DrawRectangle(x + 44, y + 20, boxW, boxH, carrierColor);
            DrawText("1", x + 49, y + 21, 10, WHITE);
            break;

        case 5: // ALG_TRIPLE
            DrawRectangle(x, y + 12, boxW, boxH, opColor);
            DrawText("4", x + 5, y + 13, 10, WHITE);
            DrawRectangle(x + 30, y, boxW, boxH, carrierColor);
            DrawText("1", x + 35, y + 1, 10, WHITE);
            DrawRectangle(x + 30, y + 14, boxW, boxH, carrierColor);
            DrawText("2", x + 35, y + 15, 10, WHITE);
            DrawRectangle(x + 30, y + 28, boxW, boxH, carrierColor);
            DrawText("3", x + 35, y + 29, 10, WHITE);
            break;
    }
}

// Waveform display
inline void DrawWaveform(int x, int y, int width, int height, float* buffer, int bufferSize, int writeIndex) {
    DrawRectangle(x, y, width, height, Color{20, 20, 30, 255});
    DrawRectangleLines(x, y, width, height, DARKGRAY);

    int centerY = y + height / 2;
    DrawLine(x, centerY, x + width, centerY, Color{60, 60, 80, 255});

    float xStep = (float)width / bufferSize;

    for (int i = 0; i < bufferSize - 1; i++) {
        int idx1 = (writeIndex + i) % bufferSize;
        int idx2 = (writeIndex + i + 1) % bufferSize;
        float sample1 = buffer[idx1];
        float sample2 = buffer[idx2];

        int y1 = centerY - (int)(sample1 * height * 0.4f);
        int y2 = centerY - (int)(sample2 * height * 0.4f);

        DrawLine(x + (int)(i * xStep), y1, x + (int)((i + 1) * xStep), y2, GREEN);
    }
}
