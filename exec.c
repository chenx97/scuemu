#include "cpu.h"
#include <signal.h>

#define def(x) void x(CPU *cpu, insn *insn, uint32_t *mem)
#define exec(x) x(cpu, &insn, mem)
#define arith(x) updapsr(cpu->apsr, x)

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
} pc_offset;

typedef union {
  r r_insn;
  i imm_insn;
  pc_offset svpc_insn;
} insn;

uint32_t updapsr(uint16_t *apsr, uint32_t value) {
  apsr[0] = value == 0;
  apsr[1] = value >> 31;
  return value;
}

def(svpc) {
  uint32_t *reg = cpu->reg;
  pc_offset *decoded = &insn->svpc_insn;
  reg[decoded->rd] = cpu->pc + decoded->offset;
  cpu->pc++;
}

def(ld) {
  r *decoded = &insn->r_insn;
  uint32_t *reg = cpu->reg;
  reg[decoded->rd] = mem[reg[decoded->rs1]];
  cpu->pc++;
}

def(st) {
  r *decoded = &insn->r_insn;
  uint32_t *reg = cpu->reg;
  mem[reg[decoded->rs1]] = reg[decoded->rs2];
  cpu->pc++;
}

def(add) {
  uint32_t *reg = cpu->reg;
  r *decoded = &insn->r_insn;
  reg[decoded->rd] = arith(reg[decoded->rs1] + reg[decoded->rs2]);
  cpu->pc++;
}

def(addi) {
  uint32_t *reg = cpu->reg;
  i *decoded = &insn->imm_insn;
  reg[decoded->rd] = arith(reg[decoded->rs1] + decoded->imm);
  cpu->pc++;
}

def(neg) {
  uint32_t *reg = cpu->reg;
  r *decoded = &insn->r_insn;
  reg[decoded->rd] = arith(-reg[decoded->rs1]);
  cpu->pc++;
}

def(sub) {
  uint32_t *reg = cpu->reg;
  r *decoded = &insn->r_insn;
  reg[decoded->rd] = arith(reg[decoded->rs1] - reg[decoded->rs2]);
  cpu->pc++;
}

def(jmp) {
  r *decoded = &insn->r_insn;
  cpu->pc_next = cpu->reg[decoded->rs1];
  cpu->in_delay_slot = 1;
  cpu->pc++;
}

def(brz) {
  if (cpu->apsr[0]) {
    jmp(cpu, insn, mem);
  } else {
    cpu->pc++;
  }
}

def(wai) {
  pc_offset *decoded = &insn->svpc_insn;
  cpu->reg[decoded->rd] = cpu->pc;
  cpu->pc++;
}

def(brn) {
  if (cpu->apsr[1]) {
    jmp(cpu, insn, mem);
  } else {
    cpu->pc++;
  }
}

void execute(CPU *cpu, uint32_t *mem) {
  uint32_t word = mem[cpu->pc];
  insn insn;
  ((uint32_t *)&insn)[0] = word;
  switch (insn.r_insn.opcode) {
  case 0b0000:
    cpu->pc++;
    arith(0);
    break;
  case 0b0001:
    raise(SIGILL);
    break;
  case 0b0011:
    exec(st);
    break;
  case 0b0010:
    raise(SIGILL);
    break;
  case 0b0110:
    exec(neg);
    break;
  case 0b0111:
    exec(sub);
    break;
  case 0b0101:
    exec(addi);
    break;
  case 0b0100:
    exec(add);
    break;
  case 0b1100:
    raise(SIGILL);
    break;
  case 0b1101:
    raise(SIGILL);
    break;
  case 0b1111:
    exec(svpc);
    break;
  case 0b1110:
    exec(ld);
    break;
  case 0b1010:
    exec(wai);
    break;
  case 0b1011:
    exec(brn);
    break;
  case 0b1001:
    exec(brz);
    break;
  case 0b1000:
    exec(jmp);
    break;
  }
}
