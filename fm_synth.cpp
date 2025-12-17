#include <iostream>
#include <cmath>
#include <vector>
#include <memory>
#include <rtaudio/RtAudio.h>

// Constantes
const double PI = 3.14159265358979323846;
const double SAMPLE_RATE = 44100.0;

/**
 * Clase que representa un oscilador sinusoidal simple
 */
class Oscillator {
private:
    double phase;           // Fase actual del oscilador (0 a 2π)
    double phaseIncrement;  // Cuánto avanza la fase por cada sample
    double frequency;       // Frecuencia en Hz
    double sampleRate;      // Sample rate del sistema
    
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
    
    // Genera el próximo sample del oscilador
    // modulation: valor que modula la fase (para FM)
    double process(double modulation = 0.0) {
        double output = std::sin(phase + modulation);
        
        // Avanzar la fase
        phase += phaseIncrement;
        
        // Mantener la fase en el rango [0, 2π]
        if (phase >= 2.0 * PI) {
            phase -= 2.0 * PI;
        }
        
        return output;
    }
    
    void reset() {
        phase = 0.0;
    }
    
private:
    void updatePhaseIncrement() {
        // phaseIncrement = 2π * frequency / sampleRate
        phaseIncrement = 2.0 * PI * frequency / sampleRate;
    }
};

/**
 * Sintetizador FM de 2 operadores
 * Operador 1 (Carrier): genera la frecuencia fundamental que escuchamos
 * Operador 2 (Modulator): modula la frecuencia del carrier
 */
class FMSynth {
private:
    Oscillator carrier;     // Oscilador portador
    Oscillator modulator;   // Oscilador modulador
    
    double modulationIndex; // Índice de modulación (intensidad del efecto FM)
    double amplitude;       // Amplitud de salida
    bool isActive;          // Si el synth está sonando
    
public:
    FMSynth(double freq, double modRatio, double modIndex, double sr)
        : carrier(freq, sr),
          modulator(freq * modRatio, sr),
          modulationIndex(modIndex),
          amplitude(0.3),
          isActive(false) {
    }
    
    // Genera el próximo sample de audio
    double process() {
        if (!isActive) return 0.0;
        
        // Paso 1: Generar la señal moduladora
        double modulatorOutput = modulator.process();
        
        // Paso 2: Calcular la modulación de fase
        // La cantidad de modulación depende del índice y la frecuencia del modulador
        double phaseModulation = modulationIndex * modulatorOutput;
        
        // Paso 3: Generar la señal del carrier con la fase modulada
        double carrierOutput = carrier.process(phaseModulation);
        
        // Paso 4: Aplicar amplitud
        return carrierOutput * amplitude;
    }
    
    void noteOn(double freq, double modRatio = 2.0) {
        carrier.setFrequency(freq);
        modulator.setFrequency(freq * modRatio);
        carrier.reset();
        modulator.reset();
        isActive = true;
    }
    
    void noteOff() {
        isActive = false;
    }
    
    void setModulationIndex(double index) {
        modulationIndex = index;
    }
    
    void setAmplitude(double amp) {
        amplitude = amp;
    }
    
    bool getIsActive() const { return isActive; }
};

// Variable global para el sintetizador
std::unique_ptr<FMSynth> synth;

/**
 * Callback de audio - RtAudio llama a esta función para llenar el buffer
 */
int audioCallback(void *outputBuffer, void *inputBuffer, unsigned int nFrames,
                  double streamTime, RtAudioStreamStatus status, void *userData) {
    
    double *buffer = (double *)outputBuffer;
    
    if (status) {
        std::cout << "Stream underflow detected!" << std::endl;
    }
    
    // Generar audio para cada frame (sample)
    for (unsigned int i = 0; i < nFrames; i++) {
        double sample = synth->process();
        
        // Stereo: mismo sample en ambos canales
        *buffer++ = sample;  // Canal izquierdo
        *buffer++ = sample;  // Canal derecho
    }
    
    return 0;
}

/**
 * Función para convertir nota MIDI a frecuencia en Hz
 */
double midiToFreq(int midiNote) {
    // A4 (440 Hz) es la nota MIDI 69
    return 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
}

int main() {
    std::cout << "=== Sintetizador FM en C++ ===" << std::endl;
    std::cout << "Usa este sintetizador para explorar la síntesis FM" << std::endl;
    std::cout << std::endl;
    
    // Crear el sintetizador
    synth = std::make_unique<FMSynth>(440.0, 2.0, 2.0, SAMPLE_RATE);
    
    // Configurar RtAudio
    RtAudio dac;
    
    if (dac.getDeviceCount() < 1) {
        std::cout << "No se encontraron dispositivos de audio!" << std::endl;
        return 1;
    }
    
    // Parámetros del stream de audio
    RtAudio::StreamParameters parameters;
    parameters.deviceId = dac.getDefaultOutputDevice();
    parameters.nChannels = 2;  // Stereo
    parameters.firstChannel = 0;
    
    unsigned int bufferFrames = 256;  // Buffer size
    
    RtAudioErrorType result = dac.openStream(&parameters, NULL, RTAUDIO_FLOAT64,
                  (unsigned int)SAMPLE_RATE, &bufferFrames,
                  &audioCallback, nullptr);
    
    if (result != RTAUDIO_NO_ERROR) {
        std::cout << "Error al abrir stream de audio" << std::endl;
        return 1;
    }
    
    result = dac.startStream();
    if (result != RTAUDIO_NO_ERROR) {
        std::cout << "Error al iniciar stream de audio" << std::endl;
        return 1;
    }
    
    std::cout << "Audio inicializado. Comandos:" << std::endl;
    std::cout << "  n <nota> <ratio> <index> - Tocar nota (ej: n 60 2.0 3.0)" << std::endl;
    std::cout << "  o - Note off (apagar nota)" << std::endl;
    std::cout << "  i <valor> - Cambiar índice de modulación" << std::endl;
    std::cout << "  q - Salir" << std::endl;
    std::cout << std::endl;
    std::cout << "Ejemplos interesantes:" << std::endl;
    std::cout << "  n 60 1.0 2.0  - Sonido de campana" << std::endl;
    std::cout << "  n 48 2.0 5.0  - Sonido de bajo FM" << std::endl;
    std::cout << "  n 72 3.5 8.0  - Sonido metálico/bell" << std::endl;
    std::cout << std::endl;
    
    // Loop principal
    char command;
    while (true) {
        std::cout << "> ";
        std::cin >> command;
        
        if (command == 'q') {
            break;
        }
        else if (command == 'n') {
            int note;
            double ratio, index;
            std::cin >> note >> ratio >> index;
            
            double freq = midiToFreq(note);
            synth->setModulationIndex(index);
            synth->noteOn(freq, ratio);
            
            std::cout << "Nota: " << note << " (f=" << freq << " Hz), "
                     << "Ratio: " << ratio << ", Index: " << index << std::endl;
        }
        else if (command == 'o') {
            synth->noteOff();
            std::cout << "Note off" << std::endl;
        }
        else if (command == 'i') {
            double index;
            std::cin >> index;
            synth->setModulationIndex(index);
            std::cout << "Índice de modulación: " << index << std::endl;
        }
        else {
            std::cout << "Comando no reconocido" << std::endl;
        }
    }
    
    // Cleanup
    if (dac.isStreamRunning()) {
        dac.stopStream();
    }
    
    if (dac.isStreamOpen()) {
        dac.closeStream();
    }
    
    std::cout << "¡Chau!" << std::endl;
    return 0;
}