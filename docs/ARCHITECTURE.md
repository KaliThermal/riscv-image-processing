# Architecture and Implementation

## 1. Program flow

1. Copy the generated RGB332 image data from `tiger_raw` (from any chosen photo) into `original_buf` and `img_buf` (zero-fill if the image contains fewer than 76,800 pixels).
2. Copy `img_buf` to the VGA framebuffer at `0x08000000`.
3. Configure the DMA framebuffer pointer in `VGA_DMA_CTRL[1]` and start the display interface via `VGA_DMA_CTRL[0]`.
4. Poll the button and switch registers in an infinite loop.
5. On a button rising edge, run the selected image transformation, copy the result to VGA memory and trigger another display update.
6. Write `zoom_level + 1` to the LED register.

The DMA register interpretation and absolute MMIO addresses are specific to the teaching platform.

## 2. RGB332 format

Each pixel is 8 bits wide:

```text
 bit: 7 6 5 | 4 3 2 | 1 0
       RED  | GREEN | BLUE
        3   |   3   |   2
```

- `r = (pixel >> 5) & 0x7`
- `g = (pixel >> 2) & 0x7`
- `b = pixel & 0x3`
- `pixel = (r << 5) | (g << 2) | b`

The implementation clamps each channel before packing it back into a pixel.

## 3. Blur filters

For each pixel outside the unprocessed image border, the code:

1. Copies the current image to `temp_buf`.
2. Sums red, green and blue channels of the surrounding 3 × 3 or 5 × 5 window.
3. Calculates an integer average of each channel with per-channel rounding offsets.
4. Writes the newly packed pixel into `img_buf`.

Using a separate source buffer prevents pixels updated earlier in the scan from contaminating later neighborhoods within the same filter pass.

## 4. Zoom

Zoom is centered at `(160, 120)` and uses the nearest source pixel from the **unchanged original image**:

```c
factor = 1 << zoom_level; // zoom_level 0..3
ox = cx + (x - cx) / factor;
oy = cy + (y - cy) / factor;
```

Coordinates are clamped to image bounds. No floating-point arithmetic or bilinear interpolation is used.

## 5. Color inversion

The RGB332 channel values are inverted independently:

```c
r = 7 - r;
g = 7 - g;
b = 3 - b;
```

These limits reflect the unequal channel widths in RGB332.

## 6. Button handling

The application computes newly pressed bits using:

```c
button_press = button & ~last_button;
```

- Bit `0`: action selected by switches.
- Bit `1`: extra 3 × 3 blur.

If both buttons produce rising edges in the same loop iteration, both actions run sequentially. The code uses a busy-wait delay rather than a timer or interrupt-driven event system. A `handle_interrupt` symbol exists to satisfy the original startup code's linkage, but interrupts are not used for the interactive controls.

## 7. Image data generation

`tools/image_to_header.py` crops/resizes an arbitrary picture to exactly 320 × 240
and packs each RGB pixel into one byte: `(r >> 5) << 5 | (g >> 5) << 2 | (b >> 6)`.
It creates `src/tiger_image.h` with the `tiger_raw` and `tiger_raw_len` names
required by the original application. The name is historical; no tiger picture
is required.

The `platform/boot.S` assembly provides boot and interrupt scaffolding, but
the original lab's linker symbols, `handle_exception` implementation, and
peripheral support are still required.
