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

//void receive(
//             hls::stream<t_s_xgmii> &s_xgmii,
//             hls::stream<t_axis> &m_axis,
//             t_rx_status* rx_status
//             )
//{
//	t_s_xgmii din;
//	t_s_xgmii cur = {0, 0};
//	t_s_xgmii precur = {0, 0};
//	MAIN: while (1) {
//		if (!s_xgmii.read_nb(din)) return;
//		cur = precur;
//		precur = pipeline.shift(din);
//		m_axis.write((t_axis){cur.rxd, 0xff, 0, 0});
//		*rx_status = (t_rx_status) {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
////		cur = precur;
//
//
//		IDLE_AND_PREAMBLE: while (!((cur.rxc == 0x01) && (cur.rxd == 0xd5555555555555fb))) {
//		#pragma HLS LATENCY max=0 min=0
//		            if (!s_xgmii.read_nb(din)) return;
//		            cur = precur;
//		            precur = pipeline.shift(din);
//		            m_axis.write((t_axis){cur.rxd, 0xff, 0, 0});
////
//		            //printf("RXD 0x%016lx, RXC 0x%02x\n", cur.rxd.to_long(), cur.rxc.to_int());
//		        }
//
//	}
//
//
//}
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

 MAIN: while (1) {
        //#pragma HLS PIPELINE rewind

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
            		case 0xe0 : last_data_mask = 0x01; break;
            		case 0xc0 : last_data_mask = 0x03; break;
            		case 0x80 : last_data_mask = 0x07; break;
            		default: last_data_mask = 0x00; break;
                    }
                    frame_end_detected = 1;
                    last_word = cur;
                    last = 1;
                } else if ((precur.rxc != 0x00) && ((ap_uint<8>) ~precur.rxc <= 0x0f)) {
                    switch(precur.rxc) {
            		case 0xf0 : last_data_mask = 0xff; break;
            		case 0xf8 : last_data_mask = 0x7f; break;
            		case 0xfc : last_data_mask = 0x3f; break;
            		case 0xfe : last_data_mask = 0x1f; break;
            		case 0xff : last_data_mask = 0x0f; frame_end_detected = 1; break;
            		default: last_data_mask = 0x00; break;
                    }
                    last_word = cur;
                    last = 1;
                }
            } else {
                frame_end_detected = 1;
            }

    		if (!last) {
    			m_axis.write((t_axis){cur.rxd, 0xff, 0, 0});
    			frm_cnt++;
    		}

    		ap_uint<64> crc_mask = 0;
    		CRC_MASK_CALC: for (i = 0; i < 8; i++) {
#pragma HLS LOOP unroll
    			if (!(cur.rxc & (1 << i))) {
    				crc_mask |= ((ap_uint<64>) 0xff << (8*i));
    			}
    		}

    		crc32(cur.rxd, ~cur.rxc, &crc_state);
    		printf("RXD 0x%016lx, RXC 0x%02x, CRC_MASK 0x%016lx, FRMEND %d\n", cur.rxd.to_long(), cur.rxc.to_int(), crc_mask.to_long(), frame_end_detected);

    		cur = precur;
            if (!s_xgmii.read_nb(precur)) return;
//            cur = precur;
//            precur = pipeline.shift(din);


            //            printf("Data 0x%x, FCS 0x%x\n", cur.rxd.to_int(), ~crc_state.to_int()); \
            //if (!gmii_input(s_xgmii, din, cur, pipeline, &empty)) goto FRAME_END;
        } while(!frame_end_detected);

//		cur = precur;
//        if (!s_xgmii.read_nb(precur)) return;

		/*

          *rx_status = (t_rx_status) {frm_cnt, good, 0, 0, under, len_err, fcs_err, data_err, 0, over};
          m_axis.write((t_axis){last.rxd, !good, 1});
          if (!s_xgmii.read_nb(din)) return;
          cur = pipeline.shift(din);
        */
    }
}
