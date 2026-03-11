#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main();
void timerinit();

// entry.S needs one stack per CPU.
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// entry.S jumps here in machine mode on stack0.
void
start()
{

  // delegate all interrupts and exceptions to supervisor mode.
  w_medeleg(0xffff);
  w_mideleg(0xffff);

  w_sie(r_sie() | SIE_SEIE | SIE_STIE); // supervisor interrupt enable

  // configure Physical Memory Protection to give supervisor mode
  // access to all of physical memory.
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  // ask for clock interrupts.
  timerinit();

  // set M Previous Privilege mode to Supervisor, for mret.
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // set M Exception Program Counter to main, for mret.
  // requires gcc -mcmodel=medany
  w_mepc((uint64)main);

  // disable paging for now.
  w_satp(0);

  // keep each CPU's hartid in its tp register, for cpuid().
  int id = r_mhartid();
  w_tp(id);

  // switch to supervisor mode and jump to main().
  asm volatile("mret");
}

// yicheng: 
//          Context: Harts run in 3 modes, either Machine mode or Supervisor mode or User mode.
//             In each mode, the hart can access and modify different kinds of CSR (mhartid). hart = hardware thread
//             By default, 1. traps (exceptions and interrupts) are handled in M-mode;
//                         2. physical memory can only be accessed and modified in M-mode;
//                         3. the timer registers can only be accessed in M-mode.
//          In function "start", each hart sets up its CSR (control and state registers).
//          The initialization can be divided into 4 parts:
//          1. delegate traps to S-mode;
//             https://github.com/riscv-software-src/opensbi/blob/51fe6a8bc958166ff79805cf69bafe5e297776f4/lib/sbi/sbi_hart.c#L216
//          2. give full access of physical memory to S-mode;
//             https://github.com/torvalds/linux/blob/d358e5254674b70f34c847715ca509e46eb81e6f/arch/riscv/kernel/head.S#L223-L226
//          3. enable timer interrupt for S-mode;
//          4. switch mode.
//             https://github.com/riscv-software-src/opensbi/blob/51fe6a8bc958166ff79805cf69bafe5e297776f4/lib/sbi/sbi_hart.c#L1061

// ask each hart to generate timer interrupts.
void
timerinit()
{
  // enable supervisor-mode timer interrupts.
  // yicheng : redundant?
  w_mie(r_mie() | MIE_STIE);
  
  // enable the sstc extension (i.e. stimecmp).
  // yicheng: sets STCE (STimecmp Enable)
  w_menvcfg(r_menvcfg() | (1L << 63)); 
  
  // allow supervisor to use stimecmp and time.
  // yicheng: TM (time)
  w_mcounteren(r_mcounteren() | 2);
  
  // ask for the very first timer interrupt.
  w_stimecmp(r_time() + 1000000);
}

// yicheng: Mechanism of timer interrupt
//          `time` is the real time register shared by all harts.
//          `stimecmp` is the relative time register for comparison, one for each hart.
//          `time` increases by itself while `stimecmp` must be manually written.
//          If `time` *becomes* greater than to equal to `stimecmp`, it triggers a timer interrupt
//          by setting the STIP bit of `sip` (supervisor interrupt pending) to 1.
//          If `stimecmp` becomes greater than `time`, `sip.STIP` will be cleared to 0.