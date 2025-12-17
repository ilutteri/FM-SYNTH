# Sintetizador FM en C++

Un sintetizador de síntesis por modulación de frecuencia (FM) educativo implementado en C++.

## ¿Qué es la síntesis FM?

La síntesis FM fue popularizada por Yamaha en los años 80 (DX7) y es la base de sonidos icónicos en música electrónica, jazz fusion y más.

### Concepto básico

En FM, una onda (el **modulador**) modula la **frecuencia** de otra onda (la **portadora**). Esto genera armónicos complejos que no existían en las ondas originales.

**Fórmula matemática:**
```
y(t) = A × sin(2π × fc × t + I × sin(2π × fm × t))
```

Donde:
- **fc** = frecuencia de la portadora (la nota que escuchás)
- **fm** = frecuencia del modulador
- **I** = índice de modulación (intensidad del efecto)
- **A** = amplitud

### Parámetros clave

1. **Carrier Frequency (fc)**: La frecuencia fundamental de la nota
2. **Modulator Ratio**: fm/fc - La relación entre modulador y portadora
   - Ratio = 1.0 → sonidos cálidos, similares a filtros
   - Ratio = 2.0, 3.0 → armónicos, sonidos tipo campana
   - Ratio = números no enteros → sonidos inarmónicos, metálicos
3. **Modulation Index (I)**: Controla el brillo/complejidad del timbre
   - I = 0 → solo la portadora (onda pura)
   - I = 1-3 → timbres con carácter pero controlados
   - I = 5-10 → sonidos muy brillantes y complejos

### ¿Por qué funciona?

La modulación de frecuencia crea **bandas laterales** (sidebands) según la fórmula de Bessel:
```
Frecuencias resultantes = fc ± n × fm
```

Donde n = 1, 2, 3... (dependiendo del índice de modulación)

Ejemplo: Si fc=440Hz y fm=880Hz (ratio 2.0), obtenés componentes en:
- 440 Hz (carrier)
- 440 - 880 = -440 Hz → se refleja a 440 Hz
- 440 + 880 = 1320 Hz
- 440 - 1760 = -1320 Hz → se refleja a 1320 Hz
- 440 + 1760 = 2200 Hz
- ... etc.

## Requisitos

- C++ compiler (g++, clang)
- CMake (3.10+)
- RtAudio library

### Instalación de RtAudio

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install librtaudio-dev
```

**macOS (con Homebrew):**
```bash
brew install rtaudio
```

**Arch Linux:**
```bash
sudo pacman -S rtaudio
```

**Desde source (si no está disponible en tu distro):**
```bash
git clone https://github.com/thestk/rtaudio.git
cd rtaudio
mkdir build && cd build
cmake ..
make
sudo make install
```

## Compilación

```bash
mkdir build
cd build
cmake ..
make
```

## Uso

```bash
./fm_synth
```

### Comandos

- `n <nota> <ratio> <index>` - Tocar una nota
  - `nota`: Nota MIDI (0-127, donde 60 = C4)
  - `ratio`: Relación modulador/portadora
  - `index`: Índice de modulación
- `o` - Note off (apagar nota)
- `i <valor>` - Cambiar índice de modulación
- `q` - Salir

### Ejemplos para experimentar

```
# Sonido tipo campana
n 60 1.0 2.0

# Bajo FM gordo
n 48 2.0 5.0

# Sonido metálico/bell
n 72 3.5 8.0

# Pad suave
n 60 1.5 1.0

# Rhodes electric piano
n 64 1.0 3.5

# Brass sintético
n 60 2.0 4.0
```

## Estructura del código

### Clase `Oscillator`
Genera ondas sinusoidales básicas. Mantiene:
- **phase**: Fase actual (0 a 2π)
- **phaseIncrement**: Cuánto avanza por sample
- Método `process()`: Genera el próximo sample

### Clase `FMSynth`
Implementa la síntesis FM con 2 operadores:
- **carrier**: Oscilador portador (genera la frecuencia que escuchás)
- **modulator**: Oscilador modulador (modifica al carrier)
- **modulationIndex**: Controla la intensidad de la modulación

El método `process()` ejecuta:
1. Genera sample del modulador
2. Calcula la modulación de fase
3. Genera sample del carrier con fase modulada
4. Aplica amplitud y devuelve

### Callback de audio
`audioCallback()` es llamada por RtAudio para llenar buffers de audio en tiempo real. Llama a `synth->process()` para cada sample.

## Conceptos de audio digital

### Sample Rate
44.1 kHz significa 44,100 samples por segundo. Es el estándar de CD audio.

### Buffer
Un chunk de samples que se procesa de una vez. Buffer más pequeño = menor latencia, pero más carga de CPU.

### Phase
La posición actual en el ciclo de la onda (0 a 2π). Se incrementa según:
```cpp
phaseIncrement = 2π × frequency / sampleRate
```

## Próximos pasos

### Para mejorar este synth:

1. **ADSR Envelope** - Controlar ataque, decay, sustain, release
```cpp
class ADSREnvelope {
    double attack, decay, sustain, release;
    // ... implementación
};
```

2. **Más operadores** - El DX7 tenía 6 operadores con diferentes algoritmos
```cpp
// Algoritmos clásicos:
// Carrier modulado por Mod1, Mod1 modulado por Mod2 (serie)
// Carrier modulado por Mod1 y Mod2 simultáneamente (paralelo)
```

3. **LFO (Low Frequency Oscillator)** - Vibrato, tremolo
```cpp
double lfo = lfoOsc.process() * lfoAmount;
carrier.setFrequency(baseFreq * (1.0 + lfo));
```

4. **Feedback** - El operador se modula a sí mismo
```cpp
double feedback = previousSample * feedbackAmount;
double sample = oscillator.process(feedback);
previousSample = sample;
```

5. **Polifonía** - Múltiples notas simultáneas
```cpp
std::vector<FMSynth> voices(8);  // 8 voces
```

6. **MIDI Input** - Control desde teclado MIDI
```cpp
// Usar RtMidi para recibir mensajes MIDI
```

## Recursos adicionales

### Teoría FM
- "The Theory and Technique of Electronic Music" - Miller Puckette
- "Computer Music: Synthesis, Composition, and Performance" - Dodge & Jerse
- Yamaha DX7 manual (un clásico)

### Audio programming
- "Designing Audio Effect Plugins in C++" - Will Pirkle
- "Audio Effects: Theory, Implementation and Application" - Reiss & McPherson
- "The Audio Programming Book" - Boulanger & Lazzarini

### Frameworks más avanzados
- **JUCE** - Framework profesional para VST/AU plugins
- **Pure Data** - Visual programming para audio
- **Supercollider** - Lenguaje de síntesis en tiempo real

## Ejercicios

1. **Experimenta con ratios**
   - Prueba ratios enteros (1, 2, 3, 4) vs decimales (1.5, 2.7, 3.14)
   - ¿Qué diferencias escuchás?

2. **Índice de modulación**
   - Comienza con I=0 y subí gradualmente a 10
   - Observa cómo cambia el timbre

3. **Notas diferentes**
   - Prueba el mismo ratio e index en notas graves (30-40) y agudas (80-90)
   - ¿El timbre cambia?

4. **Implementa un envelope simple**
   - Añade attack y release lineales
   - Hace que las notas suenen más naturales

5. **Visualización**
   - Exporta samples a archivo WAV
   - Analiza el espectro en software como Audacity

## Licencia

Código educativo - úsalo libremente para aprender y experimentar.

---

¡Divertite explorando la síntesis FM! 🎹🎶
