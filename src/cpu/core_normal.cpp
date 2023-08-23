/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "cpu.h"
#include "lazyflags.h"
#include "callback.h"
#include "pic.h"
#include "fpu.h"
#include "paging.h"
#include "mmx.h"
#include "inout.h"
#include "dos_inc.h"
#include "hooklib.h"
#include <dlfcn.h>

#define FAIL(...) do { fprintf(stderr, "FAIL: "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); abort(); } while(0)

typedef struct hooklib hooklib_t;
struct hooklib
{
  void *lib;
  hooklib_init_fn_t init;
  hooklib_exec_fn_t exec;
  hooklib_notify_fn_t notify;
  hooklib_machine_t machine[1];
};

static uint8_t *hooklib_machine_mem_hostaddr(hooklib_machine_ctx_t *, uint32_t addr) {
  // If the address is definitely in conventional memory range, just form a simple host address
  // NOTE: This bypasses the TLB & Paging mechanisms.. so it's only sane in Real Mode assuming
  // we have enough memory!
  if (0x8000 <= addr && addr < 0x9f000) {
    return GetMemBase() + addr;
  }
  FAIL("Cannot form host address here: addr=%08x", addr);

  // const HostPt tlb_addr = get_tlb_read((PhysPt)addr);
  // if (!tlb_addr) FAIL("Cannot form host address here: addr=%08x", addr);
  // return (uint8_t*)tlb_addr + addr;
}

static uint8_t hooklib_machine_mem_read8(hooklib_machine_ctx_t *, uint32_t addr) {
  return mem_readb(addr);
}

static uint16_t hooklib_machine_mem_read16(hooklib_machine_ctx_t *, uint32_t addr) { return mem_readw(addr); }

static void hooklib_machine_mem_write8(hooklib_machine_ctx_t *, uint32_t addr, uint8_t val) { mem_writeb(addr, val); }

static void hooklib_machine_mem_write16(hooklib_machine_ctx_t *, uint32_t addr, uint16_t val) { mem_writew(addr, val); }

static uint8_t hooklib_machine_io_in8(hooklib_machine_ctx_t *, uint16_t port) { return IO_ReadB(port); }

static uint16_t hooklib_machine_io_in16(hooklib_machine_ctx_t *, uint16_t port) { return IO_ReadW(port); }

static void hooklib_machine_io_out8(hooklib_machine_ctx_t *, uint16_t port, uint8_t val) { IO_WriteB(port, val); }

static void hooklib_machine_io_out16(hooklib_machine_ctx_t *, uint16_t port, uint16_t val) { IO_WriteW(port, val); }

static hooklib_t hook[1];
static void init_hook(void)
{
  const char *path = getenv("HOOKLIB_PATH");
  if (!path) path = "hooklib.dylib";

  hook->lib = dlopen(path, RTLD_NOW);
  if (!hook->lib) FAIL("Failed to load hooklib libray from '%s'", path);

  *(void**)&hook->init = dlsym(hook->lib, "hooklib_init");
  if(!hook->init) FAIL("Failed to find 'hooklib_init'");

  *(void**)&hook->exec = dlsym(hook->lib, "hooklib_exec");
  if(!hook->exec) FAIL("Failed to find 'hooklib_exec'");

  *(void**)&hook->notify = dlsym(hook->lib, "hooklib_notify");
  if(!hook->notify) FAIL("Failed to find 'hooklib_notify'");

  memset(hook->machine, 0, sizeof(*hook->machine));
  hook->machine->hardware->ctx              = NULL;
  hook->machine->hardware->mem_hostaddr     = hooklib_machine_mem_hostaddr;
  hook->machine->hardware->mem_read8        = hooklib_machine_mem_read8;
  hook->machine->hardware->mem_read16       = hooklib_machine_mem_read16;
  hook->machine->hardware->mem_write8       = hooklib_machine_mem_write8;
  hook->machine->hardware->mem_write16      = hooklib_machine_mem_write16;
  hook->machine->hardware->io_in8           = hooklib_machine_io_in8;
  hook->machine->hardware->io_in16          = hooklib_machine_io_in16;
  hook->machine->hardware->io_out8          = hooklib_machine_io_out8;
  hook->machine->hardware->io_out16         = hooklib_machine_io_out16;

  hook->init(hook->machine->hardware);
}

static void cpu_state_dump(hooklib_machine_registers_t *cpu)
{
  cpu->ax = reg_ax;
  cpu->bx = reg_bx;
  cpu->cx = reg_cx;
  cpu->dx = reg_dx;

  cpu->si = reg_si;
  cpu->di = reg_di;
  cpu->bp = reg_bp;
  cpu->sp = reg_sp;
  cpu->ip = reg_ip;

  cpu->cs = SegValue(cs);
  cpu->ds = SegValue(ds);
  cpu->es = SegValue(es);
  cpu->ss = SegValue(ss);

  cpu->flags = reg_flags;
}

static void cpu_state_load(hooklib_machine_registers_t *cpu)
{
  reg_ax = cpu->ax;
  reg_bx = cpu->bx;
  reg_cx = cpu->cx;
  reg_dx = cpu->dx;

  reg_si = cpu->si;
  reg_di = cpu->di;
  reg_bp = cpu->bp;
  reg_sp = cpu->sp;
  reg_ip = cpu->ip;

  SegSet16(cs, cpu->cs);
  SegSet16(ds, cpu->ds);
  SegSet16(es, cpu->es);
  SegSet16(ss, cpu->ss);

  reg_flags = cpu->flags;
}

static int attempt_hook(void)
{
  cpu_state_dump(hook->machine->registers);
  int hooked = hook->exec(hook->machine, InterruptCount);
  if (hooked) {
    cpu_state_load(hook->machine->registers);
  }
  return hooked;
}

static void notify_ip(void)
{
  cpu_state_dump(hook->machine->registers);
  hook->notify(hook->machine);
}

void _report_vga_write()
{
  printf("==== VGA write | CS:IP = %04x:%04x\n", SegValue(cs), reg_ip);
}

bool CPU_RDMSR();
bool CPU_WRMSR();
bool CPU_SYSENTER();
bool CPU_SYSEXIT();

#define PRE_EXCEPTION { }

#define CPU_CORE CPU_ARCHTYPE_386

#define DoString DoString_Normal

extern bool ignore_opcode_63;

#if C_DEBUG
#include "debug.h"
#endif

#define LoadMb(off) mem_readb_inline(off)
#define LoadMw(off) mem_readw_inline(off)
#define LoadMd(off) mem_readd_inline(off)
#define LoadMq(off) ((uint64_t)((uint64_t)mem_readd_inline(off+4)<<32 | (uint64_t)mem_readd_inline(off)))
#define SaveMb(off,val)	mem_writeb_inline(off,val)
#define SaveMw(off,val)	mem_writew_inline(off,val)
#define SaveMd(off,val)	mem_writed_inline(off,val)
#define SaveMq(off,val) {mem_writed_inline(off,val&0xffffffff);mem_writed_inline(off+4,(val>>32)&0xffffffff);}

Bitu cycle_count;

#if C_FPU
#define CPU_FPU	1u						//Enable FPU escape instructions
#endif

#define CPU_PIC_CHECK 1u
#define CPU_TRAP_CHECK 1u

#define CPU_TRAP_DECODER	CPU_Core_Normal_Trap_Run

#define OPCODE_NONE			0x000u
#define OPCODE_0F			0x100u
#define OPCODE_SIZE			0x200u

#define PREFIX_ADDR			0x1u
#define PREFIX_REP			0x2u

#define TEST_PREFIX_ADDR	(core.prefixes & PREFIX_ADDR)
#define TEST_PREFIX_REP		(core.prefixes & PREFIX_REP)

#define DO_PREFIX_SEG(_SEG)					\
	BaseDS=SegBase(_SEG);					\
	BaseSS=SegBase(_SEG);					\
	core.base_val_ds=_SEG;					\
	goto restart_opcode;

#define DO_PREFIX_ADDR()								\
	core.prefixes=(core.prefixes & ~PREFIX_ADDR) |		\
	(cpu.code.big ^ PREFIX_ADDR);						\
	core.ea_table=&EATable[(core.prefixes&1u) * 256u];	\
	goto restart_opcode;

#define DO_PREFIX_REP(_ZERO)				\
	core.prefixes|=PREFIX_REP;				\
	core.rep_zero=_ZERO;					\
	goto restart_opcode;

#define REMEMBER_PREFIX(_x) last_prefix = (_x)

static uint8_t last_prefix;

typedef PhysPt (*GetEAHandler)(void);

static const uint32_t AddrMaskTable[2]={0x0000ffffu,0xffffffffu};

static struct {
	Bitu opcode_index;
	PhysPt cseip;
	PhysPt base_ds,base_ss;
	SegNames base_val_ds;
	bool rep_zero;
	Bitu prefixes;
	GetEAHandler * ea_table;
} core;

/* FIXME: Someone at Microsoft tell how subtracting PhysPt - PhysPt = __int64, or PhysPt + PhysPt = __int64 */
#define GETIP		((PhysPt)(core.cseip-SegBase(cs)))
#define SAVEIP		reg_eip=GETIP;
#define LOADIP		core.cseip=((PhysPt)(SegBase(cs)+reg_eip));

#define SegBase(c)	SegPhys(c)
#define BaseDS		core.base_ds
#define BaseSS		core.base_ss

static INLINE void FetchDiscardb() {
	core.cseip+=1;
}

static INLINE uint8_t FetchPeekb() {
	uint8_t temp=LoadMb(core.cseip);
	return temp;
}

static INLINE uint8_t Fetchb() {
	uint8_t temp=LoadMb(core.cseip);
	core.cseip+=1;
	return temp;
}

static INLINE uint16_t Fetchw() {
	uint16_t temp=LoadMw(core.cseip);
	core.cseip+=2;
	return temp;
}
static INLINE uint32_t Fetchd() {
	uint32_t temp=LoadMd(core.cseip);
	core.cseip+=4;
	return temp;
}

#define Push_16 CPU_Push16
#define Push_32 CPU_Push32
#define Pop_16 CPU_Pop16
#define Pop_32 CPU_Pop32

#include "instructions.h"
#include "core_normal/support.h"
#include "core_normal/string.h"


#define EALookupTable (core.ea_table)

Bits CPU_Core_Normal_Run(void) {
  if (CPU_Cycles <= 0)
    return CBRET_NONE;

  while (1) {
    notify_ip();
    if (!(CPU_Cycles-->0)) break;

		LOADIP;
		last_prefix=MP_NONE;
		core.opcode_index=cpu.code.big*(Bitu)0x200u;
		core.prefixes=cpu.code.big;
		core.ea_table=&EATable[cpu.code.big*256u];
		BaseDS=SegBase(ds);
		BaseSS=SegBase(ss);
		core.base_val_ds=ds;
#if C_DEBUG
#if C_HEAVY_DEBUG
		if (DEBUG_HeavyIsBreakpoint()) {
			FillFlags();
			return (Bits)debugCallback;
		}
#endif
#endif
    // hooklib integration
    if (attempt_hook()) {
      continue;
    }

		cycle_count++;
restart_opcode:
		switch (core.opcode_index+Fetchb()) {
		#include "core_normal/prefix_none.h"
		#include "core_normal/prefix_0f.h"
		#include "core_normal/prefix_66.h"
		#include "core_normal/prefix_66_0f.h"
		default:
		illegal_opcode:
#if C_DEBUG	
			{
				bool ignore=false;
				Bitu len=(GETIP-reg_eip);
				LOADIP;
				if (len>16) len=16;
				char tempcode[16*2+1];char * writecode=tempcode;
				if (ignore_opcode_63 && mem_readb(core.cseip) == 0x63)
					ignore = true;
				for (;len>0;len--) {
					sprintf(writecode,"%02X",mem_readb(core.cseip++));
					writecode+=2;
				}
				if (!ignore)
					LOG(LOG_CPU,LOG_NORMAL)("Illegal/Unhandled opcode %s",tempcode);
			}
#endif
			CPU_Exception(6,0);
			continue;
		gp_fault:
			LOG_MSG("Segment limit violation");
			CPU_Exception(EXCEPTION_GP,0);
			continue;
		}
		SAVEIP;
    notify_ip();
	}
	FillFlags();
	return CBRET_NONE;
decode_end:
	SAVEIP;
  notify_ip();
	FillFlags();
	return CBRET_NONE;
}

Bits CPU_Core_Normal_Trap_Run(void) {
	Bits oldCycles = CPU_Cycles;
	CPU_Cycles = 1;
	cpu.trap_skip = false;

	Bits ret=CPU_Core_Normal_Run();
	if (!cpu.trap_skip) CPU_DebugException(DBINT_STEP,reg_eip);
	CPU_Cycles = oldCycles-1;
	cpudecoder = &CPU_Core_Normal_Run;

	return ret;
}



void CPU_Core_Normal_Init(void) {
  init_hook();
}
