#pragma once
#include <algorithm>

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

    bool isActive() const { return state != ENV_IDLE; }
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
