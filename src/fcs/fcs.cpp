#include "fcs.hpp"

#ifdef REVERSE_BYTES
unsigned char reverse(unsigned char b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}
#endif

void crc32(ap_uint<8*FCS_PARALLEL_BYTES> din, ap_uint<32> *crc_state){
#pragma HLS LATENCY max=0 min=0

	unsigned i, j;
	ap_uint<32> state = *crc_state;
	ap_uint<8> bytes[8];

	for (i = 0; i < FCS_PARALLEL_BYTES; i++) {
#pragma HLS UNROLL
#ifdef REVERSE_BYTES
		bytes[i] = reverse((din >> (8*i)));
#else
		bytes[i] = (ap_uint<8>) (din >> (8*i));
#endif
	}

 CRC32_LOOP: for (j = 0; j < 8*FCS_PARALLEL_BYTES; j++) {    // Do eight times for each bit.
#pragma HLS UNROLL
        if (j%8 == 0)
        	state ^= bytes[j/8];

        if (state & 1) {
            state = (state >> 1) ^ 0xEDB88320;
        } else {
            state = state >> 1;
        }
    }
    *crc_state = state;
}
