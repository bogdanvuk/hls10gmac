#ifndef __CRC32_HPP__
#define __CRC32_HPP__

#include "ap_int.h"
#define FCS_PARALLEL_BYTES 8
void crc32(ap_uint<8*FCS_PARALLEL_BYTES> din, ap_uint<8> dv, ap_uint<32>* crc_state);
#endif
