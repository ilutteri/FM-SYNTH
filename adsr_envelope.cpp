/*
 * ADSR Envelope - Ejemplo de expansión para el FM Synth
 * 
 * Este archivo muestra cómo implementar un envelope ADSR
 * para controlar la amplitud del sintetizador.
 * 
 * ADSR significa:
 * - Attack: tiempo que tarda en llegar al máximo
 * - Decay: tiempo que tarda en bajar al sustain
 * - Sustain: nivel mientras mantenés la nota
 * - Release: tiempo que tarda en llegar a 0 después de soltar
 */

#include <algorithm>

enum EnvelopeState {
    IDLE,
    ATTACK,
    DECAY,
    SUSTAIN,
    RELEASE
};

class ADSREnvelope {
private:
    // Parámetros en segundos (excepto sustain que es nivel 0-1)
    double attackTime;
    double decayTime;
    double sustainLevel;
    double releaseTime;
    
    // Estado interno
    EnvelopeState state;
    double currentLevel;
    double sampleRate;
    
    // Incrementos por sample
    double attackIncrement;
    double decayIncrement;
    double releaseIncrement;
    
public:
    ADSREnvelope(double attack, double decay, double sustain, double release, double sr)
        : attackTime(attack),
          decayTime(decay),
          sustainLevel(sustain),
          releaseTime(release),
          state(IDLE),
          currentLevel(0.0),
          sampleRate(sr) {
        updateIncrements();
    }
    
    void noteOn() {
        state = ATTACK;
        // Si ya había sonido, continuamos desde el nivel actual
    }
    
    void noteOff() {
        state = RELEASE;
    }
    
    // Procesa un sample y devuelve el nivel del envelope (0.0 a 1.0)
    double process() {
        switch (state) {
            case IDLE:
                currentLevel = 0.0;
                break;
                
            case ATTACK:
                currentLevel += attackIncrement;
                if (currentLevel >= 1.0) {
                    currentLevel = 1.0;
                    state = DECAY;
                }
                break;
                
            case DECAY:
                currentLevel -= decayIncrement;
                if (currentLevel <= sustainLevel) {
                    currentLevel = sustainLevel;
                    state = SUSTAIN;
                }
                break;
                
            case SUSTAIN:
                currentLevel = sustainLevel;
                break;
                
            case RELEASE:
                currentLevel -= releaseIncrement;
                if (currentLevel <= 0.0) {
                    currentLevel = 0.0;
                    state = IDLE;
                }
                break;
        }
        
        return currentLevel;
    }
    
    bool isActive() const {
        return state != IDLE;
    }
    
    // Setters
    void setAttack(double seconds) {
        attackTime = seconds;
        updateIncrements();
    }
    
    void setDecay(double seconds) {
        decayTime = seconds;
        updateIncrements();
    }
    
    void setSustain(double level) {
        sustainLevel = std::max(0.0, std::min(1.0, level));
    }
    
    void setRelease(double seconds) {
        releaseTime = seconds;
        updateIncrements();
    }
    
private:
    void updateIncrements() {
        // Calcular cuánto debe cambiar el nivel por cada sample
        attackIncrement = attackTime > 0.0 ? 1.0 / (attackTime * sampleRate) : 1.0;
        decayIncrement = decayTime > 0.0 ? (1.0 - sustainLevel) / (decayTime * sampleRate) : 1.0;
        releaseIncrement = releaseTime > 0.0 ? sustainLevel / (releaseTime * sampleRate) : 1.0;
    }
};

/*
 * Cómo integrarlo en FMSynth:
 * 
 * 1. Añadir un miembro ADSREnvelope a la clase FMSynth:
 *    ADSREnvelope envelope;
 * 
 * 2. Inicializar en el constructor:
 *    FMSynth(...) : ..., envelope(0.01, 0.1, 0.7, 0.2, sr) {}
 * 
 * 3. Modificar noteOn() y noteOff():
 *    void noteOn(...) {
 *        // código existente
 *        envelope.noteOn();
 *    }
 *    
 *    void noteOff() {
 *        envelope.noteOff();
 *        // NO pongas isActive = false inmediatamente
 *    }
 * 
 * 4. Modificar process():
 *    double process() {
 *        if (!envelope.isActive()) return 0.0;
 *        
 *        // código FM existente...
 *        double sample = carrier.process(phaseModulation);
 *        
 *        // Aplicar envelope
 *        double envelopeLevel = envelope.process();
 *        return sample * amplitude * envelopeLevel;
 *    }
 * 
 * Valores típicos para diferentes sonidos:
 * 
 * Piano:
 *   Attack: 0.001-0.01 (muy rápido)
 *   Decay: 0.1-0.3
 *   Sustain: 0.4-0.6
 *   Release: 0.2-0.5
 * 
 * Pad:
 *   Attack: 0.5-2.0 (lento)
 *   Decay: 0.5-1.0
 *   Sustain: 0.7-0.9
 *   Release: 1.0-3.0
 * 
 * Pluck (guitarra, etc):
 *   Attack: 0.001-0.01
 *   Decay: 0.2-0.5
 *   Sustain: 0.0 (se apaga solo)
 *   Release: 0.1-0.2
 * 
 * Organ:
 *   Attack: 0.0 (instantáneo)
 *   Decay: 0.0
 *   Sustain: 1.0 (nivel máximo)
 *   Release: 0.05-0.1
 */
