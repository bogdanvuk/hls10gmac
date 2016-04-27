#include "receive.hpp"
#include "fcs.hpp"
#include "ap_shift_reg.h"

#include <stdio.h>

ap_uint<32> crc_state = 0xffffffff;
ap_uint<16> frm_cnt = 0;
ap_uint<16> usr_cnt = 0;
int data_err = 0;
ap_uint<16> len_type = 0xffff;

static ap_shift_reg<t_s_xgmii, 5> pipeline;

#define is_type(x) ((x) >= 0x0600)
#define is_len(x) ((x) <= 0x05DC)

#define SFD_CHAR      0xd5

#define replace_byte(w, b, pos) ((w) & (~(0xffL << (pos)*8))) | ((((ap_uint<64>) b) & 0xff) << (pos)*8)
#define wbit(w, pos) (((w) >> (pos)) & 0x1)
#define wbyte(w, pos) (((w) >> (8*pos)) & 0xff)

void receive(
             hls::stream<t_s_xgmii> &s_xgmii,
             hls::stream<t_axis> &m_axis,
             t_rx_status* rx_status
             )
{

    //#pragma HLS interface ap_ctrl_none port=return
    //#pragma HLS data_pack variable=s_xgmii
#pragma HLS INTERFACE axis port=m_axis
#pragma HLS data_pack variable=rx_status
#pragma HLS INTERFACE ap_ovld port=rx_status
    int i;
    t_s_xgmii din;
    t_s_xgmii ignore;
//    t_s_xgmii last;
    int output_en;
	t_s_xgmii cur = {0, 0};
	t_s_xgmii precur = {0, 0};
    t_s_xgmii last_word;
    ap_uint<8> last_data_mask = 0x00;
    ap_uint<8> crc_field_mask = 0x00;

 MAIN: while (1) {

     if (last_data_mask) {
    	int fcs_err = (crc_state != 0xDEBB20E3);
		int len_err = 0;
		int under = (frm_cnt < 64);
		int over = (frm_cnt > 1500);

		if (is_len(len_type)) {
		len_err = (len_type != usr_cnt);

		}

		int good = !(fcs_err | len_err | data_err | under | over);

		m_axis.write((t_axis){last_word.rxd, last_data_mask, !good, 1});
		*rx_status = (t_rx_status) {frm_cnt, good, 0, 0, under, len_err, fcs_err, data_err, 0, over};
     }

     cur = precur;
     if (!s_xgmii.read_nb(precur)) return;

	 frm_cnt = 0;
	 output_en = 1;
	 crc_state = 0xffffffff;
	 len_type = 0xffff;
	 usr_cnt = 0;

    IDLE_AND_PREAMBLE: while (!((cur.rxc == 0x01) && (cur.rxd == 0xd5555555555555fb))) {
#pragma HLS LATENCY max=0 min=0
    		cur = precur;
    		if (!s_xgmii.read_nb(precur)) return;
            printf("RXD 0x%016lx, RXC 0x%02x\n", cur.rxd.to_long(), cur.rxc.to_int());
        }

		cur = precur;
		if (!s_xgmii.read_nb(precur)) return;
        printf("RXD 0x%016lx, RXC 0x%02x\n", cur.rxd.to_long(), cur.rxc.to_int());

        int frame_end_detected = 0;
        int last = 0;
        last_data_mask = 0xff;
    USER_DATA: do {
#pragma HLS LATENCY max=0 min=0
            // if (frm_cnt < 14) {
            // 	m_axis.write((t_axis){cur.rxd, 0, 0});
            // 	if (frm_cnt == 12) {
            // 		len_type = (len_type & 0xff00) | cur.rxd;
            // 	} else if (frm_cnt == 13) {
            // 		len_type = (len_type << 8) | cur.rxd;
            // 	}
            // } else {
            //     if (output_en) {
            //         if (din.dv && (len_type != 0) && (usr_cnt < len_type - 1)) {
            //             usr_cnt++;
            //             m_axis.write((t_axis){cur.rxd, 0, 0});
            //         } else {
            //             usr_cnt++;
            //             last = cur;
            //             output_en = 0;
            //         }
            //     }
            // }

            if (!last) {
                if (cur.rxc != 0x00) {
                    switch(cur.rxc) {
            		case 0xe0 : last_data_mask = 0x01; crc_field_mask = 0x1e; break;
            		case 0xc0 : last_data_mask = 0x03; crc_field_mask = 0x3c; break;
            		case 0x80 : last_data_mask = 0x07; crc_field_mask = 0x78; break;
            		default: last_data_mask = 0x00; break;
                    }
                    frame_end_detected = 1;
                    last_word = cur;
                    last = 1;
                } else if ((precur.rxc != 0x00) && ((ap_uint<8>) ~precur.rxc <= 0x0f)) {
                    switch(precur.rxc) {
            		case 0xf0 : last_data_mask = 0xff; break;
            		case 0xf8 : last_data_mask = 0x7f; crc_field_mask = 0x80; break;
            		case 0xfc : last_data_mask = 0x3f; crc_field_mask = 0xc0; break;
            		case 0xfe : last_data_mask = 0x1f; crc_field_mask = 0xe0; break;
            		case 0xff : last_data_mask = 0x0f; crc_field_mask = 0xf0; frame_end_detected = 1; break;
            		default: last_data_mask = 0x00; break;
                    }
                    last_word = cur;
                    last = 1;
                } else {
                	crc_field_mask = 0x00;
                }
            } else {
                frame_end_detected = 1;
                crc_field_mask = 0xff;
            }

    		if (!last) {
    			m_axis.write((t_axis){cur.rxd, 0xff, 0, 0});
    			frm_cnt++;
    		}

    		ap_uint<64> crc_data = 0;
    		CRC_MASK_CALC: for (i = 0; i < 8; i++) {
#pragma HLS LOOP unroll
    			if (!wbit(cur.rxc, i)) {
    				ap_uint<8> d = wbyte(cur.rxd, i);
    				if (wbit(crc_field_mask,i)) {
                        d = ~d;
    				}

                    crc_data = replace_byte(crc_data, d, i);
    			}
    		}

    		crc32(crc_data, &crc_state);
    		printf("RXD 0x%016lx, RXC 0x%02x, CRC_DATA 0x%016lx, crc_field_mask 0x%02x, CRC_STATE 0x%08lx, FRMEND %d\n", cur.rxd.to_long(), cur.rxc.to_int(), crc_data.to_long(), crc_field_mask.to_int(), crc_state.to_int(), frame_end_detected);
    		//printf("RXD 0x%016lx, RXC 0x%02x, CRC_STATE 0x%08lx, FRMEND %d\n", cur.rxd.to_long(), cur.rxc.to_int(), crc_state.to_int(), frame_end_detected);

    		cur = precur;
            if (!s_xgmii.read_nb(precur)) return;
        } while(!frame_end_detected);

    }
}
