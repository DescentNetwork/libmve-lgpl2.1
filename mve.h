#ifndef LIB_MVE_H
#define LIB_MVE_H

#include <stdbool.h>

void mve_init(void);
void mve_deinit(void);

bool mve_open(const char* fileName);
bool mve_close(void);

bool mve_play(void);

#endif /* LIB_MVE_H */
