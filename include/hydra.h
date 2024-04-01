#ifndef DOSBOX_HYDRA_H
#define DOSBOX_HYDRA_H
#include <stdint.h>

void HYDRA_Init(const char *libpath);
int HYDRA_Attempt(void);
void HYDRA_Notify_Ip(void);
int HYDRA_AudioCallback(uint8_t *stream, int len);

#endif
