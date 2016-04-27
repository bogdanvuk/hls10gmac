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
#define mask_up_to_bit(w, pos) (((ap_uint<w>) -1) >> (w - pos - 1))

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
    ap_uint<8> last_user_word_mask = 0x00;
    ap_uint<8> crc_field_mask = 0x00;
    int last_user_word_pos;
    ap_uint<3> last_user_byte_lane;
    int frame_end_detected = 0;
    int user_data_end_detected = 0;
	int last_user_byte_lane_before_frame_end;
 MAIN: while (1) {

     if (user_data_end_detected) {
    	int fcs_err = (crc_state != 0x00000000);
		int len_err = 0;
		int frm_byte_cnt = frm_cnt*8 + last_user_byte_lane_before_frame_end + 1 + 4;
		int under = (frm_byte_cnt < 64);
		int over = (frm_byte_cnt > 1500);

		if (is_len(len_type)) {
			len_err = (len_type != ((last_user_word_pos - 2) * 8 + 2 + last_user_byte_lane + 1));
		}

		int good = !(fcs_err | len_err | data_err | under | over);

		m_axis.write((t_axis){last_word.rxd, mask_up_to_bit(8, last_user_byte_lane), !good, 1});
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

        frame_end_detected = 0;
        user_data_end_detected = 0;
        last_user_word_mask = 0xff;
    USER_DATA: do {
#pragma HLS LATENCY max=0 min=0
    		// END-OF-FRAME detection
            if (cur.rxc != 0x00) {
                switch(cur.rxc) {
                case 0xe0 : last_user_byte_lane_before_frame_end = 0; crc_field_mask = 0x1e; break;
                case 0xc0 : last_user_byte_lane_before_frame_end = 1; crc_field_mask = 0x3c; break;
                case 0x80 : last_user_byte_lane_before_frame_end = 2; crc_field_mask = 0x78; break;
                default: crc_field_mask = 0xff; break;
                }
                frame_end_detected = 1;
                if (!user_data_end_detected) {
                    last_word = cur;
                    last_user_byte_lane = last_user_byte_lane_before_frame_end;
                }
                user_data_end_detected = 1;
            } else if ((precur.rxc != 0x00) && ((ap_uint<8>) ~precur.rxc <= 0x0f)) {
                switch(precur.rxc) {
                case 0xf0 : last_user_byte_lane_before_frame_end = 7; break;
                case 0xf8 : last_user_byte_lane_before_frame_end = 6; crc_field_mask = 0x80; break;
                case 0xfc : last_user_byte_lane_before_frame_end = 5; crc_field_mask = 0xc0; break;
                case 0xfe : last_user_byte_lane_before_frame_end = 4; crc_field_mask = 0xe0; break;
                case 0xff : last_user_byte_lane_before_frame_end = 3; crc_field_mask = 0xf0; frame_end_detected = 1; break;
                }
                if (!user_data_end_detected) {
                    last_word = cur;
                    last_user_byte_lane = last_user_byte_lane_before_frame_end;
                }
                user_data_end_detected = 1;
            } else {
                crc_field_mask = 0x00;
            }

            // END-OF-USER-DATA detection
            if (!user_data_end_detected) {
				if (frm_cnt == 1) {
					len_type = ((ap_uint<16>) wbyte(cur.rxd, 4) << 8) | wbyte(cur.rxd, 5);
					if (is_len(len_type)) {
						// Calculate the position of the last word within the frame
						// which contains valid user data (based on LENGTH/TYPE field
						// value. -3 is subtracted before division because first two
						// bytes of user data are not word aligned, and 1 is subtracted
						// additionally since we want to find out the last word that
						// still contains data, not the number of user data words. +2
						// since first word aligned user data byte starts at word #2.
						last_user_word_pos = (len_type - 3) / 8 + 2;
						last_user_byte_lane = (len_type - 3) % 8;

						if (len_type <= 2) {
							user_data_end_detected = 1;
							last_word = cur;
						}
					}
				} else if (is_len(len_type)) {
					if (frm_cnt == last_user_word_pos) {
						last_word = cur;
						user_data_end_detected = 1;
					}
				}
            }

    		if (!user_data_end_detected) {
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
