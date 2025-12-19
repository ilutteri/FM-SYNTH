#pragma once
#include <cmath>

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
