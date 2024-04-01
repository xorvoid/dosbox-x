#include "hydra.h"
#include "export/dosbox-x/hydra_machine.h"

#include "mem.h"
#include "cpu.h"
#include "inout.h"
#include "logging.h"

#include <dlfcn.h>

#define FAIL(...) do { fprintf(stderr, "FAIL: "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); abort(); } while(0)

typedef struct hydra hydra_t;
struct hydra
{
  void *lib;
  hydra_machine_init_fn_t init;
  hydra_machine_exec_fn_t exec;
  hydra_machine_notify_fn_t notify;
  hydra_machine_t machine[1];
  hydra_machine_audio_t audio[1];
};

static uint8_t *hydra_machine_mem_hostaddr(hydra_machine_ctx_t *, uint32_t addr) {
  // If the address is definitely in conventional memory range, just form a simple host address
  // NOTE: This bypasses the TLB & Paging mechanisms.. so it's only sane in Real Mode assuming
  // we have enough memory!
  if (0x8000 <= addr && addr < 0x9f000) {
    return GetMemBase() + addr;
  }
  FAIL("Cannot form host address here: addr=%08x", addr);
}

static uint8_t hydra_machine_mem_read8(hydra_machine_ctx_t *, uint32_t addr) {
  return mem_readb(addr);
}

static uint16_t hydra_machine_mem_read16(hydra_machine_ctx_t *, uint32_t addr) { return mem_readw(addr); }

static void hydra_machine_mem_write8(hydra_machine_ctx_t *, uint32_t addr, uint8_t val) { mem_writeb(addr, val); }

static void hydra_machine_mem_write16(hydra_machine_ctx_t *, uint32_t addr, uint16_t val) { mem_writew(addr, val); }

static uint8_t hydra_machine_io_in8(hydra_machine_ctx_t *, uint16_t port) { return IO_ReadB(port); }

static uint16_t hydra_machine_io_in16(hydra_machine_ctx_t *, uint16_t port) { return IO_ReadW(port); }

static void hydra_machine_io_out8(hydra_machine_ctx_t *, uint16_t port, uint8_t val) { IO_WriteB(port, val); }

static void hydra_machine_io_out16(hydra_machine_ctx_t *, uint16_t port, uint16_t val) { IO_WriteW(port, val); }

static hydra_t hydra[1];
static bool hydra_enable = false;

void HYDRA_Init(const char *libpath)
{
  LOG_MSG("Loading HYDRA from library %s", libpath);

  hydra->lib = dlopen(libpath, RTLD_NOW);
  if (!hydra->lib) FAIL("Failed to load hydra libray from '%s'", libpath);

  *(void**)&hydra->init = dlsym(hydra->lib, "hydra_machine_init");
  if(!hydra->init) FAIL("Failed to find 'hydra_machine_init'");

  *(void**)&hydra->exec = dlsym(hydra->lib, "hydra_machine_exec");
  if(!hydra->exec) FAIL("Failed to find 'hydra_machine_exec'");

  *(void**)&hydra->notify = dlsym(hydra->lib, "hydra_machine_notify");
  if(!hydra->notify) FAIL("Failed to find 'hydra_machine_notify'");

  memset(hydra->machine, 0, sizeof(*hydra->machine));
  hydra->machine->hardware->ctx              = NULL;
  hydra->machine->hardware->mem_hostaddr     = hydra_machine_mem_hostaddr;
  hydra->machine->hardware->mem_read8        = hydra_machine_mem_read8;
  hydra->machine->hardware->mem_read16       = hydra_machine_mem_read16;
  hydra->machine->hardware->mem_write8       = hydra_machine_mem_write8;
  hydra->machine->hardware->mem_write16      = hydra_machine_mem_write16;
  hydra->machine->hardware->io_in8           = hydra_machine_io_in8;
  hydra->machine->hardware->io_in16          = hydra_machine_io_in16;
  hydra->machine->hardware->io_out8          = hydra_machine_io_out8;
  hydra->machine->hardware->io_out16         = hydra_machine_io_out16;

  hydra->init(hydra->machine->hardware, hydra->audio);
  hydra_enable = true;
}

static void cpu_state_dump(hydra_machine_registers_t *cpu)
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

static void cpu_state_load(hydra_machine_registers_t *cpu)
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

int HYDRA_Attempt(void)
{
  if (!hydra_enable) {
    return 0;
  }

  cpu_state_dump(hydra->machine->registers);
  int hydraed = hydra->exec(hydra->machine, InterruptCount);
  if (hydraed) {
    cpu_state_load(hydra->machine->registers);
  }
  return hydraed;
}

void HYDRA_Notify_Ip(void)
{
  if (!hydra_enable) {
    return;
  }

  cpu_state_dump(hydra->machine->registers);
  hydra->notify(hydra->machine);
}

int HYDRA_AudioCallback(uint8_t *stream, int len)
{
  if (hydra->audio->cb) {
    hydra->audio->cb(hydra->audio->ctx, stream, len);
    return 1;
  } else {
    return 0;
  }
}
