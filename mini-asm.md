GS+ Pro Extreme Max HD Performance Edition includes a sweet-16 mini assembler as well as a 65816 mini assembler.

## Sweet-16
From the Debug shell, enter `!!` to enter the sweet-16 mini asembler.

Enter `^D` or a blank line to exit back to the debug shell.

Lines consist of an optional address, an opcode, and operands.

All numbers are hexadecimal.
The `*` operand is the current address/PC.  +/- offsets may also be applied. 

Registers are named `R0`-`R15` (decimal). `ACC`, `PC` and `SR` aliases are also valid for `R0`, `R15`, and `R14`, respectively.

### Examples

0300: set R1, #0300
      ld @R1
      br *-3
      
## 65816
From the Debug shell, enter `!` to enter the sweet-16 mini asembler.

Enter `^D` or a blank line to exit back to the debug shell.

Lines consist of an optional address, an opcode, and operands.
  
All numbers are hexadecimal.
The `*` operand is the current address/PC.  +/- offsets may also be applied.

The M/X bits are automatically set via `REP`/`SEP` instructions.  Additionally, the `long` and `short` directives
may be used to set them explicitly.

    long mx
    short m
 
Toolbox, GS/OS, ProDOS-16, and P8 MLI macros are auto generated from the NiftyList.Data file.

    _NewHandle
    _OpenGS 123456
    


