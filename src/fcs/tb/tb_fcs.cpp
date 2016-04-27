#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "fcs.hpp"

void crc32_8b(ap_uint<8> din, ap_uint<32>* crc_state){
	unsigned j;

	*crc_state ^= din;
	CRC32_LOOP: for (j = 8; j > 0; j--) {    // Do eight times.
       ap_uint<32> mask = -(*crc_state & 1);
       *crc_state = (*crc_state >> 1) ^ (0xEDB88320 & mask);
    }
}

#define CYCLES 100
#define SEED   280

int main()
{
    int i, j;
    ap_uint<8*FCS_PARALLEL_BYTES> din = 0x00000000;
    ap_uint<32> crc_state = 0xffffffff;
    ap_uint<32> crc_state_8b = 0xffffffff;
    srand(SEED); //time(NULL));

    for (j = 0; j < CYCLES; j++) {
    	din = 0;
    	for (i = 0; i < FCS_PARALLEL_BYTES; i++) {
    		ap_uint<8> din_8b = rand();//inp[i]; //rand();
			crc32_8b(din_8b, &crc_state_8b);
			printf("DIN = 0x%x, CRC - 8b: 0x%x\n", din_8b.to_int(), crc_state_8b.to_int());
			din |= (ap_uint<8*FCS_PARALLEL_BYTES>) din_8b << (i*8);
    	}
		crc32(din, &crc_state);
		printf("DIN = 0x%lx, CRC - 32b: 0x%x\n", din.to_long(), crc_state.to_int());
		if (crc_state != crc_state_8b) {
			return 1;
		}

    }

    return 0;
}
    
