#ifndef __TRANSMIT_HPP__
#define __TRANSMIT_HPP__

#include "hlsmac.hpp"
#include "ap_int.h"
#include <hls_stream.h>
#include <stdio.h>

typedef struct{
    ap_uint<64>     txd;
    ap_uint<8>      txc;
}t_m_xgmii;

typedef struct {
	ap_uint<14>     count;
	ap_uint<1>      good;
	ap_uint<1>      broad;
	ap_uint<1>      multi;
	ap_uint<1>      ctrl;
	ap_uint<1>      pause;
	ap_uint<1>      under;
}t_tx_status;

void transmit( hls::stream<t_axis> &s_axis, hls::stream<t_m_xgmii> &m_xgmii, hls::stream<t_tx_status> &tx_status);

#endif
