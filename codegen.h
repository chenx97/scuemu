#pragma once
#include "exec.h"
#include <libgccjit.h>
#include <stdint.h>
gcc_jit_result *gen_code(uint32_t *startpc, uint32_t voffset, uint32_t size);

typedef enum { REG, IMM } rvalue_type;

typedef enum { ALWAYS, ZERO, NEGATIVE } jmp_cond;

static void disasm(insn ins) {
  switch (ins.r_insn.opcode) {
  case NOP:
    fprintf(stdout, "nop\n");
    break;
  case 0b0001:
    fprintf(stdout, "illegal\n");
    break;
  case ST:
    fprintf(stdout, "st x%d, x%d\n", ins.r_insn.rs2, ins.r_insn.rs1);
    break;
  case 0b0010:
    fprintf(stdout, "illegal\n");
    break;
  case NEG:
    fprintf(stdout, "neg x%d, x%d\n", ins.r_insn.rd, ins.r_insn.rs1);
    break;
  case SUB:
    fprintf(stdout, "sub x%d, x%d, x%d\n", ins.r_insn.rd, ins.r_insn.rs1,
            ins.r_insn.rs2);
    break;
  case ADDI:
    fprintf(stdout, "addi x%d, x%d, %d\n", ins.imm_insn.rd, ins.imm_insn.rs1,
            ins.imm_insn.imm);
    break;
  case ADD:
    fprintf(stdout, "add x%d, x%d, x%d\n", ins.r_insn.rd, ins.r_insn.rs1,
            ins.r_insn.rs2);
    break;
  case 0b1100:
    fprintf(stdout, "illegal\n");
    break;
  case 0b1101:
    fprintf(stdout, "illegal\n");
    break;
  case SVPC:
    fprintf(stdout, "svpc x%d, %d\n", ins.svpc_insn.rd, ins.svpc_insn.offset);
    break;
  case LD:
    fprintf(stdout, "ld x%d, x%d\n", ins.r_insn.rd, ins.r_insn.rs1);
    break;
  case WAI:
    fprintf(stdout, "wai x%d\n", ins.r_insn.rd);
    break;
  case BRN:
    fprintf(stdout, "brn x%d\n", ins.r_insn.rs1);
    break;
  case BRZ:
    fprintf(stdout, "brz x%d\n", ins.r_insn.rs1);
    break;
  case J:
    fprintf(stdout, "j x%d\n", ins.r_insn.rs1);
    break;
  }
}

typedef struct {
  int pc;
  void *fn;
} tb;
