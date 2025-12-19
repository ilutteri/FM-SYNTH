#pragma once
#include <memory>
#include "fm_synth.h"

struct Voice {
    std::unique_ptr<FMSynth> synth;
    int note;

    Voice() : note(-1) {}
};
