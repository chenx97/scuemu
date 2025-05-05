# SCU ISA Spring 2022 Ver. Emulator

A software implementation of the SCU ISA defined in COEN 122.

SCUEMU runs the raw SCU binary in a 1GB memory space, with a pc outside the
instruction memory indicating the end of code execution. Memory access above
the 1GB boundary is not defined.

The default main.c runs your binary against an example array, and expects
x0 to contain the largest member of this array. The size of this array is
meant to be larger than the turning point where the JIT code starts to
outperform the interpreter.

## Build Dependencies

- libgccjit0 (preferably newer than 12.0)
- libglib2.0 (preferably newer than 2.74)
