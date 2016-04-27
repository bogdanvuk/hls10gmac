#include "fcs.hpp"

void crc32(ap_uint<8*FCS_PARALLEL_BYTES> din, ap_uint<8> dv, ap_uint<32> *crc_state){
#pragma HLS LATENCY max=0 min=0

	unsigned i, j;
	ap_uint<32> state = *crc_state;
	ap_uint<8> bytes[8];

	for (i = 0; i < FCS_PARALLEL_BYTES; i++) {
#pragma HLS UNROLL
		bytes[i] = (ap_uint<8>) (din >> (8*i));
	}

 CRC32_LOOP: for (j = 0; j < 8*FCS_PARALLEL_BYTES; j++) {    // Do eight times for each bit.
#pragma HLS UNROLL
	 	 if (dv & (1 << (j/8))) {
			if (j%8 == 0)
				state ^= bytes[j/8];

			if (state & 1) {
				state = (state >> 1) ^ 0xEDB88320;
			} else {
				state = state >> 1;
			}
	 	 }
    }
    *crc_state = state;
}
