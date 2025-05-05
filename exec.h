#pragma once

#include <stdint.h>
#define def(x) void x(CPU *cpu, insn *insn, uint32_t *mem)
#define exec(x) x(cpu, &insn, mem)
#define arith(x) updapsr(cpu->apsr, x)

typedef enum {
  NOP = 0,
  MAX,
  ST = 3,
  ADD,
  ADDI,
  NEG,
  SUB,
  J,
  BRZ,
  WAI,
  BRN,
  LD = 14,
  SVPC
} INSN;

typedef struct {
  uint32_t stub : 10;
  uint32_t rs2 : 6;
  uint32_t rs1 : 6;
  uint32_t rd : 6;
  uint32_t opcode : 4;
} r;

typedef struct {
  int32_t imm : 16;
  uint32_t rs1 : 6;
  uint32_t rd : 6;
  uint32_t opcode : 4;
} i;

typedef struct {
  int32_t offset : 22;
  uint32_t rd : 6;
  uint32_t opcode : 4;
} relpc22;

typedef union {
  r r_insn;
  i imm_insn;
  relpc22 svpc_insn;
  uint32_t word;
} insn;
