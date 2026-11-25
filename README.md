#### **[[ENGLISH]](#english)**

<a id="spanish"></a>
# cWav

`cWav` es una librería ligera y de alto rendimiento para ESP32 diseñada para cargar y decodificar archivos WAV comprimidos (tanto en formato propio `cwav` basado en curvas spline y tablas LUT de 8 bits, como en formatos estándar G.711 $\mu$-law y A-law) directamente desde LittleFS hacia RAM o PSRAM.

## ✨Características

- Decodificación ultrarrápida y de bajo consumo de memoria (8 bits por muestra en RAM).
- Soporte nativo para archivos WAV con subchunk personalizado `cwav` (descompresión ZigZag + interpolación por splines cúbicos para generar la LUT de 256 valores).
- Soporte integrado para archivos WAV estándar G.711 **A-law** (formato `6`) y **$\mu$-law** (formato `7`) con generación dinámica de tablas LUT al vuelo (sin gastar memoria flash en tablas estáticas).
- Detección y asignación automática de memoria **PSRAM** en placas ESP32-S3 cuando está disponible, con respaldo automático a RAM interna.
- Extracción automática de metadatos de samplers desde el chunk `smpl` (`rootNote`, `loopStart` y `loopEnd`).

## 📋Requisitos

- **Placa**: Placas basadas en ESP32 (probado en ESP32, ESP32-S2, ESP32-C3 y ESP32-S3 con/sin PSRAM).
- **Sistema de archivos**: `LittleFS` (incluido en el core de ESP32 para Arduino).
- **IDE de Arduino**: Recomendado 1.8.19 o Arduino IDE 2.x.

## 📦Instalación

### Opción 1: Desde el Gestor de Librerías de Arduino (Recomendado)
1. Abre el **Arduino IDE**.
2. Ve al menú **Herramientas** > **Gestionar bibliotecas...** (o presiona `Ctrl + Shift + I` / `Cmd + Shift + I`).
3. En el buscador, escribe **cWav**.
4. Selecciona la librería y haz clic en **Instalar**.

### Opción 2: Instalación Manual
1. Clona o descarga este repositorio como un archivo ZIP.
2. En el Arduino IDE, ve a **Programa** > **Incluir Librería** > **Añadir biblioteca .ZIP...** y selecciona el archivo descargado (o descomprímelo directamente en tu carpeta `sketchbook/libraries/`).

*Alternativamente, puedes extraer el contenido del archivo ZIP directamente dentro de la carpeta `libraries` de tu directorio de trabajo de Arduino.*

### 📝Código de ejemplo

```cpp
#include <Arduino.h>
#include <LittleFS.h>
#include <cWav.h>

void setup() {
  Serial.begin(115200);
  if (!LittleFS.begin()) {
    Serial.println("Error al montar LittleFS");
    return;
  }

  File wavFile = LittleFS.open("/piano_c4.wav", "r");
  cWavInfo audio = cWavLoad(wavFile);
  wavFile.close();

  if (audio.isValid) {
    Serial.printf("Muestras: %lu | SampleRate: %lu Hz | Nota raiz: %u\n",
                  audio.length, audio.sampleRate, audio.rootNote);

    // Acceso a la muestra en el índice 0 mediante la LUT de 16 bits
    int16_t primeraMuestra = audio.data16OrLut[audio.compressedData[0]];
    Serial.printf("Valor PCM 16-bit [0]: %d\n", primeraMuestra);

    // Liberar memoria cuando ya no se necesite
    free(audio.compressedData);
    free(audio.data16OrLut);
  } else {
    Serial.println("Formato no soportado o archivo inválido.");
  }
}

void loop() {}
```

## 🎯API

### 📦Estructura `cWavInfo`

Contiene toda la información y los punteros de memoria del archivo cargado:

* <b> `uint8_t* compressedData`:</b> Puntero al buffer de muestras comprimidas de 8 bits (índices de la LUT).
* <b> `int16_t* data16OrLut`:</b> Puntero a la tabla de búsqueda (LUT) de 256 elementos de 16 bits reconstruida dinámicamente.
* <b> `uint32_t length`:</b> Cantidad total de muestras del audio.
* <b> `uint32_t sampleRate`:</b> Frecuencia de muestreo en Hz (ej. `44100`).
* <b> `uint8_t rootNote`:</b> Nota raíz MIDI extraída del chunk `smpl` (por defecto `60`).
* <b> `uint32_t loopStart`:</b> Punto de inicio del loop en muestras.
* <b> `uint32_t loopEnd`:</b> Punto final del loop en muestras (igual a `length` si no hay loop definido).
* <b> `bool isValid`:</b> `true` si el archivo fue cargado y decodificado correctamente.

### 🧩Funciones

* <b> `cWavInfo cWavLoad(File& f)`:</b> Analiza las cabeceras RIFF/WAVE del archivo abierto, extrae metadatos (`fmt `, `smpl`, `cwav`), asigna memoria y reconstruye la LUT correspondiente.
* <b> `void* cWavAlloc(size_t sz)`:</b> Asigna memoria priorizando PSRAM en ESP32-S3 (si tiene bloque libre suficiente) y utilizando RAM interna como respaldo.

### 📜Notas

* El almacenamiento de muestras en memoria ocupa exactamente **1 byte por muestra** (`compressedData`) más **512 bytes fijos** para la tabla `data16OrLut` (256 valores `int16_t`), reduciendo a la mitad el consumo de RAM frente a un WAV PCM de 16 bits.
* Para reconstruir la señal de 16 bits durante la reproducción en tiempo real, basta con indexar la tabla: `int16_t sample = audio.data16OrLut[audio.compressedData[i]];`.
* Si el archivo de entrada es estéreo (2 canales), `cWavLoad` extrae automáticamente el canal izquierdo para optimizar el uso de memoria.
* Recuerda liberar tanto `compressedData` como `data16OrLut` usando `free()` cuando descargues un instrumento o muestra de memoria.

## 📌Ejemplos

En la carpeta `examples` del repositorio encontrarás ejemplos listos para compilar que muestran cómo cargar muestras comprimidas y reproducirlas en un sintetizador o reproductor I2S en ESP32.

## 🤝 Contribuciones y Apoyo
¡Las contribuciones, *issues* y sugerencias son más que bienvenidas! Si tienes ideas para mejorar la librería o encuentras algún problema🐛, no dudes en abrir un *Issue* o enviar un *Pull Request*.

Si `cWav` te ha sido de utilidad, **considera darle una ⭐ al repositorio**. Es una gran forma de apoyar el proyecto y ayuda a que otros desarrolladores de la comunidad puedan encontrarlo.

## ✍🏼Autor
Desarrollado y mantenido por **Holman Buenaventura**.

---

#### **[[SPANISH]](#spanish)**

<a id="english"></a>
# cWav

`cWav` is a lightweight, high-performance library for ESP32 designed to load and decode compressed WAV files (both the custom `cwav` spline-curve 8-bit LUT format and standard G.711 $\mu$-law and A-law formats) directly from LittleFS into RAM or PSRAM.

## ✨Features

- Ultra-fast decoding with minimal memory footprint (8 bits per sample in RAM).
- Native support for WAV files with custom `cwav` subchunk (ZigZag decompression + cubic spline interpolation to build the 256-entry LUT).
- Built-in support for standard G.711 **A-law** (format `6`) and **$\mu$-law** (format `7`) WAV files with on-the-fly dynamic LUT generation (zero flash wasted on static tables).
- Automatic **PSRAM** detection and allocation on ESP32-S3 boards when available, with automatic fallback to internal RAM.
- Automatic extraction of sampler metadata from the `smpl` chunk (`rootNote`, `loopStart`, and `loopEnd`).

## 📋Requirements

- **Board**: ESP32-based boards (tested on ESP32, ESP32-S2, ESP32-C3, and ESP32-S3 with/without PSRAM).
- **Filesystem**: `LittleFS` (included in the ESP32 Arduino core).
- **Arduino IDE**: Recommended 1.8.19 or Arduino IDE 2.x.

## 📦Installation

### Option 1: From the Arduino Library Manager (Recommended)
1. Open the **Arduino IDE**.
2. Go to **Tools** > **Manage Libraries...** (or press `Ctrl + Shift + I` / `Cmd + Shift + I`).
3. In the search bar, type **cWav**.
4. Select the library and click **Install**.

### Option 2: Manual Installation
1. Clone or download this repository as a ZIP file.
2. In the Arduino IDE, go to **Sketch** > **Include Library** > **Add .ZIP Library...** and select the downloaded file (or unzip it directly into your `sketchbook/libraries/` folder).

*Alternatively, you can extract the contents of the ZIP file directly into the `libraries` folder of your Arduino working directory.*

### 📝Sample code

```cpp
#include <Arduino.h>
#include <LittleFS.h>
#include <cWav.h>

void setup() {
  Serial.begin(115200);
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount LittleFS");
    return;
  }

  File wavFile = LittleFS.open("/piano_c4.wav", "r");
  cWavInfo audio = cWavLoad(wavFile);
  wavFile.close();

  if (audio.isValid) {
    Serial.printf("Samples: %lu | SampleRate: %lu Hz | Root note: %u\n",
                  audio.length, audio.sampleRate, audio.rootNote);

    // Access sample at index 0 through the 16-bit LUT
    int16_t firstSample = audio.data16OrLut[audio.compressedData[0]];
    Serial.printf("16-bit PCM value [0]: %d\n", firstSample);

    // Free memory when no longer needed
    free(audio.compressedData);
    free(audio.data16OrLut);
  } else {
    Serial.println("Unsupported format or invalid file.");
  }
}

void loop() {}
```

## 🎯API

### 📦`cWavInfo` Structure

Holds all metadata and memory pointers for the loaded audio file:

* <b> `uint8_t* compressedData`:</b> Pointer to the 8-bit compressed sample buffer (LUT indices).
* <b> `int16_t* data16OrLut`:</b> Pointer to the dynamically reconstructed 256-element 16-bit lookup table (LUT).
* <b> `uint32_t length`:</b> Total number of audio samples.
* <b> `uint32_t sampleRate`:</b> Sample rate in Hz (e.g., `44100`).
* <b> `uint8_t rootNote`:</b> MIDI root note extracted from the `smpl` chunk (defaults to `60`).
* <b> `uint32_t loopStart`:</b> Loop start point in samples.
* <b> `uint32_t loopEnd`:</b> Loop end point in samples (equals `length` if no loop is defined).
* <b> `bool isValid`:</b> `true` if the file was successfully loaded and decoded.

### 🧩Functions

* <b> `cWavInfo cWavLoad(File& f)`:</b> Parses RIFF/WAVE headers of the opened file, extracts metadata (`fmt `, `smpl`, `cwav`), allocates memory, and builds the corresponding LUT.
* <b> `void* cWavAlloc(size_t sz)`:</b> Allocates memory prioritizing PSRAM on ESP32-S3 (if a large enough free block is available) and falling back to internal RAM.

### 📜Notes

* Sample storage in memory takes exactly **1 byte per sample** (`compressedData`) plus **512 fixed bytes** for the `data16OrLut` table (256 `int16_t` entries), cutting RAM usage in half compared to standard 16-bit PCM WAV files.
* To reconstruct the 16-bit signal during real-time playback, simply index the table: `int16_t sample = audio.data16OrLut[audio.compressedData[i]];`.
* If the input file is stereo (2 channels), `cWavLoad` automatically extracts the left channel to optimize memory usage.
* Remember to free both `compressedData` and `data16OrLut` using `free()` when unloading a sample from memory.

## 📌Examples

In the `examples` folder of the repository, you will find ready-to-compile examples demonstrating how to load compressed samples and play them back on an ESP32 synthesizer or I2S player.

## 🤝 Contributions and Support
Contributions, issues, and suggestions are more than welcome! If you have ideas for improving the library or encounter any problems 🐛, don't hesitate to open an issue or submit a pull request.

If you found `cWav` useful, please consider giving the repository a ⭐. It's a great way to support the project and helps other developers in the community find it.

## ✍🏼Author
Developed and maintained by **Holman Buenaventura**.