
#include "disasm.h"


const char * const disasm_opcodes[256] = {
#define x(a,b,c,d) #a,
#include "opcodes.x"
#undef x
};


const unsigned disasm_types[256] = {
#define x(a,b,c,d) b + (d << 8),
#include "opcodes.x"
#undef x
};
