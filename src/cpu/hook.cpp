#include "hook.h"
#include "mem.h"
#include "cpu.h"
#include "inout.h"

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
static bool hook_enable = false;

void HOOK_Init(const char *libpath)
{
  hook->lib = dlopen(libpath, RTLD_NOW);
  if (!hook->lib) FAIL("Failed to load hooklib libray from '%s'", libpath);

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
  hook_enable = true;
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

int HOOK_Attempt(void)
{
  if (!hook_enable) {
    return 0;
  }
  
  cpu_state_dump(hook->machine->registers);
  int hooked = hook->exec(hook->machine, InterruptCount);
  if (hooked) {
    cpu_state_load(hook->machine->registers);
  }
  return hooked;
}

void HOOK_Notify_Ip(void)
{
  if (!hook_enable) {
    return;
  }

  cpu_state_dump(hook->machine->registers);
  hook->notify(hook->machine);
}
