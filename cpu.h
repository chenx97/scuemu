#include <stdint.h>

typedef struct {
  uint32_t reg[64];
  uint32_t pc;
  uint32_t pc_next;
  uint16_t apsr[2]; // Z, N
  uint8_t in_delay_slot;
} CPU;

extern void execute(CPU *cpu, uint32_t *mem);
