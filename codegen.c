#include "codegen.h"
#include "cpu.h"
#include "exec.h"
#include "util.h"

static uint8_t *tb_end;
static gcc_jit_lvalue *m_locals[64];
extern CPU cpu;

static gcc_jit_location *
gen_jit_loc(gcc_jit_context *ctxt, const char *filename, int line, int column) {
  return gcc_jit_context_new_location(ctxt, filename, line, column);
}

static gcc_jit_lvalue *gen_deref_lvalue(void *ctxt, void *loc,
                                        gcc_jit_type *ptr_type, void *src) {
  return gcc_jit_rvalue_dereference(
      gcc_jit_context_new_rvalue_from_ptr(ctxt, ptr_type, src), loc);
}

static gcc_jit_rvalue *gen_rvalue_from_ptr(void *ctxt, gcc_jit_type *num_type,
                                           void *src, rvalue_type is_imm) {
  if (is_imm) {
    return gcc_jit_context_new_rvalue_from_int(ctxt, num_type,
                                               *(uint32_t *)src);
  } else {
    return gcc_jit_lvalue_as_rvalue(src);
  }
}

static gcc_jit_rvalue *gen_deref_rvalue(void *ctxt, void *loc,
                                        gcc_jit_type *ptr_type, void *src) {
  // num type not needed for gcc_jit_lvalue_as_rvalue
  return gen_rvalue_from_ptr(ctxt, NULL,
                             gen_deref_lvalue(ctxt, loc, ptr_type, src), REG);
}

static void set_jit_options(gcc_jit_context *ctxt) {
  gcc_jit_context_set_int_option(ctxt, GCC_JIT_INT_OPTION_OPTIMIZATION_LEVEL,
                                 1);
  gcc_jit_context_set_bool_option(ctxt, GCC_JIT_BOOL_OPTION_DUMP_INITIAL_TREE,
                                  0);
  gcc_jit_context_set_bool_option(ctxt, GCC_JIT_BOOL_OPTION_DUMP_INITIAL_GIMPLE,
                                  DEF);
  gcc_jit_context_set_bool_option(ctxt, GCC_JIT_BOOL_OPTION_DUMP_GENERATED_CODE,
                                  0);
  gcc_jit_context_set_bool_option(ctxt, GCC_JIT_BOOL_OPTION_DUMP_EVERYTHING, 0);
  gcc_jit_context_set_bool_option(ctxt, GCC_JIT_BOOL_OPTION_KEEP_INTERMEDIATES,
                                  0);
}

gcc_jit_result *gen_code(uint32_t *startpc, uint32_t voffset, uint32_t size) {
  uint32_t *ram = startpc - voffset;
  clean_up(ctxt_cleanup) gcc_jit_context *ctxt = gcc_jit_context_acquire();
  set_jit_options(ctxt);

  // pc relative to block 0
  uint32_t pc;
  gcc_jit_location *fn_loc =
      gcc_jit_context_new_location(ctxt, "fake_scu.s", 0, 0);
  gcc_jit_type *int_type = gcc_jit_context_get_type(ctxt, GCC_JIT_TYPE_INT);
  gcc_jit_type *uint_type =
      gcc_jit_context_get_type(ctxt, GCC_JIT_TYPE_UNSIGNED_INT);
  gcc_jit_type *bool_type = gcc_jit_context_get_type(ctxt, GCC_JIT_TYPE_BOOL);
  gcc_jit_type *int_ptr_type =
      gcc_jit_type_get_pointer(gcc_jit_type_get_volatile(int_type));
  gcc_jit_type *bool_ptr_type = gcc_jit_type_get_pointer(bool_type);
  // params will be initialized externally
  gcc_jit_function *fn = gcc_jit_context_new_function(
      ctxt, gen_jit_loc(ctxt, "fake_scu.s", 0, 0), GCC_JIT_FUNCTION_EXPORTED,
      int_type, "_start", /* FIXME */
      0, NULL, 0);

  gcc_jit_block *initial = gcc_jit_function_new_block(fn, "initial");
  // init a 128-entry block
  int block_size;
  size = (size < 128) ? size : 128;
  cg_dprintf("block size: %d\n", size);
  int size_on_j = 1;
  while (size_on_j) {
    insn i = {.word = startpc[size - 1]};
    switch (i.imm_insn.opcode) {
    case J:
    case BRZ:
    case BRN:
      size--;
      break;
    default:
      size_on_j = 0;
      break;
    }
  }

  cg_dprintf("final block size: %d\n", size);

  clean_up(blocks_cleanup) gcc_jit_block **blocks =
      malloc(size * sizeof(uintptr_t));
  // init blocks; find registers the blocks may modify
  for (block_size = 0; block_size < size; block_size++) {
    char buf[16];
    sprintf(buf, "instr%i", block_size + 1);
    gcc_jit_block *block = gcc_jit_function_new_block(fn, buf);
    blocks[block_size] = block;
  }
  gcc_jit_block *fn_terminator = gcc_jit_function_new_block(fn, "exit");
  gcc_jit_block_end_with_return(fn_terminator, NULL,
                                gcc_jit_context_zero(ctxt, int_type));
  gcc_jit_block *fn_terminator_one = NULL;
  // jump to insn 0
  gcc_jit_block_end_with_jump(initial, NULL, blocks[0]);
  // 2nd pass: fill in instructions:
  int jmp_target_reg = 0, jmp_cond = ALWAYS;
  for (pc = 0; pc < block_size; pc++) {
    gcc_jit_location *loc = gen_jit_loc(ctxt, "fake_scu.s", pc + 1, 0);
    gcc_jit_block *block = blocks[pc];

    const insn ins = {.word = startpc[pc]};
    int opcode = ins.r_insn.opcode;
#if defined(DEBUG)
    disasm(ins);
    cg_dprintf("pc: %d\n", voffset + pc);
#endif
    gcc_jit_lvalue *rd;
    gcc_jit_rvalue *rs1;
    gcc_jit_rvalue *op2;
    int32_t imm_val;
    switch (opcode) {
    case NOP:
      cg_dprintf("gen nop\n");
      break;
    case ADDI:
    case ADD:
    case SUB:
    case NEG:
    case WAI:
    case SVPC: {
      cg_dprintf("gen add/sub (arithmetic and/or pc-relative)\n");
      rd = gen_deref_lvalue(ctxt, loc, int_ptr_type, &cpu.reg[ins.r_insn.rd]);
      switch (opcode) {
      case WAI:
      case SVPC:
        imm_val = pc + voffset;
        rs1 = gen_rvalue_from_ptr(ctxt, int_type, &imm_val, IMM);
        break;
      default:
        rs1 =
            gen_deref_rvalue(ctxt, loc, int_ptr_type, &cpu.reg[ins.r_insn.rs1]);
        break;
      }

      op2 = NULL;
      switch (opcode) {
      case NEG:
        break;
      case ADDI:
        imm_val = ins.imm_insn.imm;
        goto imm_common;
      case WAI:
        imm_val = 0;
        goto imm_common;
      case SVPC:
        imm_val = ins.svpc_insn.offset;
      imm_common:
        op2 = gen_rvalue_from_ptr(ctxt, int_type, &imm_val, IMM);
        break;
      default:
        op2 =
            gen_deref_rvalue(ctxt, loc, int_ptr_type, &cpu.reg[ins.r_insn.rs2]);
        break;
      }
      int op = opcode == SUB || opcode == NEG;
      gcc_jit_rvalue *result =
          (opcode == NEG) ? gcc_jit_context_new_binary_op(
                                ctxt, loc, op, int_type,
                                gcc_jit_context_zero(ctxt, int_type), rs1)
                          : gcc_jit_context_new_binary_op(ctxt, loc, op,
                                                          int_type, rs1, op2);
      gcc_jit_block_add_assignment(block, loc, rd, result);

      gcc_jit_rvalue *zflag = gcc_jit_context_new_comparison(
          ctxt, loc, GCC_JIT_COMPARISON_EQ,
          gcc_jit_context_zero(ctxt, int_type), gcc_jit_lvalue_as_rvalue(rd));
      gcc_jit_rvalue *nflag = gcc_jit_context_new_binary_op(
          ctxt, loc, GCC_JIT_BINARY_OP_RSHIFT, uint_type,
          gcc_jit_context_new_cast(ctxt, loc, gcc_jit_lvalue_as_rvalue(rd),
                                   uint_type),
          gcc_jit_context_new_rvalue_from_int(ctxt, uint_type, 31));
      // update zflag
      gcc_jit_block_add_assignment(
          block, loc, gen_deref_lvalue(ctxt, loc, int_ptr_type, &cpu.apsr[0]),
          gcc_jit_context_new_cast(ctxt, loc, zflag, int_type));
      // update nflag
      gcc_jit_block_add_assignment(
          block, loc, gen_deref_lvalue(ctxt, loc, int_ptr_type, &cpu.apsr[1]),
          gcc_jit_context_new_cast(ctxt, loc, nflag, int_type));
    } break;
    case J:
    case BRN:
    case BRZ: {
      cg_dprintf("branch: saving pc and apsr...\n");
      jmp_target_reg = ins.r_insn.rs1;
      gcc_jit_block_add_assignment(
          block, loc,
          gen_deref_lvalue(ctxt, loc, bool_ptr_type, &cpu.in_delay_slot),
          gcc_jit_context_one(ctxt, bool_type));
      switch (opcode) {
      case J:
        cg_dprintf("ALWAYS\n");
        jmp_cond = ALWAYS;
        break;
      case BRZ:
        cg_dprintf("ZERO\n");
        jmp_cond = ZERO;
        break;
      case BRN:
        cg_dprintf("NEGATIVE\n");
        jmp_cond = NEGATIVE;
        break;
      }

      // save potential jump target to cpu.pc_next
      rd = gen_deref_lvalue(ctxt, loc, int_ptr_type, &cpu.pc_next);
      rs1 = gen_deref_rvalue(ctxt, loc, int_ptr_type, &cpu.reg[ins.r_insn.rs1]);
      gcc_jit_block_add_assignment(block, loc, rd, rs1);

      // save zflag and nflag to apsr_ds (delay slot)
      for (int i = 0; i < 2; i++) {
        rd = gen_deref_lvalue(ctxt, loc, int_ptr_type, &cpu.apsr_ds[i]);
        rs1 = gen_deref_rvalue(ctxt, loc, int_ptr_type, &cpu.apsr[i]);
        gcc_jit_block_add_assignment(block, loc, rd, rs1);
      }

      gcc_jit_block_end_with_jump(block, loc, blocks[pc + 1]);
      goto loop_end; // avoid the regular pc update logic
    } break;
    case LD:
    case ST: {
      // note: rd will be the eventual lvalue, and rs1 will be the eventual
      // rvalue, regardless of what they initially meant in the context of SCU's
      // assembly language.
      if (opcode == LD) {
        rd = gen_deref_lvalue(ctxt, loc, int_ptr_type, &cpu.reg[ins.r_insn.rd]);
        // reg rs1; not queried from mem yet
        rs1 =
            gen_deref_rvalue(ctxt, loc, int_ptr_type, &cpu.reg[ins.r_insn.rs1]);
        // build memory access
        gcc_jit_lvalue *memaccess = gcc_jit_context_new_array_access(
            ctxt, loc,
            gcc_jit_context_new_rvalue_from_ptr(ctxt, int_ptr_type, ram), rs1);
        rs1 = gcc_jit_lvalue_as_rvalue(memaccess);
      } else {
        rs1 = gcc_jit_lvalue_as_rvalue(gen_deref_lvalue(
            ctxt, loc, int_ptr_type, &cpu.reg[ins.r_insn.rs1]));
        rd = gcc_jit_context_new_array_access(
            ctxt, loc,
            gcc_jit_context_new_rvalue_from_ptr(ctxt, int_ptr_type, ram), rs1);
        rs1 =
            gen_deref_rvalue(ctxt, loc, int_ptr_type, &cpu.reg[ins.r_insn.rs2]);
      }
      gcc_jit_block_add_assignment(block, loc, rd, rs1);
    } break;
    }

    uint32_t next_pc = pc + 1;
    // prepare to write into cpu.pc
    rd = gen_deref_lvalue(ctxt, loc, int_ptr_type, &cpu.pc);
    gcc_jit_rvalue *in_delay_slot =
        gen_deref_rvalue(ctxt, loc, bool_ptr_type, &cpu.in_delay_slot);

    gcc_jit_block_add_assignment(
        block, loc, rd,
        gcc_jit_context_new_rvalue_from_int(ctxt, int_type, pc + voffset));

    // delay slot handling
    char in_ds_name[16];
    snprintf(in_ds_name, 15, "ds%d", pc);
    gcc_jit_block *ds_entry = gcc_jit_function_new_block(fn, in_ds_name);
    gcc_jit_block_add_assignment(
        ds_entry, loc,
        gen_deref_lvalue(ctxt, loc, bool_ptr_type, &cpu.in_delay_slot),
        gcc_jit_context_zero(ctxt, bool_type));

    switch (jmp_cond) {
    case ALWAYS:
      rs1 = gcc_jit_context_one(ctxt, int_type);
      break;
    case ZERO:
      rs1 = gen_deref_rvalue(ctxt, loc, int_ptr_type, &cpu.apsr_ds[0]);
      break;
    case NEGATIVE:
      rs1 = gen_deref_rvalue(ctxt, loc, int_ptr_type, &cpu.apsr_ds[1]);
      break;
    }
    gcc_jit_rvalue *bool_flag =
        gcc_jit_context_new_cast(ctxt, loc, rs1, bool_type);
    gcc_jit_block *b_false;
    gcc_jit_block *b_true;

    // check if jump target has changed during runtime
    gcc_jit_block *b_verify_target;
    char name_verify[30];
    snprintf(name_verify, 29, "verify_target%d", pc);
    b_verify_target = gcc_jit_function_new_block(fn, name_verify);

    char b_false_name[16];
    snprintf(b_false_name, 15, "false%d", pc);
    b_false = gcc_jit_function_new_block(fn, b_false_name);
    // pc = pc_next; exit 1
    gcc_jit_block_add_assignment(b_false, NULL, rd,
                                 gcc_jit_lvalue_as_rvalue(gen_deref_lvalue(
                                     ctxt, NULL, int_ptr_type, &cpu.pc_next)));
    if (!fn_terminator_one) {
      fn_terminator_one = gcc_jit_function_new_block(fn, "exit_recompile");
      gcc_jit_block_end_with_return(fn_terminator_one, NULL,
                                    gcc_jit_context_one(ctxt, int_type));
    }
    gcc_jit_block_end_with_jump(b_false, loc, fn_terminator_one);

    // fast jump target calculation
    // if outside range then save pc and return 0
    // else jump to target block
    if (cpu.reg[jmp_target_reg] < voffset ||
        (cpu.reg[jmp_target_reg] - voffset) >= size) {
      cg_dprintf("jump target outside this tb: x%d\n", cpu.reg[jmp_target_reg]);
      char name[16];
      snprintf(name, 15, "reloc%d", pc);
      b_true = gcc_jit_function_new_block(fn, name);
      // pc = pc_next; exit 0
      gcc_jit_block_add_assignment(
          b_true, NULL, rd,
          gen_deref_rvalue(ctxt, NULL, int_ptr_type, &cpu.pc_next));
      gcc_jit_block_end_with_jump(b_true, NULL, fn_terminator);
    } else {
      b_true = blocks[cpu.reg[jmp_target_reg] - voffset];
    }

    // if the jump target is unmodified, we jump to b_true and ask for no
    // recompilation
    // if the jump target is dirty, we ask for recompilation
    // (return 1)
    gcc_jit_rvalue *jmp_tgt_unmodified = gcc_jit_context_new_comparison(
        ctxt, loc, GCC_JIT_COMPARISON_EQ,
        gen_deref_rvalue(ctxt, NULL, int_ptr_type, &cpu.pc_next),
        gcc_jit_context_new_rvalue_from_int(ctxt, int_type,
                                            cpu.reg[jmp_target_reg]));
    gcc_jit_block_end_with_conditional(b_verify_target, loc, jmp_tgt_unmodified,
                                       b_true, b_false);

    if (next_pc < block_size) {
      b_false = blocks[next_pc];
    } else {
      char b_false_name[18];
      snprintf(b_false_name, 17, "b_false%d", pc);
      b_false = gcc_jit_function_new_block(fn, b_false_name);
      // pc++
      gcc_jit_block_add_assignment(b_false, NULL, rd,
                                   gcc_jit_context_new_rvalue_from_int(
                                       ctxt, int_type, next_pc + voffset));
      gcc_jit_block_end_with_jump(b_false, NULL, fn_terminator);
    }
    b_true = b_verify_target;
    gcc_jit_block_end_with_conditional(ds_entry, loc, bool_flag, b_true,
                                       b_false);

    gcc_jit_block_end_with_conditional(block, loc, in_delay_slot, ds_entry,
                                       b_false);
  loop_end:
    (void)0;
  }

  return gcc_jit_context_compile(ctxt);
}
