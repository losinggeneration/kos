#include <arch/arch.h>

void _exit(int code) {
	arch_exit(code);
}
