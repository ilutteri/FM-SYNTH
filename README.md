# FM Synth

Sintetizador FM con interfaz gráfica, 4 operadores, 6 voces de polifonía, ADSR y efectos.

![FM Synth Screenshot](https://img.shields.io/badge/Platform-Windows%20%7C%20macOS-blue)

## Descargar

**No necesitás instalar nada.** Descargá el ejecutable listo para usar:

1. Ir a [Releases](../../releases) o [Actions](../../actions)
2. Descargar `fm-synth-windows.zip` o `fm-synth-macos.zip`
3. Descomprimir y ejecutar `fm_synth_gui`

## Características

- **4 Operadores FM** con ratio e índice de modulación ajustables
- **6 Algoritmos** de ruteo: Stack, Twin, Branch, Parallel, Dual, Triple
- **6 voces de polifonía**
- **Envelope ADSR** global con visualización gráfica
- **Filtro** LP/HP con cutoff y resonancia
- **Chorus** estilo Juno-106
- **Reverb** atmosférica
- **Visualización** de forma de onda en tiempo real
- **Piano virtual** de 2 octavas
- **Botón Randomize** para explorar sonidos

## Controles

### Teclado (estilo piano)
```
Negras:   W E   T Y U   O P
Blancas: A S D F G H J K L ;
         C D E F G A B C D E
```

### Otros controles
- `Z` / `X` - Bajar/subir octava
- `ESC` - Salir
- Mouse - Click en sliders y teclas del piano

## Algoritmos FM

```
Stack:     Op4 → Op3 → Op2 → Op1 (carrier)
Twin:      (Op4→Op3) + Op2 → Op1 (carrier)
Branch:    Op4 → Op3 → Op1, Op4 → Op2 → Op1
Parallel:  Op2 + Op3 + Op4 → Op1 (carrier)
Dual:      Op4 → Op3 (carrier), Op2 → Op1 (carrier)
Triple:    Op4 → Op1, Op2, Op3 (todos carriers)
```

## Compilar desde source

### Dependencias

**macOS:**
```bash
brew install rtaudio raylib cmake
```

**Ubuntu/Debian:**
```bash
sudo apt-get install librtaudio-dev libraylib-dev cmake build-essential
```

**Windows (MSYS2):**
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-raylib mingw-w64-x86_64-rtaudio
```

### Compilar

```bash
mkdir build && cd build
cmake ..
make
./fm_synth_gui
```

## ¿Qué es la síntesis FM?

La síntesis FM (Frequency Modulation) fue popularizada por Yamaha en los años 80 con el DX7. Una onda **moduladora** modifica la frecuencia de una onda **portadora**, generando armónicos complejos.

**Fórmula:**
```
y(t) = sin(2π × fc × t + I × sin(2π × fm × t))
```

- **Ratio** (fm/fc): Controla qué armónicos se generan
  - Enteros (1, 2, 3) → sonidos armónicos
  - Decimales → sonidos inarmónicos, metálicos
- **Index** (I): Controla el brillo/complejidad
  - 0 → onda pura
  - 1-3 → timbres con carácter
  - 5-10 → sonidos muy brillantes

## Licencia

Código educativo - usalo libremente para aprender y experimentar.
