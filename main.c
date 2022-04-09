#include "cpu.h"
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 1GB
#define MEM_SIZE 268435456ul

// test parameters
#define DATA_ADDR 80
#define TEST_SIZE 48000

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Not enough arguments!\n"
           "Please provide a file name.\n");
    exit(1);
  }
  CPU cpu = {};
  uint32_t buf[16] = {};
  uint32_t *ram = malloc(MEM_SIZE * 4);
  FILE *bin = fopen(argv[1], "r");
  if (bin == NULL) {
    perror("Failed to open file");
    exit(1);
  }

  // copy the max function to memory
  int size = 0;
  char *marker = (void *)ram;
  while ((size = fread(buf, 1, 64, bin)) > 0) {
    memcpy(marker, buf, size);
    marker += size;
  }
  fclose(bin);

  srand(time(NULL));
  // copy a test array
  uint32_t array[TEST_SIZE] = {};
  for (int i = 0; i < (sizeof(array) / sizeof(array[0])); i++) {
    array[i] = rand() & 0xffff;
  }

  clock_t start = clock();
  int result = array[0];
  for (int i = 1; i < TEST_SIZE; i++) {
    int tmp = array[i];
    result = result > tmp ? result : tmp;
  }
  clock_t end = clock();
  clock_t native_diff = end - start;
  uint32_t *mark = &ram[DATA_ADDR];
  memcpy(mark, array, sizeof(array));

  // set up arguments
  cpu.reg[0] = DATA_ADDR;
  cpu.reg[1] = TEST_SIZE;

  start = clock();
  while (cpu.pc < MEM_SIZE) {
    execute(&cpu, ram);
    if (cpu.in_delay_slot) {
      execute(&cpu, ram);
      cpu.in_delay_slot = 0;
      cpu.pc = cpu.pc_next;
    }
  }
  end = clock();
  printf("Simulation return value: %d\n", cpu.reg[0]);
  free(ram);
  if (result == cpu.reg[0]) {
    printf("execution finished in %ld us\n", end - start);
    printf("native code finished in %ld us\n", native_diff);
  } else
    return 1;
  return 0;
}
