#include <stdint.h>
#include "tiger_image.h"

// Dummy-interrupt, behövs för att boot.S länkar
void handle_interrupt(uint32_t mcause)
{
    (void)mcause;  // undvik unused-varning
}

#define VGA_BUFFER    ((volatile uint8_t*)0x08000000)
#define VGA_DMA_CTRL  ((volatile uint32_t*)0x04000100)
#define LEDS          ((volatile uint32_t*)0x04000000)
#define BUTTON        ((volatile uint32_t*)0x040000d0)
#define SWITCHES      ((volatile uint32_t*)0x04000010)

#define WIDTH  320
#define HEIGHT 240
#define NPIX   (WIDTH * HEIGHT)

// Zoom-nivå, heltal:
// 0 => 1x (original)
// 1 => 2x
// 2 => 4x
// 3 => 8x
#define MAX_ZOOM_LEVEL 3
static int zoom_level = 0;

// Buffertar (RGB332)
static uint8_t temp_buf[NPIX];     // temporär buffert för filter
static uint8_t img_buf[NPIX];      // nuvarande bild (det som visas)
static uint8_t original_buf[NPIX]; // original-bild (oförändrad)

// ----------------------------------------------------
// Hjälpfunktioner: RGB332 <-> komponenter
// ----------------------------------------------------
static inline void rgb332_to_components(uint8_t p, int *r, int *g, int *b)
{
    *r = (p >> 5) & 0x7;  // 3 bitar röd
    *g = (p >> 2) & 0x7;  // 3 bitar grön
    *b =  p       & 0x3;  // 2 bitar blå
}

static inline uint8_t components_to_rgb332(int r, int g, int b)
{
    if (r < 0) r = 0;
    if (r > 7) r = 7;
    if (g < 0) g = 0;
    if (g > 7) g = 7;
    if (b < 0) b = 0;
    if (b > 3) b = 3;
    return (uint8_t)((r << 5) | (g << 2) | b);
}

// ----------------------------------------------------
// Uppdatera VGA_BUFFER från img_buf (ren RGB332-färg)
// ----------------------------------------------------
void update_vga_from_image(void) {
    for (int i = 0; i < NPIX; i++) {
        VGA_BUFFER[i] = img_buf[i];
    }
}

// ----------------------------------------------------
// Återställ till originalbilden (1x zoom)
// ----------------------------------------------------
void restore_original(void) {
    for (int i = 0; i < NPIX; i++) {
        img_buf[i] = original_buf[i];
    }
    zoom_level = 0;   // vi är tillbaka på 1x
}

// ----------------------------------------------------
// Snabb heltals-zoom (1x, 2x, 4x, 8x) från original_buf
// ----------------------------------------------------
void apply_zoom(void) {
    // 0 = original
    if (zoom_level == 0) {
        restore_original();
        return;
    }

    int factor = 1 << zoom_level;  // 1,2,4,8 ...
    int cx = WIDTH  / 2;
    int cy = HEIGHT / 2;

    for (int y = 0; y < HEIGHT; y++) {
        int dy = y - cy;
        for (int x = 0; x < WIDTH; x++) {
            int dx = x - cx;

            // "backa" koordinaten till originalet
            int ox = cx + dx / factor;
            int oy = cy + dy / factor;

            if (ox < 0)      ox = 0;
            if (ox >= WIDTH) ox = WIDTH - 1;
            if (oy < 0)      oy = 0;
            if (oy >= HEIGHT)oy = HEIGHT - 1;

            img_buf[y * WIDTH + x] = original_buf[oy * WIDTH + ox];
        }
    }
}

// Zoom in: öka heltalsnivån (max MAX_ZOOM_LEVEL)
void zoom_in(void) {
    if (zoom_level < MAX_ZOOM_LEVEL) {
        zoom_level++;
        apply_zoom();
    }
}

// Zoom out: minska nivån, men aldrig längre ut än original
void zoom_out(void) {
    if (zoom_level > 0) {
        zoom_level--;
        apply_zoom();   // när zoom_level blir 0 hamnar vi i restore_original()
    }
}

// ----------------------------------------------------
// 3×3 box-blur: medelvärde per färgkanal i RGB332
// ----------------------------------------------------
void blur3x3(void) {
    for (int i = 0; i < NPIX; i++) {
        temp_buf[i] = img_buf[i];
    }

    for (int y = 1; y < HEIGHT - 1; y++) {
        for (int x = 1; x < WIDTH - 1; x++) {
            int sumR = 0, sumG = 0, sumB = 0;

            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    uint8_t p = temp_buf[(y + ky) * WIDTH + (x + kx)];
                    int r, g, b;
                    rgb332_to_components(p, &r, &g, &b);
                    sumR += r;
                    sumG += g;
                    sumB += b;
                }
            }

            // /9 med rounding för att undvika att det ser mörkare ut
            int r = (sumR + 4) / 9;
            int g = (sumG + 4) / 9;
            int b = (sumB + 2) / 9;

            img_buf[y * WIDTH + x] = components_to_rgb332(r, g, b);
        }
    }
}

// ----------------------------------------------------
// 5×5 box-blur per färgkanal
// ----------------------------------------------------
void blur5x5(void) {
    for (int i = 0; i < NPIX; i++) {
        temp_buf[i] = img_buf[i];
    }

    for (int y = 2; y < HEIGHT - 2; y++) {
        for (int x = 2; x < WIDTH - 2; x++) {
            int sumR = 0, sumG = 0, sumB = 0;

            for (int ky = -2; ky <= 2; ky++) {
                for (int kx = -2; kx <= 2; kx++) {
                    uint8_t p = temp_buf[(y + ky) * WIDTH + (x + kx)];
                    int r, g, b;
                    rgb332_to_components(p, &r, &g, &b);
                    sumR += r;
                    sumG += g;
                    sumB += b;
                }
            }

            // /25 med lite rounding
            int r = (sumR + 12) / 25;
            int g = (sumG + 12) / 25;
            int b = (sumB + 6)  / 25;

            img_buf[y * WIDTH + x] = components_to_rgb332(r, g, b);
        }
    }
}

// ----------------------------------------------------
// Invert per färgkanal
// ----------------------------------------------------
void invert_image(void) {
    for (int i = 0; i < NPIX; i++) {
        uint8_t p = img_buf[i];
        int r, g, b;
        rgb332_to_components(p, &r, &g, &b);
        r = 7 - r;
        g = 7 - g;
        b = 3 - b;
        img_buf[i] = components_to_rgb332(r, g, b);
    }
}

// ----------------------------------------------------
// Visa zoom-nivå på LED (1–4)
// ----------------------------------------------------
void show_zoom_level(void) {
    *LEDS = (zoom_level + 1) & 0xF;
}

// -------------------------------------------------------------------
int main(void) {
    // 1) Spara tigern i både original_buf och img_buf (RGB332)
    int max = NPIX;
    if (tiger_raw_len < (unsigned int)max) {
        max = tiger_raw_len;
    }

    for (int i = 0; i < max; i++) {
        original_buf[i] = tiger_raw[i];
        img_buf[i]      = tiger_raw[i];
    }
    // Om det finns plats över, fyll med svart
    for (int i = max; i < NPIX; i++) {
        original_buf[i] = 0;
        img_buf[i]      = 0;
    }

    // 2) Bygg första VGA-bilden från img_buf (färg)
    update_vga_from_image();

    // 3) Starta VGA-DMA
    VGA_DMA_CTRL[1] = (uint32_t)VGA_BUFFER;
    VGA_DMA_CTRL[0] = 1;

    uint32_t last_button = 0;

    while (1) {
        uint32_t button   = *BUTTON;
        uint32_t switches = *SWITCHES;

        uint32_t button_press = button & ~last_button;

        // LED: visa zoom-level
        show_zoom_level();

        // ---- BTN0: applicera valt filter en gång -------------------
        if (button_press & 0x1) {   // BTN0
            uint32_t mode     =  switches        & 0x3; // SW1:0 = kernelval
            uint32_t size_sel = (switches >> 2) & 0x1;  // SW2   = blur-storlek

            switch (mode) {
                case 0: // Blur (3x3 / 5x5)
                    if (size_sel == 0)
                        blur3x3();
                    else
                        blur5x5();
                    break;

                case 1: // Zoom in
                    zoom_in();
                    break;

                case 2: // Zoom out
                    zoom_out();
                    break;

                case 3: // Invert
                    invert_image();
                    break;
            }

            update_vga_from_image();
            VGA_DMA_CTRL[0] = 1; // trigga uppdatering
        }

        // ---- BTN1: extra blur --------------------------------------
        if (button_press & 0x2) {   // BTN1
            blur3x3();
            update_vga_from_image();
            VGA_DMA_CTRL[0] = 1;
        }

        last_button = button;

        // enkel delay
        for (volatile int i = 0; i < 100000; i++);
    }

    return 0;
}
