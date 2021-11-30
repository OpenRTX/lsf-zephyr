#include <sys/queue.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "stream_buffer.h"

#include "Driver_SPI.h"
#include "csk_spi.h"
#include "Driver_Common.h"
#include "venus_log.h"
#include "spi.h"

#include "IOMuxManager.h"

#define SPI_TAG "csk_spi"

#define STREAMBUFFER_DEFAULT_TRIGGER_BYTE   10

#define SPI_CHECK(cond, ret_val) ({                                           \
    if (!(cond)) {                                                            \
        CLOGE(SPI_TAG ": %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__);      \
        return (ret_val);                                                     \
    }                                                                         \
})

#define SPI_CHECK_GOTO(cond, label) ({                                        \
    if (!(cond)) {                                                            \
        goto label;                                                           \
    }                                                                         \
})

#define SPI_FREE_AND_NULL(ptr) ({                                             \
    vPortFree(ptr);                                                           \
    (ptr) = NULL;                                                             \
})

#define SPI_CLK_PIN             19 //   IO18
#define SPI_CS_PIN              16 //   IO5
#define SPI_MISO_PIN            17 //  IO19
#define SPI_MOSI_PIN            6 //  IO23


// #define SPI_CLK_PIN             16 //   IO18
// #define SPI_CS_PIN              17 //   IO5
// #define SPI_MISO_PIN            18 //  IO19
// #define SPI_MOSI_PIN            19 //  IO23


typedef struct {
    // StreamBufferHandle_t tx_buffer_handle;
    // StreamBufferHandle_t rx_buffer_handle;
    uint32_t spi_port;
	void *driver;
    void (*isr_cb)(uint32_t);
    SemaphoreHandle_t transfer_lock; //spi transfer transaction lock
    SemaphoreHandle_t transfer_done_sem;
} spi_handle_t;

static spi_handle_t* s_spi_handle_list[2] = {NULL};

__attribute__((section(".code_eilm")))
static void spi0_isr_callback(uint32_t evt_mask)
{
    spi_handle_t *spi_handle = s_spi_handle_list[0];
    BaseType_t task_woken = pdFALSE;

    if (evt_mask & CSK_SPI_EVENT_TRANSFER_COMPLETE) {
        xSemaphoreGiveFromISR(spi_handle->transfer_done_sem, &task_woken);
        portYIELD_FROM_ISR(task_woken);
    } else {
        // CLOGW("spi transaction failed");
    }
    // if (task_woken == pdTRUE) {
    //     portYIELD_FROM_ISR(task_woken);
    // }
}

__attribute__((section(".code_eilm")))
static void spi1_isr_callback(uint32_t evt_mask)
{
    spi_handle_t *spi_handle = s_spi_handle_list[1];
    BaseType_t task_woken = pdFALSE;
    if (evt_mask & CSK_SPI_EVENT_TRANSFER_COMPLETE) {
        xSemaphoreGiveFromISR(spi_handle->transfer_done_sem, &task_woken);
    }

    portYIELD_FROM_ISR(task_woken);
}

static void spi_config_pin(uint8_t spi_port)
{
    (void) spi_port;//todo
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI_CLK_PIN, CSK_IOMUX_FUNC_ALTER6);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI_MOSI_PIN, CSK_IOMUX_FUNC_ALTER6);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI_MISO_PIN, CSK_IOMUX_FUNC_ALTER6);
    IOMuxManager_PinConfigure(CSK_IOMUX_PAD_A, SPI_CS_PIN, CSK_IOMUX_FUNC_ALTER6);
}

cst_err_t csk_spi_init(csk_spi_config_t *spi_config, csk_spi_handle_t *csk_spi_handle)
{
    SPI_CHECK(spi_config != NULL && csk_spi_handle != NULL, CST_ERR_INVALID_ARG);
    SPI_CHECK(spi_config->spi_port >= 0 && spi_config->spi_port < 3, CST_ERR_INVALID_ARG);
    SPI_CHECK(spi_config->spi_parameter_config.data_bit_len > 0 &&
              spi_config->spi_parameter_config.data_bit_len <= 32,
              CST_ERR_INVALID_ARG);

    SPI_CHECK(s_spi_handle_list[spi_config->spi_port] == NULL, CST_ERR_INVALID_STATE); //This spi port has been inited

    cst_err_t ret = CST_OK;
    spi_handle_t *spi_handle = (spi_handle_t *) pvPortMalloc(sizeof(spi_handle_t));
    SPI_CHECK(spi_handle != NULL, CST_ERR_NO_MEM);

    spi_handle->transfer_lock = xSemaphoreCreateMutex();
    spi_handle->transfer_done_sem = xSemaphoreCreateBinary();

    if ((spi_handle->transfer_lock == NULL) || (spi_handle->transfer_done_sem == NULL)) {
        ret = CST_ERR_NO_MEM;
        goto __cleanup;
    }

    uint32_t spi_mode = (spi_config->spi_parameter_config.spi_mode == SPI_MASTER) ?
                                            CSK_SPI_MODE_MASTER : CSK_SPI_MODE_SLAVE;
    uint32_t spi_sample_mode = 0;
    switch (spi_config->spi_parameter_config.spi_sample_mode) {
        case SPI_CPOL0_CPHA0:
            spi_sample_mode = CSK_SPI_CPOL0_CPHA0;
        break;

        case SPI_CPOL0_CPHA1:
            spi_sample_mode = CSK_SPI_CPOL0_CPHA1;
        break;

        case SPI_CPOL1_CPHA0:
            spi_sample_mode = CSK_SPI_CPOL1_CPHA0;
        break;

        case SPI_CPOL1_CPHA1:
            spi_sample_mode = CSK_SPI_CPOL1_CPHA1;
        break;

        default:
            ret = CST_ERR_INVALID_ARG;
            goto __cleanup;
            break;
    }
    uint32_t data_bit_len = CSK_SPI_DATA_BITS(spi_config->spi_parameter_config.data_bit_len);
    uint32_t byte_order = (spi_config->spi_parameter_config.spi_byte_order == SPI_BYTE_LSB) ? CSK_SPI_LSB_MSB : CSK_SPI_MSB_LSB;
    spi_handle->driver = (spi_config->spi_port == 0) ? SPI0() : SPI1();
    spi_handle->isr_cb = (spi_config->spi_port == 0) ? spi0_isr_callback : spi1_isr_callback;
    portENTER_CRITICAL();
    SPI_Initialize(spi_handle->driver, spi_handle->isr_cb, NULL);
    SPI_PowerControl(spi_handle->driver, CSK_POWER_FULL);
    SPI_Control(spi_handle->driver, spi_mode | spi_sample_mode | data_bit_len |
                             byte_order, spi_config->spi_parameter_config.spi_clk);
    spi_config_pin(spi_config->spi_port);
    portEXIT_CRITICAL();
    spi_handle->spi_port = spi_config->spi_port;
    *csk_spi_handle = spi_handle;
    s_spi_handle_list[spi_config->spi_port] = spi_handle;

    return CST_OK;
__cleanup:
    portENTER_CRITICAL();
    SPI_Uninitialize(spi_handle->driver);
    // spi_handle->driver->Uninitialize();
    portEXIT_CRITICAL();
    if (spi_handle->isr_cb != NULL) {
        spi_handle->isr_cb = NULL;
    }
    // if (spi_handle->tx_buffer_handle != NULL) {
    //     vStreamBufferDelete(spi_handle->tx_buffer_handle);
    // }
    // if (spi_handle->rx_buffer_handle != NULL) {
    //     vStreamBufferDelete(spi_handle->rx_buffer_handle);
    // }
    if (spi_handle->transfer_done_sem != NULL) {
        vSemaphoreDelete(spi_handle->transfer_done_sem);
    }
    if (spi_handle->transfer_lock != NULL) {
        vSemaphoreDelete(spi_handle->transfer_lock);
    }
    SPI_FREE_AND_NULL(spi_handle);
    CLOGE("csk spi init failed ret:%d", ret);
    return ret;
}

cst_err_t csk_spi_deinit(csk_spi_handle_t csk_spi_handle)
{
    SPI_CHECK(csk_spi_handle != NULL, CST_ERR_INVALID_ARG);

    spi_handle_t *spi_handle = csk_spi_handle;
    xSemaphoreTake(spi_handle->transfer_lock, portMAX_DELAY);
    portENTER_CRITICAL();
    SPI_Uninitialize(spi_handle->driver);
    portEXIT_CRITICAL();
    vSemaphoreDelete(spi_handle->transfer_lock);
    vSemaphoreDelete(spi_handle->transfer_done_sem);
    // vStreamBufferDelete(spi_handle->tx_buffer_handle);
    // vStreamBufferDelete(spi_handle->rx_buffer_handle);
    SPI_FREE_AND_NULL(spi_handle);
    s_spi_handle_list[spi_handle->spi_port] = NULL;
    return CST_OK;
}

cst_err_t csk_spi_send_recv(csk_spi_handle_t csk_spi_handle, void* tx_data, void* rx_data, uint32_t max_buffer_len, uint32_t xTicksToWait)
{
    SPI_CHECK(csk_spi_handle != NULL, CST_ERR_INVALID_ARG);
    SPI_CHECK(max_buffer_len > 0, CST_ERR_INVALID_ARG);
    SPI_CHECK(tx_data != NULL && rx_data != NULL, CST_ERR_INVALID_ARG);

    spi_handle_t *spi_handle = csk_spi_handle;
    BaseType_t os_ret = xSemaphoreTake(spi_handle->transfer_lock, xTicksToWait);
    if (os_ret != pdPASS) {
        CLOGE("spi timout");
        // xSemaphoreGive(spi_handle->transfer_lock);
        return CST_ERR_TIMEOUT; // timeout
    }
    int32_t ret = SPI_Transfer(spi_handle->driver, tx_data, rx_data, max_buffer_len);
    if (ret != CSK_DRIVER_OK) {
        CLOG("driver excetion");
        xSemaphoreGive(spi_handle->transfer_lock);
        return CST_FAIL;
    }
    xSemaphoreTake(spi_handle->transfer_done_sem, portMAX_DELAY);
    xSemaphoreGive(spi_handle->transfer_lock);
    return CST_OK;
}

cst_err_t csk_spi_send(csk_spi_handle_t csk_spi_handle, void* tx_data, uint32_t tx_len, uint32_t xTicksToWait)
{
    SPI_CHECK(csk_spi_handle != NULL, CST_ERR_INVALID_ARG);
    SPI_CHECK(tx_data != NULL, CST_ERR_INVALID_ARG);
    SPI_CHECK(tx_len > 0, CST_ERR_INVALID_ARG);
    spi_handle_t *spi_handle = csk_spi_handle;
    BaseType_t os_ret = xSemaphoreTake(spi_handle->transfer_lock, xTicksToWait);
    if (os_ret != pdPASS) {
        xSemaphoreGive(spi_handle->transfer_lock);
        return CST_ERR_TIMEOUT; // timeout
    }
    SPI_Send(spi_handle->driver, tx_data, tx_len);
    xSemaphoreTake(spi_handle->transfer_done_sem, portMAX_DELAY);
    xSemaphoreGive(spi_handle->transfer_lock);
    return CST_OK;
}

cst_err_t csk_spi_recv(csk_spi_handle_t csk_spi_handle, void* rx_data, uint32_t rx_len, uint32_t xTicksToWait)
{
    SPI_CHECK(csk_spi_handle != NULL, CST_ERR_INVALID_ARG);
    SPI_CHECK(rx_len > 0, CST_ERR_INVALID_ARG);
    SPI_CHECK(rx_data != NULL, CST_ERR_INVALID_ARG);

    spi_handle_t *spi_handle = csk_spi_handle;
    BaseType_t os_ret = xSemaphoreTake(spi_handle->transfer_lock, xTicksToWait);
    if (os_ret != pdPASS) {
        return CST_ERR_TIMEOUT; // timeout
    }
    SPI_Receive(spi_handle->driver, rx_data, rx_len);
    xSemaphoreTake(spi_handle->transfer_done_sem, portMAX_DELAY);
    xSemaphoreGive(spi_handle->transfer_lock);
    return CST_OK;
}
