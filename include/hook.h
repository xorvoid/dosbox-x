#ifndef DOSBOX_HOOK_H
#define DOSBOX_HOOK_H
#include <stdint.h>

void HOOK_Init(const char *libpath);
int HOOK_Attempt(void);
void HOOK_Notify_Ip(void);
int HOOK_AudioCallback(uint8_t *stream, int len);

#endif
