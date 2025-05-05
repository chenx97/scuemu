# JIT Design

SCUEMU facilitates libgccjit to speed up the execution of the supplied binary,
by converting the target machine code into host machine code.

## Frontend

The input instructions need minimal processing: they are already of fixed
width and meaning, with relocations that are often dynamic and unknown at
compile time.

## Translation Block Overview

A translation block (tb) generated upon request is returned as a
`gcc_jit_result *` pointer.

Using `gcc_jit_result_get_code`, one can get the respective function pointer
representing the dynamically compiled code. This function returns 1 if it
requests a recompilation, and returns 0 otherwise.

By default, a tb translates at most 128 guest machine instructions. If the last
instruction is a jump or a conditional branch, the size keeps increasing until
it no longer is one of these jumping instructions.

### Entry Point

The first guest instruction simulated by a tb is the instruction referenced by
cpu.pc when the compiler is called.

```c
gcc_jit_result *gen_code(uint32_t *startpc, uint32_t voffset, uint32_t size);
```

### Arithmetic Instructions

Most instructions fall into this category, even though they have a variety of
operand types. PC-relative additions are also put in this category to simplify
condition code handling.

- Base idea: `rd <- operation(rs1, op2)`
  - rd: an lvalue made from dereferencing a pointer, e.g. `cpu.reg[rs1]`
  - rs1: an rvalue made from dereferencing a pointer, e.g. `cpu.reg[rs1]`,
    `*&cpu.pc`, or `gcc_jit_context_zero` in the case of `NEG`.
  - op2: an rvalue made from various sources, such as:
    - imm: `gcc_jit_context_new_rvalue_from_int`
    - pointer: similar to rs1.
  - operation: code generated using `gcc_jit_context_new_binary_op`. In this
    implemenetation, its `enum gcc_jit_binary_op` field is
    `opcode == SUB || opcode == NEG` because 0 is add and 1 is subtract.

#### Condition code updates

The rvalue to be assigned to the desired rd is processed to update the two
flags:

- z: `(int)(rvalue == 0)`
- n: `(int)((unsigned)rvalue >> 31u)`

### Load/Store Instructions

Load and store instructions reference the memory at runtime.

- load: `rd <- (rs1)mem[rs1]`
- store: `(rd)mem[rs1] <- (rs1)op2`

In the source code, the lvalue is always assigned to variable rd, while the
rvalue is always assigned to rs1, in order to share the final
`gcc_jit_block_add_assignment` call.

### Branch and Jump Instructions

To faithfully emulate what most students implemented for COEN 122, This
emulator implements delay slots. However, to simplify the control flow, the
compiler backs up jump targets and jump conditions (flags) upon encountering
these instructions, mark the current translation state as in_delay_slot, then
forcefully jump to the next guest instruction, instead of generating branching
code.

Savestate:

- cpu.pc_next: the jump target address
- cpu.apsr_ds: the condition codes to be processed in the delay-slot.

### Updating the PC register

At the end of each instruction, we need to know where we are jumping to. Here's
an overview of the process:

- cpu.pc = pc + voffset
- if (in_delay_slot)
  - generate condition rvalue
  - if true
    - if jump target register unmodified
      - if target out of tb
        - pc = pc_next
        - return 0
      - else
        - jump to target block
    - else
      - pc = pc_next
      - return 1
  - else
    - if pc + 1 out of tb
      - pc++
      - return 0
    - else
      - jump to the next block
- else
  - if pc + 1 out of tb
    - pc++
    - return 0
  - else
    - jump to the next block
