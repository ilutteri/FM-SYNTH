#pragma once
#include <atomic>
#include "oscillator.h"
#include "envelope.h"

enum FMAlgorithm {
    ALG_STACK = 0,      // 4 -> 3 -> 2 -> 1 (serie completa)
    ALG_TWIN,           // (4->3) + (2) -> 1
    ALG_BRANCH,         // 4 -> 3 -> 1, 4 -> 2 -> 1
    ALG_PARALLEL,       // (2 + 3 + 4) -> 1
    ALG_DUAL_CARRIER,   // 4 -> 3, 2 -> 1 (dos carriers)
    ALG_TRIPLE,         // 4 -> (1, 2, 3) todos carriers
    ALG_COUNT
};

inline const char* algorithmNames[] = {
    "Stack", "Twin", "Branch", "Parallel", "Dual", "Triple"
};

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
          ratio1(1.0), ratio2(2.0), ratio3(3.0), ratio4(4.0),
          index1(0.0), index2(2.0), index3(1.5), index4(1.0),
          prevSample1(0.0),
          algorithm(ALG_STACK),
          amplitude(0.3),
          noteActive(false),
          currentFrequency(freq),
          sampleRate(sr) {}

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
                out4 = op4.process();
                out3 = op3.process(idx4 * out4);
                out2 = op2.process(idx3 * out3);
                out1 = op1.process(idx2 * out2 + feedback);
                break;

            case ALG_TWIN:
                out4 = op4.process();
                out3 = op3.process(idx4 * out4);
                out2 = op2.process();
                out1 = op1.process(idx3 * out3 + idx2 * out2 + feedback);
                break;

            case ALG_BRANCH:
                out4 = op4.process();
                out3 = op3.process(idx4 * out4);
                out2 = op2.process(idx4 * out4);
                out1 = op1.process(idx3 * out3 + idx2 * out2 + feedback);
                break;

            case ALG_PARALLEL:
                out2 = op2.process();
                out3 = op3.process();
                out4 = op4.process();
                out1 = op1.process(idx2 * out2 + idx3 * out3 + idx4 * out4 + feedback);
                break;

            case ALG_DUAL_CARRIER:
                out4 = op4.process();
                out3 = op3.process(idx4 * out4);
                out2 = op2.process();
                out1 = op1.process(idx2 * out2 + feedback);
                prevSample1 = out1;
                return (out1 + out3 * 0.7) * amplitude * envLevel * 0.7;

            case ALG_TRIPLE:
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
        op1.reset(); op2.reset(); op3.reset(); op4.reset();
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
