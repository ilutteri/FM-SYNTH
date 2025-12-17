#!/bin/bash

echo "=== Instalador de dependencias para FM Synth ==="
echo ""

# Detectar sistema operativo
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "Sistema detectado: Linux"

    # Detectar distribución
    if [ -f /etc/debian_version ]; then
        echo "Distribución: Debian/Ubuntu"
        echo "Instalando RtAudio y raylib..."
        sudo apt-get update
        sudo apt-get install -y librtaudio-dev cmake build-essential

        # raylib puede no estar en repos oficiales, intentar instalar
        if apt-cache show libraylib-dev &> /dev/null; then
            sudo apt-get install -y libraylib-dev
        else
            echo ""
            echo "raylib no está en los repositorios. Instalando desde source..."
            git clone --depth 1 https://github.com/raysan5/raylib.git /tmp/raylib
            cd /tmp/raylib/src
            make PLATFORM=PLATFORM_DESKTOP
            sudo make install
            cd -
            rm -rf /tmp/raylib
        fi

    elif [ -f /etc/arch-release ]; then
        echo "Distribución: Arch Linux"
        echo "Instalando RtAudio y raylib..."
        sudo pacman -Sy --noconfirm rtaudio raylib cmake base-devel

    elif [ -f /etc/fedora-release ]; then
        echo "Distribución: Fedora"
        echo "Instalando RtAudio y raylib..."
        sudo dnf install -y rtaudio-devel raylib-devel cmake gcc-c++

    else
        echo "Distribución no reconocida. Instala manualmente:"
        echo "  - RtAudio"
        echo "  - raylib"
        echo "  - CMake"
        echo "  - C++ compiler"
        exit 1
    fi

elif [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Sistema detectado: macOS"

    # Verificar si Homebrew está instalado
    if ! command -v brew &> /dev/null; then
        echo "Homebrew no está instalado."
        echo "Instalá desde: https://brew.sh"
        exit 1
    fi

    echo "Instalando RtAudio, raylib y CMake con Homebrew..."
    brew install rtaudio raylib cmake

else
    echo "Sistema operativo no soportado: $OSTYPE"
    exit 1
fi

echo ""
echo "Dependencias instaladas!"
echo ""
echo "Ahora podés compilar con:"
echo "  mkdir build && cd build"
echo "  cmake .."
echo "  make"
echo ""
echo "Ejecutables:"
echo "  ./fm_synth      - Versión CLI"
echo "  ./fm_synth_gui  - Versión con interfaz gráfica"
