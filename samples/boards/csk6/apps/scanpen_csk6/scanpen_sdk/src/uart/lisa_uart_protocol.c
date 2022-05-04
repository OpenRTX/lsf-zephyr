#include "lisa_uart_protocol.h"
#include <lisa_log.h>
#include <lisa_mem.h>
#include <lisa_time.h>
#include <string.h>

#define TAG "lisa_uart_protocol"

struct uart_frame_cmd {
	char *name;
	bool (*exec)(frame_desc_u *head);
	frame_desc_u *proto_recv;
};
bool _frame_cmd_send_algo(frame_desc_u *head);

static lisa_uart_protocol_t* s_uart_protocol = NULL;

static frame_desc_u s_send_frame __attribute__((aligned(32))) = {{0}};

// recv frame
static frame_desc_u proto_recv_algo[2] __attribute__((aligned(32))) = {{{0}}};

const struct uart_frame_cmd frame_cmd_send = {"uart frame algo", _frame_cmd_send_algo, proto_recv_algo};

static frame_desc_u *head = {NULL};
uart_fifo_t proto_fifo = {
		        .buff = &proto_recv_algo[0],
				.pos = 0,
				.cnt = sizeof(proto_recv_algo) / sizeof(proto_recv_algo[0])};

static uint8_t pack_check(void *dat, uint16_t len)
{
	uint8_t check = 0;
	uint8_t *data = (uint8_t *)dat;
	while (len--)
		check += *data++;
	return check;
}

lisa_uart_protocol_t* lisa_uart_protocol_init(lisa_evs_uart_cb_t cb)
{
    lisa_uart_protocol_t* handle = (lisa_uart_protocol_t*)lisa_mem_calloc(1, sizeof(lisa_uart_protocol_t));
    handle->uart_cb = cb;
    s_uart_protocol = handle;
	return handle;
}

lisa_err_t lisa_uart_add_recv_frame_cb(uart_frame_recv_cb cb)
{
	s_uart_protocol->frame_recv_cb = cb;
	return LISA_OK;

}

void lisa_uart_pack_send(void *dat, int len)
{
	s_send_frame.frame_u.header_t.tag = FRM_HDR_TAG;
	s_send_frame.frame_u.header_t.len = len + 9;  // 9 = FRM_LEN - DAT_LEN
	LISA_LOGV(TAG, "\n HDR_LEN : %d", s_send_frame.frame_u.header_t.len);
	s_send_frame.frame_u.header_t.frame_id++;
	s_send_frame.frame_u.header_t.head_check =
			(0 - pack_check(&s_send_frame, 5));  // head check
	s_send_frame.frame_u.data_t.type = 0xF0;
	s_send_frame.frame_u.data_t.address = 0x00;
	memcpy(&s_send_frame.frame_u.data_t.data[0], dat, len);
	s_send_frame.frame_u.data_t.data[len] =
			(0 - pack_check(&s_send_frame.frame_u.data_t.type, len + 2));

#if 0
	printf("DATA TO SEND:\n");
	for (int i = 0; i < s_send_frame.frame_u.header_t.len; i++) {
		printf("%02x ", s_send_frame.frame_data[i]);
		if ((i + 1) % 16 == 0) printf("\n");
	}
	printf("\n");
#endif

    s_uart_protocol->uart_cb.write_uart_data((void *)&s_send_frame, s_send_frame.frame_u.header_t.len);
	// uart_send(UART_COM_ALGO, (void *)&s_send_frame, s_send_frame.frame_u.header_t.len);
}

static bool _frame_check_head(frame_desc_u *head)
{
	memset(head, 0, sizeof(frame_desc_u));
	if (1 != s_uart_protocol->uart_cb.read_uart_data((uint8_t *)&(head->frame_data[0]), 1, LISA_WAIT_FOREVER)) {
		LISA_LOGE(TAG, "queue read FRM_HDR_TAG_L error.\n");
		return false;
	} else {
		if (FRM_HDR_TAG_L != head->frame_data[0]) return false;
	}

	if (1 != s_uart_protocol->uart_cb.read_uart_data((uint8_t *)&(head->frame_data[1]), 1, LISA_WAIT_FOREVER)) {
		LISA_LOGE(TAG, "queue read FRM_HDR_TAG_H error.\n");
		return false;
	} else {
		if (FRM_HDR_TAG_H != head->frame_data[1]) return false;
	}

	if (4 != s_uart_protocol->uart_cb.read_uart_data((uint8_t *)&(head->frame_data[2]), 4, LISA_WAIT_FOREVER)) {
		LISA_LOGE(TAG, "queue read FRM_HDR_LEN -2(4) error\n.");
		return false;
	} else {
		if (pack_check((void *)head, FRM_HDR_LEN)) return false;
	}

	return true;
}

static bool _frame_read_data(frame_desc_u *head)
{
	uint16_t to_recv = 0;
	if (head->frame_u.header_t.len > FRM_MAX_LEN) return false;

	to_recv = head->frame_u.header_t.len - FRM_HDR_LEN;
	uint16_t recv_len = 0;
	uint16_t once_len = 0;
	uint16_t recv_total_len = 0;
	uint8_t *tmp_buf = (uint8_t *)lisa_mem_alloc(to_recv);
	if (!tmp_buf) return false;
	while (recv_total_len < to_recv) {
		once_len = to_recv - recv_total_len;
		recv_len = s_uart_protocol->uart_cb.read_uart_data(tmp_buf + recv_total_len, once_len, LISA_WAIT_FOREVER);
		if (recv_len > 0) recv_total_len += recv_len;
	}
	if (recv_total_len <= to_recv)
		memcpy((uint8_t *)&(head->frame_data[FRM_HDR_LEN]), tmp_buf, recv_total_len);
	if (tmp_buf) {
		lisa_mem_free(tmp_buf);
	}
	if (pack_check((void *)&(head->frame_data[FRM_HDR_LEN]), to_recv)) return false;

#if 0
	ALGO_LOG("DART RECVED :\n");
	for (int i = 0; i < head->frame_u.header_t.len; i++) {
		ALGO_LOG("%02x ", head->frame_data[i]);
		if ((i + 1) % 16 == 0) ALGO_LOG("\n");
	}
	ALGO_LOG("\n");
#endif

	return true;
}

bool _frame_cmd_send_algo(frame_desc_u *head)
{
	s_uart_protocol->frame_recv_cb(head);
	return true;
}

bool lisa_uart_frame_recv()
{
	head = (frame_desc_u *)&frame_cmd_send
						 .proto_recv[proto_fifo.pos % proto_fifo.cnt];
	if (_frame_check_head(head) == false) return false;
	if (_frame_read_data(head) == false) return false;
	proto_fifo.pos++;

	return frame_cmd_send.exec(head);
}
