# RISC-V platform files

`boot.S` is the **original startup/interrupt assembly supplied with this project**.
It sets the stack pointer, initializes the global pointer, performs an `ecall`
for a startup message, calls `main`, and forwards exceptions/interrupts to
C symbols.

## Files still required from the original lab setup

The project is **not a universal drop-in RISC-V build**. You still need:

- A linker script / startup environment defining `_stack_end` and
  `__global_pointer` and arranging the memory layout expected by `boot.S`.
- A definition of `handle_exception` (called from `boot.S` but **not**
  present in `src/main.c`). The assembly also invokes an `ecall` to print a
  welcome message, which requires the original lab's handling convention.
- A RISC-V toolchain/build procedure and **hardware with the matching VGA,
  DMA, button, switch and LED memory map**.

`src/main.c` already defines `handle_interrupt(uint32_t mcause)` as an unused
interrupt stub; it polls the buttons instead of using button interrupts.

For preparing the required image data from **any PNG/JPG**, see
[the image-conversion instructions](../README.md#prepare-any-image).

Check your course's licensing and redistribution requirements before sharing
lab-provided startup files publicly.
