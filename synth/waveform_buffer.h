#pragma once
#include <vector>
#include <atomic>

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
