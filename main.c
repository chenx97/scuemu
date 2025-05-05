#include "codegen.h"
#include "cpu.h"
#include "util.h"

// 1GB
#define MEM_SIZE 268435456ul

// test parameters
#define DATA_ADDR 80
#define TEST_SIZE (1ul << 22)

CPU cpu = {};
int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Not enough arguments!\n"
           "Please provide a file name.\n");
    exit(1);
  }
  uint32_t buf[16] = {};
  clean_up(ram_cleanup) uint32_t *ram = malloc(MEM_SIZE << 2);
  FILE *bin = fopen(argv[1], "r");
  if (bin == NULL) {
    perror("Failed to open file");
    exit(1);
  }

  // copy the max function to memory
  int count = 0;
  uint32_t *marker = (void *)ram;
  while ((count = fread(buf, 4, 16, bin)) > 0) {
    memcpy(marker, buf, count << 2);
    marker += count;
  }
  fclose(bin);

  srand((int)clock());
  // copy a test array
  uint32_t *now = &ram[DATA_ADDR];
  int result = 1u << 31;
  for (int i = 0; i < TEST_SIZE; i++) {
    int32_t v = rand();
    *now = v;
    result = result > v ? result : v;
    now++;
  }

  uint32_t *mark = &ram[DATA_ADDR];

  // set up arguments
  cpu.reg[0] = DATA_ADDR;
  cpu.reg[1] = TEST_SIZE;

  clean_up(tb_cleanup) GTree *tb = g_tree_new(tb_cmp);
  printf("begin execution\n");
  clock_t start = clock();
  int interp_thr = 1 << 14;
  while (cpu.pc < (uintptr_t)(marker - ram)) {
    if (interp_thr < 0) {
      void *key = (void *)(uintptr_t)cpu.pc;
      void *data = g_tree_lookup(tb, key);
      if (data == NULL || data == (void *)(uintptr_t)1) {
        void *new_data =
            gen_code(&ram[cpu.pc], cpu.pc, (uintptr_t)(marker - &ram[cpu.pc]));
        if (data) {
          g_tree_replace(tb, key, new_data);
        } else {
          g_tree_insert(tb, key, new_data);
        }
        data = new_data;
      }
      int (*fn)(void) = gcc_jit_result_get_code(data, "_start");
      if (fn) {
        cg_dprintf("entering jit at pc = %d\n", cpu.pc);
        int r = fn();
        cg_dprintf("exiting jit at pc = %d\n", cpu.pc);
        if (r) {
          // mark as invalid
          cg_dprintf("dirty jump target found, new pc = %d\n", cpu.pc);
          gcc_jit_result_release(data);
          g_tree_replace(tb, key, (void *)(uintptr_t)1);
        }
      } else {
        g_tree_replace(tb, key, (void *)(uintptr_t)1);
      }
    } else {
      execute(&cpu, ram);
      if (cpu.in_delay_slot) {
        execute(&cpu, ram);
        cpu.in_delay_slot = 0;
        cpu.pc = cpu.pc_next;
        interp_thr--;
      }
      interp_thr--;
    }
  }
  clock_t end = clock();
  printf("Simulation return value: %d\n", cpu.reg[0]);
  printf("execution finished in %ld us\n", end - start);
  if (result != cpu.reg[0]) {
    printf("expected x0: %d, %x\n", result, result);
  }
  for (int i = 0; i < 64; i++) {
    if (cpu.reg[i]) {
      printf("x%d: %d\n", i, cpu.reg[i]);
    }
  }
  printf("pc: %d\n", cpu.pc);
  printf("z: %d\n", cpu.apsr[0]);
  printf("n: %d\n", cpu.apsr[1]);
  printf("translation block entry count: %d\n", g_tree_nnodes(tb));
  return 0;
}
