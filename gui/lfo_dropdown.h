#pragma once
#include "raylib.h"
#include "../synth/lfo.h"

// Dropdown para seleccion de target del LFO
inline bool DrawLfoDropdown(int x, int y, int width, int* target, bool* isOpen, const char* label) {
    Vector2 mouse = GetMousePosition();
    bool clicked = false;

    DrawText(label, x, y - 10, 8, Color{100, 100, 120, 255});

    DrawRectangle(x, y, width, 14, Color{40, 40, 50, 255});
    DrawRectangleLines(x, y, width, 14, *isOpen ? Color{100, 180, 180, 255} : Color{60, 60, 80, 255});

    const char* currentName = lfoTargetNames[*target];
    int tw = MeasureText(currentName, 8);
    DrawText(currentName, x + (width - tw) / 2, y + 3, 8, WHITE);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (mouse.x >= x && mouse.x <= x + width && mouse.y >= y && mouse.y <= y + 14) {
            *isOpen = !(*isOpen);
            clicked = true;
        }
    }

    return clicked;
}

inline void DrawLfoDropdownList(int x, int y, int width, int* target, bool* isOpen) {
    if (!*isOpen) return;

    Vector2 mouse = GetMousePosition();
    int itemH = 12;
    int listH = LFO_TARGET_COUNT * itemH;

    DrawRectangle(x, y + 14, width, listH, Color{35, 35, 45, 255});
    DrawRectangleLines(x, y + 14, width, listH, Color{100, 180, 180, 255});

    for (int i = 0; i < LFO_TARGET_COUNT; i++) {
        int itemY = y + 14 + i * itemH;
        bool hover = (mouse.x >= x && mouse.x <= x + width &&
                      mouse.y >= itemY && mouse.y <= itemY + itemH);

        if (hover) {
            DrawRectangle(x + 1, itemY, width - 2, itemH, Color{60, 100, 100, 255});
        }

        Color textColor = (i == *target) ? Color{100, 200, 200, 255} : Color{150, 150, 150, 255};
        DrawText(lfoTargetNames[i], x + 4, itemY + 2, 8, textColor);

        if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            *target = i;
            *isOpen = false;
        }
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (!(mouse.x >= x && mouse.x <= x + width &&
              mouse.y >= y && mouse.y <= y + 14 + listH)) {
            *isOpen = false;
        }
    }
}

// Dropdown para Mod Envelope target
inline bool DrawModEnvDropdown(int x, int y, int width, int* target, bool* isOpen, const char* label, Color accentColor) {
    Vector2 mouse = GetMousePosition();
    bool clicked = false;

    if (label[0] != '\0') {
        DrawText(label, x, y - 10, 8, Color{100, 100, 120, 255});
    }

    DrawRectangle(x, y, width, 14, Color{40, 40, 50, 255});
    DrawRectangleLines(x, y, width, 14, *isOpen ? accentColor : Color{60, 60, 80, 255});

    const char* currentName = modEnvTargetNames[*target];
    int tw = MeasureText(currentName, 8);
    DrawText(currentName, x + (width - tw) / 2, y + 3, 8, WHITE);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (mouse.x >= x && mouse.x <= x + width && mouse.y >= y && mouse.y <= y + 14) {
            *isOpen = !(*isOpen);
            clicked = true;
        }
    }

    return clicked;
}

inline void DrawModEnvDropdownList(int x, int y, int width, int* target, bool* isOpen, Color accentColor) {
    if (!*isOpen) return;

    Vector2 mouse = GetMousePosition();
    int itemH = 12;
    int listH = MODENV_TARGET_COUNT * itemH;

    DrawRectangle(x, y + 14, width, listH, Color{35, 35, 45, 255});
    DrawRectangleLines(x, y + 14, width, listH, accentColor);

    for (int i = 0; i < MODENV_TARGET_COUNT; i++) {
        int itemY = y + 14 + i * itemH;
        bool hover = (mouse.x >= x && mouse.x <= x + width &&
                      mouse.y >= itemY && mouse.y <= itemY + itemH);

        if (hover) {
            DrawRectangle(x + 1, itemY, width - 2, itemH, Color{80, 60, 80, 255});
        }

        Color textColor = (i == *target) ? accentColor : Color{150, 150, 150, 255};
        DrawText(modEnvTargetNames[i], x + 4, itemY + 2, 8, textColor);

        if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            *target = i;
            *isOpen = false;
        }
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (!(mouse.x >= x && mouse.x <= x + width &&
              mouse.y >= y && mouse.y <= y + 14 + listH)) {
            *isOpen = false;
        }
    }
}
