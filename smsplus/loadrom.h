#ifndef _LOADROM_H_
#define _LOADROM_H_

#include <stdint.h>

/* Function prototypes */
int load_rom(char *filename);

inline uint32_t *getCartAddress() {
	extern uint32_t ADDR_CART[];
	return ADDR_CART;
}

#endif /* _LOADROM_H_ */

