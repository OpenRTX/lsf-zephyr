
#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "endian.h"

#include "interface.h"
#include "adapter.h"
#include "priv_events.h"

static const char* TAG = "priv_events";

#define SPI_BUFFER_SIZE          (1600)

#define MAC2STR(a)                  (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR                      "%02x:%02x:%02x:%02x:%02x:%02x"

static interface_context_t *if_context = NULL;
static interface_handle_t *if_handle = NULL;

static void station_event_monitor(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void softap_event_monitor(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static esp_err_t priv_post_event_to_host(struct esp_priv_event *event);
static inline void* copy_mac(uint8_t *dest, uint8_t *src, uint8_t *len);

esp_err_t priv_events_init(interface_context_t *in_if_ctx, interface_handle_t *in_if_hdl)
{
    if ((in_if_ctx == NULL) || (in_if_hdl == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    if_context = in_if_ctx;
    if_handle = in_if_hdl;
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &station_event_monitor, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &station_event_monitor, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &softap_event_monitor, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &softap_event_monitor, NULL));
    return ESP_OK;
}

static void station_event_monitor(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_DISCONNECTED)) {
        ESP_LOGI(TAG,"wifi disconnected");
        struct esp_priv_event priv_event;
        bzero(&priv_event, sizeof(struct esp_priv_event));
        priv_event.event_type = ESP_PRIV_EVENT_WIFI_STA_DISCONN;
        priv_post_event_to_host(&priv_event);
    } else if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_STA_CONNECTED)) {
        ESP_LOGI(TAG,"wifi connected");
    }
}

static void softap_event_monitor(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
        struct esp_priv_event priv_event;
        bzero(&priv_event, sizeof(struct esp_priv_event));
        priv_event.event_type = ESP_PRIV_EVENT_WIFI_AP_STACONN;
        uint8_t *priv_data = priv_event.event_data;  //Get the first element index
        priv_data = copy_mac(priv_data, event->mac, &priv_event.event_len);  //Get the currently last element index
        *priv_data = event->aid; priv_data++; priv_event.event_len++;
        priv_post_event_to_host(&priv_event);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
        struct esp_priv_event priv_event;
        bzero(&priv_event, sizeof(struct esp_priv_event));
        priv_event.event_type = ESP_PRIV_EVENT_WIFI_AP_STADISCONN;
        uint8_t *priv_data = priv_event.event_data;
        priv_data = copy_mac(priv_data, event->mac, &priv_event.event_len);
        *priv_data = event->aid; priv_data++; priv_event.event_len++;
        priv_post_event_to_host(&priv_event);
    }
}

static esp_err_t priv_post_event_to_host(struct esp_priv_event *event)
{
    interface_buffer_handle_t buf_handle = {0};
    struct esp_priv_event *temp_event;
    struct esp_payload_header *header = NULL;
    memset(&buf_handle, 0, sizeof(buf_handle));
    buf_handle.payload = heap_caps_malloc(SPI_BUFFER_SIZE, MALLOC_CAP_DMA);
    assert(buf_handle.payload);
    memset(buf_handle.payload, 0, SPI_BUFFER_SIZE);

    header = (struct esp_payload_header *) buf_handle.payload;
    header->if_type = ESP_PRIV_IF;
    header->if_num = 0;
    header->offset = htole16(sizeof(struct esp_payload_header));
    header->priv_pkt_type = ESP_PACKET_TYPE_EVENT;
    /* Populate event data (potocal payload body) */
    temp_event = (struct esp_priv_event *) (buf_handle.payload + sizeof(struct esp_payload_header));
    temp_event->event_type = event->event_type;
    temp_event->event_len = event->event_len;
    memcpy(temp_event->event_data, event->event_data, event->event_len);
    /* payload len = Event len + sizeof(event type) + sizeof(event len) */
    uint16_t payload_len = sizeof(event->event_type) + sizeof(event->event_len) + event->event_len;
    header->len = htole16(payload_len);

    buf_handle.payload_len = payload_len + sizeof(struct esp_payload_header);
	header->checksum = htole16(compute_checksum(buf_handle.payload, buf_handle.payload_len));

    interface_send_raw_pkg(&buf_handle, portMAX_DELAY);
    return ESP_OK;
}

static inline void* copy_mac(uint8_t *dest, uint8_t *src, uint8_t *len)
{
    for (int i = 0; i < 6; i++) {
        *dest = src[i]; dest++; (*len)++;
    }
    return dest;
}
