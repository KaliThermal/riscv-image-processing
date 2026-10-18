# RISC-V Bare-Metal Image Processing

**Interactive image processing in C on a RISC-V-based lab platform, using memory-mapped I/O, a VGA framebuffer and DMA control.**

This project was developed as a hardware-oriented laboratory exercise. It applies image filters and centered integer zoom to a 320 × 240 image stored in **RGB332 (8-bit)** format. Physical switches select an operation, push buttons trigger it, and LEDs indicate the current zoom level.

> **Hardware-specific project:** This is the original RISC-V application, **not** a desktop port or emulator demo. Its device addresses and VGA/DMA control sequence depend on the original lab platform. The original startup assembly (`platform/boot.S`) is included. The image header is generated from **any picture you choose**, while board-specific build/link support is still needed.

## Highlights

- **Bare-metal C:** direct access to peripheral registers through `volatile` pointers.
- **VGA / DMA:** writes processed pixels to a memory-mapped framebuffer and uses DMA control registers for display updates.
- **Image filters:** 3 × 3 and 5 × 5 box blur, implemented separately for red, green and blue channels.
- **Image transformations:** RGB332 color inversion and centered 1× / 2× / 4× / 8× nearest-neighbor zoom.
- **Hardware interaction:** switch-based mode selection, button-triggered operations and LED zoom-level feedback.
- **Integer arithmetic:** bit operations and integer-based image processing without floating-point calculations.

## Controls

Set the switches, then press **BTN0** to apply the selected operation:

| SW1:SW0 | SW2 | BTN0 action |
|:--:|:--:|---|
| `00` | `0` | Apply 3 × 3 box blur |
| `00` | `1` | Apply 5 × 5 box blur |
| `01` | Any | Zoom in (up to 8×) |
| `10` | Any | Zoom out (down to 1×) |
| `11` | Any | Invert RGB332 colors |

**BTN1** applies an additional 3 × 3 box blur regardless of switch settings.

The LEDs display the **zoom level encoded as 1–4**, not the literal magnification factor:

| LED value | Magnification |
|:--:|:--:|
| `1` | 1× (original) |
| `2` | 2× |
| `3` | 4× |
| `4` | 8× |

## System overview

```text
 Physical switches / buttons
            |
            v
    RISC-V bare-metal C
            |
            v
   Image-processing routine
  (blur / invert / zoom)
            |
            v
     img_buf [320 x 240]
            |
            v
   Memory-mapped VGA buffer
            |
            v
        VGA DMA
            |
            v
         Display

   RISC-V ---> LEDs (zoom indicator)
```

The program keeps three static 320 × 240 byte buffers:

| Buffer | Purpose | Size |
|---|---|---:|
| `original_buf` | Preserves the original image | 76,800 bytes |
| `img_buf` | Holds the currently processed image | 76,800 bytes |
| `temp_buf` | Snapshot used by blur filters | 76,800 bytes |

Together these arrays occupy **230,400 bytes (225 KiB)** of program data memory, excluding the separate memory-mapped VGA framebuffer and other program state.

## Memory-mapped peripherals

These addresses are **specific to the original laboratory setup** and should not be assumed to work on other RISC-V boards:

| Peripheral | Address | Access in the code |
|---|---|---|
| VGA framebuffer | `0x08000000` | Pixel writes (`uint8_t`) |
| VGA DMA control | `0x04000100` | Control register at offset `0`; buffer address at offset `4` |
| LEDs | `0x04000000` | Zoom-level output |
| Switches | `0x04000010` | Mode selection input |
| Buttons | `0x040000D0` | Button input |

The CPU copies `img_buf` to VGA memory and writes to the VGA DMA control register to start/trigger the display update, according to the lab's peripheral interface.

## Repository structure

```text
riscv-image-processing/
├── README.md
├── .gitignore
├── src/
│   └── main.c                 # Original application source
├── platform/
│   ├── boot.S                 # Original RISC-V startup/interrupt code
│   └── README.md              # Remaining platform dependencies
├── tools/
│   ├── image_to_header.py     # Convert any picture into a C header
│   └── requirements.txt      # Pillow for conversion on your PC
├── docs/
│   └── ARCHITECTURE.md        # Image-processing and I/O details
└── media/
    └── .gitkeep               # Optional real hardware photos/demo
```

## Prepare any image

**You do not need the original tiger picture.** The program requires **320 × 240
pixels in RGB332 format**, stored in a C header. The provided helper converts
PNG/JPG or another Pillow-supported image into that header on your computer.
The helper runs on your computer **only to prepare the image data**; the image
processing application itself still runs on RISC-V hardware.

### 1. Install the image conversion dependency

From the repository root:

```bash
python3 -m pip install -r tools/requirements.txt
```

### 2. Choose a photo and generate the header

For example, with a picture named `my_photo.jpg` in the repository root:

```bash
python3 tools/image_to_header.py my_photo.jpg
```

This generates **`src/tiger_image.h`** automatically. It does **not** require
editing the C application.

> The header is still called `tiger_image.h` for compatibility with the original
> lab code. **The picture can be anything**, not just a tiger. The script
> defines the original `tiger_raw` and `tiger_raw_len` variables that
> `src/main.c` expects.

### 3. What the converter does

1. Loads the picture (PNG, JPG, etc.) and handles EXIF orientation.
2. Crops it around the center to a 4:3 aspect ratio and resizes it to **320 × 240**.
3. Converts each RGB pixel to **RGB332**: 3 red bits, 3 green bits, 2 blue bits.
4. Saves the pixels as a **C array of 76,800 bytes** (`tiger_raw`) and the byte count (`tiger_raw_len`).

The header is automatically excluded by `.gitignore` so you can regenerate it
locally without committing someone else's photo data.

It will have this structure (short example, not the full image):

```c
#ifndef TIGER_IMAGE_H
#define TIGER_IMAGE_H
#include <stdint.h>
static const uint8_t tiger_raw[] = { 0xE0, 0x1C, 0x03 /* ... */ };
static const unsigned int tiger_raw_len = sizeof(tiger_raw);
#endif
```

The full generated file contains **76,800** pixel values; the 3 values above
are red, green and blue examples.

## Building / running on RISC-V hardware

The source is intended for the **original memory map and DMA/VGA peripherals**, not
for generic RISC-V hardware. To build and run it:

1. Generate `src/tiger_image.h` from any image as described above.
2. Include both `src/main.c` and `platform/boot.S` in your original lab build.
3. Restore the **lab-specific linker script / support code** and use the
   RISC-V cross-compiler and build instructions provided for the platform.
4. Load the result on the **compatible RISC-V hardware** (or an exact lab model
   of its memory-mapped peripherals).

**Important:** `boot.S` references `handle_exception`, `_stack_end` and
`__global_pointer`; these are **not fully supplied** by this repository.
It also uses `ecall` during startup, which depends on the lab environment.
See [Platform dependencies](platform/README.md). There is no generic
one-command build or guarantee it runs on an arbitrary RISC-V board.

**Status:** Portfolio archive with original C source, provided startup assembly,
and a reproducible image-header generator. Full execution still requires the
original platform-specific build/runtime environment.

## Implementation notes and limitations

- **Box blur** averages each RGB332 channel across a 3 × 3 or 5 × 5 neighborhood. Border pixels are left unchanged.
- **Zoom** maps destination pixels to the *original image* using integer coordinate division. Applying zoom therefore discards prior blur/inversion effects; zooming back to 1× restores the original image.
- **Blur/inversion** modify the current image, so repeated presses can accumulate effects (two inversions restore the previous colors).
- **Controls** are read in a polling loop with software delay and rising-edge detection; this is not a general-purpose hardware debouncing implementation.
- **RGB332** prioritizes low memory usage over high color depth; each pixel is stored in one byte.

For details about the processing pipeline, see [Architecture](docs/ARCHITECTURE.md).

## Portfolio context

This project demonstrates fundamentals of **computer architecture, low-level C, peripheral interaction, framebuffer-based graphics, bit manipulation and embedded image processing** on a RISC-V platform.

> Note: If this project originated in a university lab, review your course's publication rules before uploading the lab-provided image, startup files, or other third-party assets.
