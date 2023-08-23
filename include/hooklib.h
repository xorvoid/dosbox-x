#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hooklib_machine            hooklib_machine_t;
typedef struct hooklib_machine_ctx        hooklib_machine_ctx_t;
typedef struct hooklib_machine_hardware   hooklib_machine_hardware_t;
typedef struct hooklib_machine_registers  hooklib_machine_registers_t;

struct hooklib_machine_hardware
{
  hooklib_machine_ctx_t *ctx;

  uint8_t *(*mem_hostaddr)(hooklib_machine_ctx_t *ctx, uint32_t addr);
  uint8_t  (*mem_read8)(hooklib_machine_ctx_t *ctx, uint32_t addr);
  uint16_t (*mem_read16)(hooklib_machine_ctx_t *ctx, uint32_t addr);
  void     (*mem_write8)(hooklib_machine_ctx_t *ctx, uint32_t addr, uint8_t val);
  void     (*mem_write16)(hooklib_machine_ctx_t *ctx, uint32_t addr, uint16_t val);

  uint8_t  (*io_in8)(hooklib_machine_ctx_t *ctx, uint16_t port);
  uint16_t (*io_in16)(hooklib_machine_ctx_t *ctx, uint16_t port);
  void     (*io_out8)(hooklib_machine_ctx_t *ctx, uint16_t port, uint8_t val);
  void     (*io_out16)(hooklib_machine_ctx_t *ctx, uint16_t port, uint16_t val);
};

struct hooklib_machine_registers
{
  uint16_t ax, bx, cx, dx;
  uint16_t si, di, bp, sp, ip;
  uint16_t cs, ds, es, ss;
  uint16_t flags;
};

struct hooklib_machine
{
  hooklib_machine_hardware_t   hardware[1];
  hooklib_machine_registers_t  registers[1];
};

#define HOOKLIB_INIT_FUNC(name) void name(hooklib_machine_hardware_t *hw)
HOOKLIB_INIT_FUNC(hooklib_init);
typedef HOOKLIB_INIT_FUNC((*hooklib_init_fn_t));

#define HOOKLIB_EXEC_FUNC(name) int name(hooklib_machine_t *m, size_t interrupt_count)
HOOKLIB_EXEC_FUNC(hooklib_exec);
typedef HOOKLIB_EXEC_FUNC((*hooklib_exec_fn_t));

#define HOOKLIB_NOTIFY_FUNC(name) void name(hooklib_machine_t *m)
HOOKLIB_NOTIFY_FUNC(hooklib_notify);
typedef HOOKLIB_NOTIFY_FUNC((*hooklib_notify_fn_t));

#ifdef __cplusplus
}
#endif
