# Ultra-simple RISC-V program for testing
# Just add two numbers

.section .text
.global _start

_start:
    li x1, 5        # x1 = 5
    li x2, 7        # x2 = 7  
    add x3, x1, x2  # x3 = 12
    nop             # no-op
    nop
    j _start        # loop back