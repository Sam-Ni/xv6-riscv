        # qemu -kernel loads the kernel at 0x80000000
        # and causes each hart (i.e. CPU) to jump there.
        # yicheng: xv6 run on the "virt" machine of qemu.
        #   The phisical memory layout of "virt" is encoded in https://github.com/qemu/qemu/blob/2257f52a97f28ce3be4366817ea8817ad866562b/hw/riscv/virt.c#L84.
        #   When "virt" is reset, the program counter of each hart will be set to 0x1000, the address of MROM.
        #   Then, each hart begins to fetch instructions and finally jumps to the base address of DRAM, which is 0x80000000.
        #   Typically, 0x80000000 should store openSBI (open source supervisor binary interface),
        #   which offers setups in Machine mode for a bootloader of OS kernel.
        #   xv6 sets up in M-mode by itself through "entry.S" and "start.c", which is a simplified process following openSBI.
        #   Typical boot flow of Linux riscv: ROM --> OpenSBI           --> Linux kernel
        #   Boot flow of xv6-riscv          : ROM --> entry.S + start.c --> the remaining of xv6 kernel

        # kernel.ld causes the following code to
        # be placed at 0x80000000.
.section .text
.global _entry
_entry:
        # set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # sp = stack0 + ((hartid + 1) * 4096)
        la sp, stack0
        li a0, 1024*4
        csrr a1, mhartid
        addi a1, a1, 1
        mul a0, a0, a1
        add sp, sp, a0
        # jump to start() in start.c
        call start
spin:   # yicheng: dead code
        j spin

        # yicheng: 
        #          Stack layout: ---------------- stack0 (boundary of `sp` for hart0)
        #                        |              |
        #                        |              |
        #                        |              |
        #                        |              |
        #                        ---------------- stack0 + 4096 (initial `sp` for hart0, also as the boundary of `sp` for hart1)
        #                        |              |
        #                        |              |
        #                        |              |
        #                        |              |
        #                        ---------------- stack0 + 4096 * 2 (initial `sp` for hart1)
        #                        |              |
        #                        |              |
        #                        |              |

        #          The setup of stack for each hart is necessary, 
        #          because the following C functions rely on the `sp` register to stores function arguments and return addresses.

        #          Corresponding code in openSBI: https://github.com/riscv-software-src/opensbi/blob/51fe6a8bc958166ff79805cf69bafe5e297776f4/firmware/fw_base.S#L354-L367.