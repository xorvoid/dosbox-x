#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hydra_machine            hydra_machine_t;
typedef struct hydra_machine_ctx        hydra_machine_ctx_t;
typedef struct hydra_machine_hardware   hydra_machine_hardware_t;
typedef struct hydra_machine_registers  hydra_machine_registers_t;
typedef struct hydra_machine_audio      hydra_machine_audio_t;

struct hydra_machine_hardware
{
  hydra_machine_ctx_t *ctx;

  uint8_t *(*mem_hostaddr)(hydra_machine_ctx_t *ctx, uint32_t addr);
  uint8_t  (*mem_read8)(hydra_machine_ctx_t *ctx, uint32_t addr);
  uint16_t (*mem_read16)(hydra_machine_ctx_t *ctx, uint32_t addr);
  void     (*mem_write8)(hydra_machine_ctx_t *ctx, uint32_t addr, uint8_t val);
  void     (*mem_write16)(hydra_machine_ctx_t *ctx, uint32_t addr, uint16_t val);

  uint8_t  (*io_in8)(hydra_machine_ctx_t *ctx, uint16_t port);
  uint16_t (*io_in16)(hydra_machine_ctx_t *ctx, uint16_t port);
  void     (*io_out8)(hydra_machine_ctx_t *ctx, uint16_t port, uint8_t val);
  void     (*io_out16)(hydra_machine_ctx_t *ctx, uint16_t port, uint16_t val);
};

struct hydra_machine_registers
{
  uint16_t ax, bx, cx, dx;
  uint16_t si, di, bp, sp, ip;
  uint16_t cs, ds, es, ss;
  uint16_t flags;
};

struct hydra_machine
{
  hydra_machine_hardware_t   hardware[1];
  hydra_machine_registers_t  registers[1];
};

struct hydra_machine_audio
{
  void (*cb)(void * userdata, uint8_t *stream, int len);
  void *ctx;
};

#define HYDRA_MACHINE_INIT_FUNC(name) void name(hydra_machine_hardware_t *hw, hydra_machine_audio_t *audio)
HYDRA_MACHINE_INIT_FUNC(hydra_machine_init);
typedef HYDRA_MACHINE_INIT_FUNC((*hydra_machine_init_fn_t));

#define HYDRA_MACHINE_EXEC_FUNC(name) int name(hydra_machine_t *m, size_t interrupt_count)
HYDRA_MACHINE_EXEC_FUNC(hydra_machine_exec);
typedef HYDRA_MACHINE_EXEC_FUNC((*hydra_machine_exec_fn_t));

#define HYDRA_MACHINE_NOTIFY_FUNC(name) void name(hydra_machine_t *m)
HYDRA_MACHINE_NOTIFY_FUNC(hydra_machine_notify);
typedef HYDRA_MACHINE_NOTIFY_FUNC((*hydra_machine_notify_fn_t));

#ifdef __cplusplus
}
#endif
