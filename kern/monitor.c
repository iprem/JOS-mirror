// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/dwarf.h>
#include <kern/kdebug.h>
#include <kern/dwarf_api.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display a listing of function call frames", mon_backtrace }
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	cprintf("Stack backtrace:\n");
	uint64_t stack_base_p = read_rbp();
	uint64_t ret_inst_p;
	struct Ripdebuginfo rip_debug_info;
	read_rip(ret_inst_p);
	while(stack_base_p != 0) {
		cprintf("%5s %016x%5s %016x\n", "rbp", stack_base_p, 
			"rip", ret_inst_p);
		if (debuginfo_rip(ret_inst_p, &rip_debug_info) == 0) {
			// Print th file name and line within that file of the stack frame's rip
			cprintf("%7s%s:%d: ", " ", rip_debug_info.rip_file, rip_debug_info.rip_line);
			// Print the name of the function
			cprintf("%.*s+", rip_debug_info.rip_fn_namelen, rip_debug_info.rip_fn_name);
			// Print the offset of the rip from the first instruction of the function
			cprintf("%016x", ret_inst_p - rip_debug_info.rip_fn_addr);
			// Print the number of function arguments
			cprintf("  args:%d ", rip_debug_info.rip_fn_narg);
			// Print the actual arguments themselves
			int arg_num;
			uint64_t arg_addr = stack_base_p;
			for (arg_num = 0; arg_num < rip_debug_info.rip_fn_narg; arg_num++) {
				arg_addr -= rip_debug_info.size_fn_arg[arg_num];
				if (rip_debug_info.size_fn_arg[arg_num] == sizeof(uint32_t)) {
					cprintf(" %016x", *(uint32_t *)arg_addr);
				} else {
					arg_addr = arg_addr - arg_addr % 8;
					cprintf(" %016x", *(uint64_t *)arg_addr);
				}
			}
			cprintf("\n");
		}
		ret_inst_p = *((uint64_t *)(stack_base_p + 1 * sizeof(uint64_t)));
		stack_base_p = *(uint64_t *)stack_base_p;
	}
	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
