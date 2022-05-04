#include "lisa_evs_uart_parser.h"
#include "lisa_uart_protocol.h"
#include "lisa_uart_recv.h"
#include "lisa_uart_send.h"

lisa_err_t lisa_evs_parser_uart_init(lisa_evs_uart_cb_t uart_cb, listen_algo_config_t config)
{
    lisa_uart_protocol_t* protocol = lisa_uart_protocol_init(uart_cb);
    lisa_uart_recv_create(protocol, config);
    lisa_uart_send_init(10);
    return LISA_OK;
}

lisa_err_t lisa_evs_scan_tts_text(const uint8_t* const text, unsigned int len)
{
    return lisa_uart_scan_tts_text(text, len);
}

lisa_err_t lisa_evs_uart_set_scan_mode(scan_mode_e mode, int wait_ms)
{
    return lisa_uart_set_scan_mode(mode, wait_ms);
}

lisa_err_t lisa_evs_uart_get_csk_version(all_version_query_e type)
{
    return lisa_uart_get_csk_version(type);
}

lisa_err_t lisa_evs_set_tts_base_speed(uint8_t language, uint8_t speed)
{
    uint16_t param = (uint16_t)(((speed << 8) & 0xFF00) | language);
    return lisa_uart_set_scan_param(e_listen_set_tts_base_speed, param);
}

lisa_err_t lisa_evs_set_tts_speed(uint8_t language, uint8_t speed)
{
    uint16_t param = (uint16_t)(((speed << 8) & 0xFF00) | language);
    return lisa_uart_set_scan_param(e_listen_set_tts_speed, param);
}

lisa_err_t lisa_evs_set_multiline_timeout(uint8_t timeout)
{
    return lisa_uart_set_scan_param(e_listen_set_multiline_timeout, timeout);
}

lisa_err_t lisa_evs_set_scan_direction(uint8_t direction)
{
    return lisa_uart_set_scan_param(e_listen_set_scan_direction, direction);
}

lisa_err_t lisa_evs_scan_tts_auth(uint8_t *code, uint32_t len)
{
    return lisa_uart_scan_tts_auth(code, len);
}

lisa_err_t lisa_evs_uart_adjustment(int16_t x_offset, int16_t y_offset)
{
    return lisa_uart_adjustment(x_offset, y_offset);
}

lisa_err_t lisa_evs_uart_frame_recv()
{
    return lisa_uart_frame_recv() ? LISA_OK : LISA_FAIL;
}

lisa_err_t lisa_evs_uart_frame_send()
{
    lisa_uart_recv();
    listen_send_uart_frame();
    return LISA_OK;
}

lisa_err_t lisa_evs_parser_uart_deinit()
{
    return lisa_uart_send_deinit(10);
}
