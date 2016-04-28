#ifndef __CRC32_HPP__
#define __CRC32_HPP__

#include "ap_int.h"

//#define FCS_PARALLEL_BYTES 8

template<typename T> void crc32(T din, ap_uint<32> *crc_state);
#endif
