#include <stdio.h>

#include "receive.hpp"

int frm1[] = {
    0xfb, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xd5,
    0x00, 0x10, 0xa4, 0x7b, 0xea, 0x80, 0x00, 0x12,
    0x34, 0x56, 0x78, 0x90, 0x08, 0x00, 0x45, 0x00,
    0x00, 0x2e, 0xb3, 0xfe, 0x00, 0x00, 0x80, 0x11,
    0x05, 0x40, 0xc0, 0xa8, 0x00, 0x2c, 0xc0, 0xa8,
    0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x1a,
    0x2d, 0xe8, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
    0x0e, 0x0f, 0x10, 0x11, 0xb3, 0x31, 0x88, 0x1b};

int frm2[] = {
	0xfb, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xd5,
    0x00, 0x10, 0xa4, 0x7b, 0xea, 0x80, 0x00, 0x12,
    0x34, 0x56, 0x78, 0x90, 0x00, 0x2e, 0x45, 0x00,
    0x00, 0x2e, 0xb3, 0xfe, 0x00, 0x00, 0x80, 0x11,
    0x05, 0x40, 0xc0, 0xa8, 0x00, 0x2c, 0xc0, 0xa8,
    0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x1a,
    0x2d, 0xe8, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
    0x11, 0x57, 0x38, 0xc4, 0xaf};

int frm3[] = {
    0xfb, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xd5,
    0x00, 0x10, 0xa4, 0x7b, 0xea, 0x80, 0x00, 0x12,
    0x34, 0x56, 0x78, 0x90, 0x00, 0x02, 0x45, 0x00,
    0x00, 0x2e, 0xb3, 0xfe, 0x00, 0x00, 0x80, 0x11,
    0x05, 0x40, 0xc0, 0xa8, 0x00, 0x2c, 0xc0, 0xa8,
    0x00, 0x04, 0x04, 0x00, 0x04, 0x00, 0x00, 0x1a,
    0x2d, 0xe8, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
    0x05, 0xf9, 0x38, 0xdc};

typedef struct {
    int* data;
    int len;
}t_frame;

#define frm_inst(f) ((t_frame) {(f), sizeof(f) / sizeof(int)})

t_frame frames[] = {
    frm_inst(frm2),
    frm_inst(frm1),
    frm_inst(frm3)
};

#define FRAMES_CNT sizeof(frames) / sizeof(t_frame)

#define replace_byte(w, b, pos) ((w) & (~(0xffL << (pos)*8))) | ((((ap_uint<64>) b) & 0xff) << (pos)*8)

int main()
{
    int i;
    int j;
    int k;

    hls::stream<t_axis> s_axis;
    hls::stream<t_s_xgmii> xgmii;
    int correct_frames = 0;
    hls::stream<t_rx_status> rx_status_stream;

    for (j = 0; j < FRAMES_CNT; j++) {
    	i = 0;
    	int start_word = 0;
        while(i < frames[j].len){
        	t_s_xgmii word = (t_s_xgmii) {0x0707070707070707, 0xff};
            ap_uint<64> mask;
            for(k = 0; (k < 8) && (i < frames[j].len); k++){
                word.rxd = replace_byte(word.rxd, frames[j].data[i++], k);
                word.rxc <<= 1;
            }
            if (!start_word) {
            	word.rxc = 0x01;
            	start_word = 1;
            }

            if (i == frames[j].len) {
            	if (k < 8) {
            		word.rxd = replace_byte(word.rxd, 0xfd, k);
            	}
            }
            xgmii.write(word);
        }
        if (k == 8) {
        	xgmii.write((t_s_xgmii) {0x07070707070707fd, 0xff});
        } else {
        	xgmii.write((t_s_xgmii) {0x0707070707070707, 0xff});
        }

    }
    for (j = 0; j < 5; j++) {
        xgmii.write((t_s_xgmii) {0x0707070707070707, 0xff});
    }

    receive(xgmii, s_axis, rx_status_stream);

    printf("*****************************************************************\n");
    printf("RECEIVED FRAME %d\n", j);
    printf("*****************************************************************\n");
    t_axis m_axis;
    while (!s_axis.empty()) {
        t_axis din=s_axis.read();
        printf("DATA 0x%016lx, KEEP 0x%02x, LAST %d, USER %d\n", din.data.to_long(), din.keep.to_int(), din.last.to_int(), din.user.to_int());
        if (din.last) {
        	t_rx_status rx_status = rx_status_stream.read();
            if((!m_axis.user) && (!rx_status.fcs_err) && (!rx_status.len_err)){
                correct_frames++;
            }
            printf("*****************************************************************\n");
            printf("Frame status: count=%d, good=%d, under=%d, len_err=%d, fcs_err=%d, data_err=%d, over=%d\n",
                   rx_status.count.to_int(),
                   rx_status.good.to_int(),
                   rx_status.under.to_int(),
                   rx_status.len_err.to_int(),
                   rx_status.fcs_err.to_int(),
                   rx_status.data_err.to_int(),
                   rx_status.over.to_int()
                   );
            printf("*****************************************************************\n");
        }
    }

    printf("*****************************************************************\n");
    if (correct_frames < FRAMES_CNT) {
        printf("FRAME ERROR %d/%d\n", FRAMES_CNT-correct_frames, FRAMES_CNT);
        return 0;
    }

    printf("Frame correct %d/%d\n", FRAMES_CNT,FRAMES_CNT);
    printf("*****************************************************************\n");

    return 0;

}
