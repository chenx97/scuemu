#include "cpu.h"
#include "exec.h"
#include <signal.h>

uint32_t updapsr(uint32_t *apsr, uint32_t value) {
  apsr[0] = value == 0;
  apsr[1] = value >> 31;
  return value;
}

def(svpc) {
  uint32_t *reg = cpu->reg;
  relpc22 *decoded = &insn->svpc_insn;
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
  relpc22 *decoded = &insn->svpc_insn;
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
  insn insn = {.word = mem[cpu->pc]};
  switch (insn.r_insn.opcode) {
  case NOP:
    cpu->pc++;
    arith(0);
    break;
  case 0b0001:
    raise(SIGILL);
    break;
  case ST:
    exec(st);
    break;
  case 0b0010:
    raise(SIGILL);
    break;
  case NEG:
    exec(neg);
    break;
  case SUB:
    exec(sub);
    break;
  case ADDI:
    exec(addi);
    break;
  case ADD:
    exec(add);
    break;
  case 0b1100:
    raise(SIGILL);
    break;
  case 0b1101:
    raise(SIGILL);
    break;
  case SVPC:
    exec(svpc);
    break;
  case LD:
    exec(ld);
    break;
  case WAI:
    exec(wai);
    break;
  case BRN:
    exec(brn);
    break;
  case BRZ:
    exec(brz);
    break;
  case J:
    exec(jmp);
    break;
  }
}
