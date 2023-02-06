/*
 * Copyright (c) 2021 listenai Intelligent Technology (anhui) Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT listenai_csk_i2c

#include <drivers/i2c.h>
#include "hal/Driver_I2C.h"
#include "hal/i2c.h"
#include <zephyr/drivers/pinctrl.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(i2c_csk);

#define DEV_CFG(dev) ((const struct i2c_csk_config *const)(dev)->config)
#define DEV_DATA(dev) ((struct i2c_csk_data *const)(dev)->data)
#define DEV_BASE(dev) ((I2C_RegDef *)(DEV_CFG(dev))->base)
#define DEV_HANDLER(dev) ((void *)(DEV_DATA(dev)->handle))

#define I2C_SLAVE_ADDR (0x35) /* csk6 default i2c slave address 0x35 */

/** Here we use the lowest rate of 100khz to calculate the timeout
 * 1 * 1000 / (100000) * (256byte * 10(bit) = 25.6ms
 * 25.6ms * 2 = 50
 * At the slowest rate, we only need to wait up to 25.6ms,
 * here we reserve twice the time.
 */
#define I2C_CSK6_MAX_TIME_OUT_MS 50

#define I2C_XFER_MAX_NUM (255)
struct i2c_csk_config {
	I2C_RegDef *base;
	uint32_t bitrate;
	void (*irq_config_func)(const struct device *dev);
	/* I2C alternate configuration */
	const struct pinctrl_dev_config *pcfg;
};

struct i2c_csk_data {
	void *handle; /* driver handle */
	struct k_msgq *msgq; /* driver message queues*/
	struct k_event *evt;
	uint32_t status; /* driver event status */
	bool mode; /* transfer mode: true: master, false: slave */
    struct i2c_slave_config *slave;
};

static void
i2cx_master_fifo_write(I2C_RESOURCES* i2c, uint8_t is_flash_addr);

static void
i2cx_slave_addr_hit_handler(const struct device *dev,I2C_RESOURCES* i2c)
{
    struct i2c_csk_data *data = DEV_DATA(dev);
    uint32_t Tmp_C = 0;
    int32_t stat = 0;

    if (i2c->info->Pwr_State != CSK_POWER_FULL)
    {
        return;
    }
    
    // I2C clear fifo first to prevent mistake i2cx_slave_fifo_read()
    // if the Completion Interrupt asserts, clear the FIFO and go next transaction.
    Tmp_C = i2c->reg->REG_CMD.all;
    Tmp_C &= (~ BITSET_0X028_CMD_MASK);
    Tmp_C |= (BITSET_0X028_CMD_CLEAR_FIFO);
    i2c->reg->REG_CMD.all = Tmp_C;

    // slave mode Rx: if address hit, fifo may not be full state
    if (i2c->info->Driver_State & I2C_DRV_SLAVE_RX)
    {
        // A new I2C data transaction(start-addr-data-stop)
        i2c->info->Xfered_Data_Rd_Ptr_Ow = 0;
        i2c->info->Xfered_Data_Rd_Ptr = 0;
        // keypoint to reset
        i2c->info->middleware_rx_ptr = 0;
        __builtin_memset (i2c->info->Xfer_Data_Rd_Buf, 0,
                sizeof(i2c->info->Xfer_Data_Rd_Buf));

        if (i2c->info->DMA_EN)
        {
            i2c->info->last_rx_data_count =
                    (i2c->info->Slave_Rx_Cmpl_Ctrl_Reg_Val & BITSET_0X024_DATACNT);

            // If DMA is enabled, DataCnt is the number of
            // bytes to transmit/receive. It will not be reset to 0
            // when the slave is addressed and it will be
            // decreased by one for each byte of data
            // transmitted/received.
            // fix bug of dma-slave 10bit address test(rx 3 bytes flash addr)
            Tmp_C = i2c->reg->REG_CTRL.all;
            Tmp_C &= (~ BITSET_0X024_DATACNT);
            Tmp_C |= (MAX_XFER_SZ & BITSET_0X024_DATACNT);
            i2c->reg->REG_CTRL.all = Tmp_C;

            dma_channel_select(
                &i2c->dma_rx->channel,
                i2c->dma_rx->cb_event,
                0,
                DMA_CACHE_SYNC_DST);
            if (i2c->dma_rx->channel == DMA_CHANNEL_ANY) {
                return;
            }

            stat = dma_channel_configure (i2c->dma_rx->channel,
                        (uint32_t) (&(i2c->reg->REG_DATA.all)),
                        (uint32_t) (&i2c->info->Xfer_Data_Rd_Buf[i2c->info->Xfered_Data_Rd_Ptr]),
                        MAX_XFER_SZ,
                        DMA_CH_CTLL_DST_WIDTH(CSK_I2C_RX_DMA_WIDTH) | DMA_CH_CTLL_SRC_WIDTH(CSK_I2C_RX_DMA_WIDTH) |\
                      DMA_CH_CTLL_DST_BSIZE(CSK_I2C_RX_DMA_BSIZE) | DMA_CH_CTLL_SRC_BSIZE(CSK_I2C_RX_DMA_BSIZE) |\
                      DMA_CH_CTLL_DST_INC | DMA_CH_CTLL_SRC_FIX | DMA_CH_CTLL_TTFC_P2M |\
                      DMA_CH_CTLL_DMS(0) | DMA_CH_CTLL_SMS(0) | DMA_CH_CTLL_INT_EN, // control
                      DMA_CH_CFGL_CH_PRIOR(1), // config_low
                      DMA_CH_CFGH_FIFO_MODE | DMA_CH_CFGH_SRC_PER(i2c->dma_rx->reqsel), // config_high
                      0, 0);

            /*if(uart->info->timeout){
                uart->reg->REG_IER.all |= UARTC_IER_RDR;
            }*/

            if(stat == -1){
                return;
            }

        }
        else
        {
            i2c->info->last_rx_data_count = 0;

            // I2C Enable the FIFO Full Interrupt in the Interrupt Enable Register
            Tmp_C = i2c->reg->REG_INTEN.all;
            Tmp_C |= BITSET_0X014_FIFOFULL;
            i2c->reg->REG_INTEN.all = Tmp_C;
			printk("yyyyyyyyyyyyyyyyy\n");
        }
    }
    // slave mode Tx: if address hit, fifo may not be empty state
    else if (i2c->info->Driver_State & I2C_DRV_SLAVE_TX)
    {
        // master will gen nack_assert to notify slave TX complete
        i2c->info->nack_assert = 0;

        if (i2c->info->DMA_EN){
            i2c->reg->REG_SETUP.bit.DMAEN = 0x1;

            i2c->reg->REG_INTEN.all |= (BITSET_0X014_CMPL | BITSET_0X014_ARBLOS);
        }else{
            // I2C Enable the Byte Transmit Interrupt in the Interrupt Enable Register
            // for status.busy flag support
            Tmp_C = i2c->reg->REG_INTEN.all;
            Tmp_C |= BITSET_0X014_BYTETRANS;
            i2c->reg->REG_INTEN.all = Tmp_C;
        }
    }
}
static void
i2cx_slave_fifo_read(I2C_RESOURCES* i2c, uint8_t is_fifo_full)
{
    uint32_t i = 0, read_fifo_count = 0, curr_rx_data_count = 0;

    // slave rx data count is accumulated and depend on
    // master tx data count of one transaction(start-addr-data-stop),
    // possible larger than fifo length(4 bytes)
    // slave: If DMA is not enabled(FIFO mode), DataCnt is the number of
    // bytes transmitted/received from the bus master.
    // It is reset to 0 when the controller is addressed
    // and then increased by one for each byte of data
    // transmitted/received
    // curr_rx_data_count = (i2c->reg->REG_CTRL.all & BITSET_0X024_DATACNT);
    curr_rx_data_count = (i2c->info->Slave_Rx_Cmpl_Ctrl_Reg_Val
            & BITSET_0X024_DATACNT);

    // error hit
    if (curr_rx_data_count > MAX_XFER_SZ)
    {
        while (1)
            ;
    }

    if (is_fifo_full)
    {
        read_fifo_count = i2c->info->FIFO_Depth;
    }
    // just only read fifo in complete interrut handler if fifo is not full
    else
    {
        read_fifo_count = curr_rx_data_count - i2c->info->last_rx_data_count;
    }

    // error hit
    if (read_fifo_count > MAX_XFER_SZ)
    {
        while (1)
            ;
    }

    // I2C read a patch of data(FIFO_Depth) from FIFO,
    // it will be consumed empty if data is actually read out by driver */
    for (i = 0; i < read_fifo_count; i++)
            {
        // I2C read data from FIFO through data port register
        i2c->info->Xfer_Data_Rd_Buf[i2c->info->Xfered_Data_Rd_Ptr] =
                (i2c->reg->REG_DATA.all & (BITSET_0X020_DATA_MASK));

        i2c->info->Xfered_Data_Rd_Ptr++;

        if (i2c->info->Xfered_Data_Rd_Ptr == MAX_XFER_SZ)
        {
            // slave rx buffer overwrite
            i2c->info->Xfered_Data_Rd_Ptr_Ow++;
            i2c->info->Xfered_Data_Rd_Ptr = 0;
        }
    }

    i2c->info->last_rx_data_count = curr_rx_data_count;
}
static void
i2cx_master_fifo_write(I2C_RESOURCES* i2c, uint8_t is_flash_addr)
{
    uint32_t i = 0, write_fifo_count = 0, Tmp_C = 0;

    write_fifo_count =
            ((i2c->info->Xfer_Wt_Num - i2c->info->Xfered_Data_Wt_Ptr)
                    >= i2c->info->FIFO_Depth) ?
                    i2c->info->FIFO_Depth :
                    (i2c->info->Xfer_Wt_Num - i2c->info->Xfered_Data_Wt_Ptr);

    if (is_flash_addr)
    {
        write_fifo_count = 2;
    }

    if (write_fifo_count == 0) {
	    Tmp_C = i2c->reg->REG_INTEN.all;
	    Tmp_C &= (~BITSET_0X014_FIFOEMPTY);
	    i2c->reg->REG_INTEN.all = Tmp_C;
    }

    // I2C write a patch of data(FIFO_Depth) to FIFO,
    // it will be consumed empty if data is actually issued on I2C bus */
    for (i = 0; i < write_fifo_count; i++)
            {
        // I2C write data to FIFO through data port register
        i2c->reg->REG_DATA.all =
                (i2c->info->middleware_tx_buf[i2c->info->Xfered_Data_Wt_Ptr]
                        & (BITSET_0X020_DATA_MASK));

        i2c->info->Xfered_Data_Wt_Ptr++;

        // If all data are pushed into the FIFO, disable the FIFO Empty Interrupt. Otherwise, repeat
        if (i2c->info->Xfered_Data_Wt_Ptr == i2c->info->Xfer_Wt_Num)
                {
            // I2C disable the FIFO Empty Interrupt in the Interrupt Enable Register
            Tmp_C = i2c->reg->REG_INTEN.all;
            Tmp_C &= (~ BITSET_0X014_FIFOEMPTY);
            i2c->reg->REG_INTEN.all = Tmp_C;
        }
    }
}

static void
i2cx_fifo_empty_handler(I2C_RESOURCES* i2c)
{
    if (i2c->info->Pwr_State != CSK_POWER_FULL)
            {
        return;
    }

    if (i2c->info->Driver_State & I2C_DRV_MASTER_TX)
            {
        i2cx_master_fifo_write(i2c, 0);
    }
    else if (i2c->info->Driver_State & I2C_DRV_SLAVE_TX)
            {
        // i2cx_slave_fifo_write(prI2C_Info);
    }
}
static void
i2cx_master_fifo_read(I2C_RESOURCES* i2c)
{
    uint32_t i = 0, read_fifo_count = 0, Tmp_C = 0;

    read_fifo_count =
            ((i2c->info->Xfer_Rd_Num - i2c->info->Xfered_Data_Rd_Ptr)
                    >= i2c->info->FIFO_Depth) ?
                    i2c->info->FIFO_Depth :
                    (i2c->info->Xfer_Rd_Num - i2c->info->Xfered_Data_Rd_Ptr);

    // I2C read a patch of data(FIFO_Depth) from FIFO,
    // it will be consumed empty if data is actually read out by driver
    for (i = 0; i < read_fifo_count; i++)
            {
        // I2C read data from FIFO through data port register
        i2c->info->middleware_rx_buf[i2c->info->Xfered_Data_Rd_Ptr] =
                (i2c->reg->REG_DATA.all & (BITSET_0X020_DATA_MASK));

        i2c->info->Xfered_Data_Rd_Ptr++;

        // If all data are read from the FIFO, disable the FIFO Full Interrupt. Otherwise, repeat
        if (i2c->info->Xfered_Data_Rd_Ptr == i2c->info->Xfer_Rd_Num)
                {
            // I2C disable the FIFO Full Interrupt in the Interrupt Enable Register
            Tmp_C = i2c->reg->REG_INTEN.all;
            Tmp_C &= (~ BITSET_0X014_FIFOFULL);
            i2c->reg->REG_INTEN.all = Tmp_C;
        }
    }
}
static void
i2cx_fifo_full_handler(I2C_RESOURCES* i2c)
{
    if (i2c->info->Pwr_State != CSK_POWER_FULL)
            {
        return;
    }

    if (i2c->info->Driver_State & I2C_DRV_MASTER_RX)
            {
        i2cx_master_fifo_read(i2c);
    }
    else if (i2c->info->Driver_State & I2C_DRV_SLAVE_RX)
            {
        i2cx_slave_fifo_read(i2c, 1);
    }
}
// basic fifo read function
static void
i2c_cmpl_handler(I2C_RESOURCES* i2c)
{
    uint32_t Tmp_S = 0, Tmp_S1 = 0, Tmp_C = 0;

    Tmp_S = i2c->reg->REG_SETUP.all;

    // master mode
    if (Tmp_S & BITSET_0X02C_MASTER)
    {
        // I2C disable all Interrupts in the Interrupt Enable Register
        Tmp_C = i2c->reg->REG_INTEN.all;
        Tmp_C &= (~ BITSET_0X014_INTEN_MASK);
        i2c->reg->REG_INTEN.all = Tmp_C;
    }
    else
    {
        // I2C no disable all Interrupts in the Interrupt Enable Register,
        // keep previous setting for slave tx */
    }

    // check the DataCnt field of the Control Register
    // to know if all data are successfully transmitted.
    // -> Master: The number of bytes to transmit/receive.
    // 0 means 256 bytes. DataCnt will be decreased by one
    // for each byte transmitted/received.
    if ((i2c->info->Driver_State & I2C_DRV_MASTER_TX) ||
            (i2c->info->Driver_State & I2C_DRV_MASTER_RX))
            {
        Tmp_S = i2c->reg->REG_CTRL.all;

        if (i2c->info->Driver_State & I2C_DRV_MASTER_TX)
                {
            if (i2c->info->DMA_EN)
            {
                i2c->info->Xfered_Data_Wt_Ptr = dma_channel_get_count(
                        i2c->dma_tx->channel);
                i2c->info->Xfer_Cmpl_Count = dma_channel_get_count(
                        i2c->dma_tx->channel);
            }
            else
            {
                i2c->info->Xfer_Cmpl_Count = i2c->info->Xfer_Wt_Num
                        - (Tmp_S & BITSET_0X024_DATACNT);
            }

            // clear & set driver state to master tx complete
            i2c->info->Driver_State = I2C_DRV_MASTER_TX_CMPL;

            // clear busy bit on i2c complete event as master dma/cpu tx
            i2c->info->Status.busy = 0;
        }

        if (i2c->info->Driver_State & I2C_DRV_MASTER_RX)
                {
            if (i2c->info->DMA_EN)
            {
                i2c->info->Xfered_Data_Rd_Ptr = dma_channel_get_count(
                        i2c->dma_rx->channel);
                i2c->info->Xfer_Cmpl_Count = dma_channel_get_count(
                        i2c->dma_rx->channel);

                // clear & set driver state to master rx complete
                i2c->info->Driver_State = I2C_DRV_MASTER_RX_CMPL;

            }
            else
            {
                i2cx_master_fifo_read(i2c);

                i2c->info->Xfer_Cmpl_Count = i2c->info->Xfer_Rd_Num
                        - (Tmp_S & BITSET_0X024_DATACNT);

                // clear & set driver state to master rx complete
                i2c->info->Driver_State = I2C_DRV_MASTER_RX_CMPL;

                // clear busy bit on i2c complete event as master cpu rx
                i2c->info->Status.busy = 0;
            }
        }
    }

    // check the DataCnt field of the Control Register
    // to know if all data are successfully transmitted.
    // -> Slave: the meaning of DataCnt depends on the
    // DMA mode:
    // If DMA is not enabled, DataCnt is the number of
    // bytes transmitted/received from the bus master.
    // It is reset to 0 when the controller is addressed
    // and then increased by one for each byte of data
    // transmitted/received.
    // If DMA is enabled, DataCnt is the number of
    // bytes to transmit/receive. It will not be reset to 0
    // when the slave is addressed and it will be
    // decreased by one for each byte of data
    // transmitted/received..
    if ((i2c->info->Driver_State & I2C_DRV_SLAVE_TX) ||
            (i2c->info->Driver_State & I2C_DRV_SLAVE_RX))
            {
        Tmp_S1 = i2c->reg->REG_SETUP.all;
        Tmp_S = i2c->reg->REG_CTRL.all;

        // I2C_FIFO mode
        if (!(Tmp_S1 & BITSET_0X02C_DMAEN))
        {
            if (i2c->info->Driver_State & I2C_DRV_SLAVE_TX)
                    {
                // I2C Disable the Byte Transmit Interrupt in the Interrupt Enable Register
                Tmp_C = i2c->reg->REG_INTEN.all;
                Tmp_C &= (~ BITSET_0X014_BYTETRANS);
                i2c->reg->REG_INTEN.all = Tmp_C;

                // clear & set driver state to slave tx complete
                i2c->info->Driver_State = I2C_DRV_SLAVE_TX_CMPL;
            }

            if (i2c->info->Driver_State & I2C_DRV_SLAVE_RX)
                    {
                i2cx_slave_fifo_read(i2c, 0);

                // I2C Disable the FIFO Full Interrupt in the Interrupt Enable Register
                Tmp_C = i2c->reg->REG_INTEN.all;
                Tmp_C &= (~ BITSET_0X014_FIFOFULL);
                i2c->reg->REG_INTEN.all = Tmp_C;

                // keypoint for middleware to query
                i2c->info->Xfer_Cmpl_Count = i2c->info->Xfered_Data_Rd_Ptr
                        - i2c->info->middleware_rx_ptr;

                // clear & set driver state to slave rx complete
                i2c->info->Driver_State = I2C_DRV_SLAVE_RX_CMPL;
            }

            // clear busy bit on i2c complete event as slave cpu tx/rx
            i2c->info->Status.busy = 0;
        }
        // I2C_DMA mode
        else
        {
            if (i2c->info->Driver_State & I2C_DRV_SLAVE_TX)
                    {
                // I2C Disable the Byte Transmit Interrupt in the Interrupt Enable Register
//                Tmp_C = i2c->reg->REG_INTEN.all;
//                Tmp_C &= (~ BITSET_0X014_BYTETRANS);
//                i2c->reg->REG_INTEN.all = Tmp_C;

                // keypoint for middleware to query
                i2c->info->Xfered_Data_Wt_Ptr = dma_channel_get_count(
                        i2c->dma_tx->channel);
                i2c->info->Xfer_Cmpl_Count = dma_channel_get_count(
                        i2c->dma_tx->channel);

                dma_channel_disable(i2c->dma_tx->channel, 1);

                // clear & set driver state to slave tx complete
                i2c->info->Driver_State = I2C_DRV_SLAVE_TX_CMPL;

                // clear busy bit on i2c complete event as slave dma tx
                i2c->info->Status.busy = 0;
            }

            if (i2c->info->Driver_State & I2C_DRV_SLAVE_RX)
                    {
                // keypoint for middleware to query
                i2c->info->Xfer_Cmpl_Count = dma_channel_get_count(
                        i2c->dma_rx->channel);

                // abort dma channel since MAX_XFER_SZ-read and expect complete in cmpl_handler
                dma_channel_disable(i2c->dma_rx->channel, 1);

                // clear & set driver state to slave rx complete
                i2c->info->Driver_State = I2C_DRV_SLAVE_RX_CMPL;

                // clear busy bit on i2c complete event as slave dma rx since read MAX_XFER_SZ
                i2c->info->Status.busy = 0;
            }
        }

        // I2C clear fifo first to prevent mistake i2cx_slave_fifo_read()
        // if the Completion Interrupt asserts, clear the FIFO and go next transaction.
        Tmp_C = i2c->reg->REG_CMD.all;
        Tmp_C &= (~ BITSET_0X028_CMD_MASK);
        Tmp_C |= (BITSET_0X028_CMD_CLEAR_FIFO);
        i2c->reg->REG_CMD.all = Tmp_C;
    }

    // i2c->info->Status.busy = 0;
}

static void
i2cx_signalevent(uint32_t event, I2C_RESOURCES* i2c)
{
    // invoke callback function
    if (i2c->info->Sig_Evt_CB)
    {
        i2c->info->Sig_Evt_CB(event, i2c->info->workspace);
    }
}
void
i2c_irq_handler_cbl(const struct device *dev,I2C_RESOURCES* i2c)
{
    uint32_t Tmp_S = 0, Tmp_S1 = 0, Tmp_W = 0, Tmp_C = 0, Evt_Id = 0;
    struct i2c_csk_data *data = DEV_DATA(dev);
    Tmp_W = Tmp_S = i2c->reg->REG_STATUS.all;
    Tmp_S1 = i2c->reg->REG_SETUP.all;
    Tmp_C = i2c->reg->REG_CTRL.all;

    // write 1 clear for those interrupts be able to W1C
    i2c->reg->REG_STATUS.all = (Tmp_W & BITSET_0X018_STATUS_CLEAR);

    if (Tmp_S & BITSET_0X018_CMPL)
    {
        // as i2c irq hit, quick store hw already-tranfered data count
        i2c->info->Slave_Rx_Cmpl_Ctrl_Reg_Val = Tmp_C;

        i2c_cmpl_handler(i2c);

        Evt_Id |= CSK_I2C_EVENT_TRANSFER_DONE;
    }
    else
    {
        Evt_Id |= CSK_I2C_EVENT_TRANSFER_INCOMPLETE;
    }

    if (Tmp_S & BITSET_0X018_FIFO_EMPTY)
    {
        i2cx_fifo_empty_handler(i2c);
    }

    if (Tmp_S & BITSET_0X018_FIFOFULL)
    {
        // as i2c irq hit, quick store hw already-tranfered data count
        i2c->info->Slave_Rx_Cmpl_Ctrl_Reg_Val = Tmp_C;

        i2cx_fifo_full_handler(i2c);
    }

    // Here is the entry for slave mode driver to detect
    // slave RX/TX action depend on master TX/RX action.
    // Addr hit is W1C bit, so during payload transaction,
    // it is not set again.
    // A new I2C data transaction(start-addr-data-stop)
    if (Tmp_S & BITSET_0X018_ADDRHIT)
    {
        // slave mode
        if (!(Tmp_S1 & BITSET_0X02C_MASTER))
        {
            // cleared on start of next Slave operation
            i2c->info->Status.general_call = 0;

            if (((Tmp_C & BITSET_0X024_DIR) >> 8) == I2C_SLAVE_RX)
                    {
                i2c->info->Slave_Rx_Cmpl_Ctrl_Reg_Val = Tmp_C;

                // notify middleware to do slave rx action
                Evt_Id |= CSK_I2C_EVENT_SLAVE_RECEIVE;

                //clear & set driver state to slave rx before data transaction
                i2c->info->Driver_State = I2C_DRV_SLAVE_RX;
            }
            else if (((Tmp_C & BITSET_0X024_DIR) >> 8) == I2C_SLAVE_TX)
                    {
                // notify middleware to do slave tx action
                Evt_Id |= CSK_I2C_EVENT_SLAVE_TRANSMIT;

                //clear & set driver state to slave tx before data transaction
                i2c->info->Driver_State = I2C_DRV_SLAVE_TX;
            }

            // A new I2C data transaction(start-addr-data-stop)
            i2cx_slave_addr_hit_handler(dev,i2c);
        }
    }

    // for slave mode to complete slave tx
    if (!(Tmp_S & BITSET_0X018_ACK))
    {
        //slave mode
        if (!(Tmp_S1 & BITSET_0X02C_MASTER))
        {
            if (Tmp_S & BITSET_0X018_BYTEREVC)
            {

            }
            else if (Tmp_S & BITSET_0X018_BYTETRANS)
            {
                // notify middleware to do slave tx complete
                Evt_Id |= CSK_I2C_EVENT_ADDRESS_NACK;

                i2c->info->nack_assert = 1;
            }
        }
    }

    // Indicates that the address of the current
    // transaction is a general call address.
    // This status is only valid in slave mode.
    // A new I2C data transaction(start-addr-data-stop)
    if ((Tmp_S & BITSET_0X018_ADDRHIT) &&
            (Tmp_S & BITSET_0X018_GENCALL) &&
            // slave mode
            !(Tmp_S1 & BITSET_0X02C_MASTER))
            {
        // cleared on start of next Slave operation
        i2c->info->Status.general_call = 1;

        // I2C General Call slave address
        Evt_Id |= CSK_I2C_EVENT_GENERAL_CALL;
    }

    if ((Tmp_S & BITSET_0X018_ARBLOS) &&
            // master mode
            (Tmp_S1 & BITSET_0X02C_MASTER))
            {
        // cleared on start of next Master operation
        i2c->info->Status.arbitration_lost = 1;

        Evt_Id |= CSK_I2C_EVENT_ARBITRATION_LOST;
    }

    if ((Tmp_S & BITSET_0X018_BYTETRANS) &&
            // slave mode
            !(Tmp_S1 & BITSET_0X02C_MASTER))
            {
        // I2C clear fifo first to prevent mistake i2cx_slave_fifo_read()
        // if the Completion Interrupt asserts, clear the FIFO and go next transaction.
        Tmp_C = i2c->reg->REG_CMD.all;
        Tmp_C &= (~ BITSET_0X028_CMD_MASK);
        Tmp_C |= (BITSET_0X028_CMD_CLEAR_FIFO);
        i2c->reg->REG_CMD.all = Tmp_C;
        
        // set on start of next slave operation,
        // cleared on slave tx 1 byte done or data transaction complete.
        i2c->info->Status.busy = 0;
        // printf("data->slave = %p\n",(uint32_t *)data->slave);
        // printf("11111 \n");
        // if(data->slave->callbacks->read_processed != NULL){
        //     printf("2222 \n");
        //     data->slave->callbacks->read_processed(data->slave,1);
        // }
    }

    i2cx_signalevent(Evt_Id, i2c);
}

static void *i2c_csk_get_ctx(const struct device *dev)
{
	I2C_RegDef *base = DEV_BASE(dev);
	if (base == VENUS_I2C0) {
		return I2C0();
	} else if (base == VENUS_I2C1) {
		return I2C1();
	} else {
		LOG_ERR("i2c reg base[%p] err, default: I2C0", base);
		return NULL;
	}
}

static void i2c_csk_event_cb(uint32_t event, void *user_arg)
{
	struct i2c_csk_data *data = (struct i2c_csk_data *)user_arg;

	k_event_post(data->evt, event);

	return;
}

static int i2c_csk_configure(const struct device *dev, uint32_t dev_config)
{
	struct i2c_csk_data *data = DEV_DATA(dev);
	void *I2C_Handler = DEV_HANDLER(dev);
	uint32_t arg = 0;
	int32_t r;

	if (!(I2C_MODE_MASTER & dev_config)) { /* slave mode */

		/* slave address 10bit*/
		if (I2C_ADDR_10_BITS & dev_config) {
			arg |= CSK_I2C_ADDRESS_10BIT;
		}

		/* use default slave address, user also can define slave adderss use dev_config*/
		arg |= I2C_SLAVE_ADDR;

		r = I2C_Control(I2C_Handler, CSK_I2C_OWN_ADDRESS, arg);

		/* interrupt mode */
		r = I2C_Control(I2C_Handler, CSK_I2C_TRANSMIT_MODE, 0); /* 0: interrupt 1: DMA */

		/* slave mode */
		LOG_DBG("slave mode");
		data->mode = false;

	} else { /* master mode */

		/* master mode support both 7bit and 10bit slave address*/

		/* bus speed */
		switch (I2C_SPEED_GET(dev_config)) {
		case I2C_SPEED_STANDARD:
			arg = CSK_I2C_BUS_SPEED_STANDARD;
			break;
		case I2C_SPEED_FAST:
			arg = CSK_I2C_BUS_SPEED_FAST;
			break;
		case I2C_SPEED_FAST_PLUS:
			arg = CSK_I2C_BUS_SPEED_FAST_PLUS;
			break;
		case I2C_SPEED_HIGH: /* High speed is not supported. */
			return -ENOTSUP;
		case I2C_SPEED_ULTRA:
			return -ENOTSUP; /* 5 MHz not support */
		default:
			return -EINVAL;
		}
		r = I2C_Control(I2C_Handler, CSK_I2C_BUS_SPEED, arg);

		/* interrupt mode */
		r |= I2C_Control(I2C_Handler, CSK_I2C_TRANSMIT_MODE, 0); /* 0: interrupt 1: DMA */

		/* Clear bus */
		r |= I2C_Control(I2C_Handler, CSK_I2C_BUS_CLEAR, 0);

		/* master mode */
		data->mode = true;
	}

	if (r != 0) {
		LOG_ERR("I2C_Control failed. r:%d", r);
		return -EIO;
	}

	return 0;
}

static int i2c_csk_transfer(const struct device *dev, struct i2c_msg *msgs, uint8_t num_msgs,
			    uint16_t addr)
{
	struct i2c_csk_data *data = DEV_DATA(dev);
	void *I2C_Handler = DEV_HANDLER(dev);
	uint32_t ret = 0;
	uint32_t evt;

	if (!num_msgs) {
		return 0;
	}

	/* attenton: one i2c transfer(trnsmit or receive) can transfer max 256 bytes(address len + data len), 
	which means that if address length is 1 byte, then max data length is 255 bytes*/
	if (data->mode) {
		for (int i = 0; i < num_msgs; i++) {
			I2C_RESOURCES* i2c = (I2C_RESOURCES*)I2C_Handler;

			if (msgs->len > I2C_XFER_MAX_NUM) {
				LOG_ERR("The maximum transfer size is 255 bytes");
				return -EINVAL;
			}

			if (i2c->reg->REG_STATUS.bit.LINESDA == 0) {
				LOG_WRN("i2c(%p) sda line is low.", i2c->reg);
			}

			k_event_set(data->evt, 0);

			if ((msgs->flags & I2C_MSG_RW_MASK) == I2C_MSG_WRITE) {
				ret = I2C_MasterTransmit(I2C_Handler, addr, msgs->buf, msgs->len,
							 (msgs->flags & I2C_MSG_STOP) !=
								 I2C_MSG_STOP);
			} else {
				ret = I2C_MasterReceive(I2C_Handler, addr, msgs->buf, msgs->len,
							(msgs->flags & I2C_MSG_STOP) !=
								I2C_MSG_STOP);
			}

			if (ret != CSK_DRIVER_OK) {
				LOG_ERR("i2c(%p) master transfer error: %d\n", i2c->reg, ret);
				I2C_Control(I2C_Handler, CSK_I2C_ABORT_TRANSFER, 0);
				ret = -EIO;
				break;
			}

			evt = k_event_wait(data->evt,
					   CSK_I2C_EVENT_TRANSFER_DONE |
						   CSK_I2C_EVENT_ARBITRATION_LOST,
					   false, K_MSEC(I2C_CSK6_MAX_TIME_OUT_MS));
			if (!(evt & CSK_I2C_EVENT_TRANSFER_DONE)) {
				LOG_ERR("i2c(%p) Wait complete time out, event:%x, sda line:%d.",
					i2c->reg, evt, i2c->reg->REG_STATUS.bit.LINESDA);

				if (evt & CSK_I2C_EVENT_ARBITRATION_LOST) {
					LOG_ERR("i2c(%p) lost arbitration", i2c->reg);
				}

				return -ETIMEDOUT;
			}

			/* This register will be decreased by one for each byte transmitted */
			if (i2c->reg->REG_CTRL.bit.DATACNT != 0) {
				LOG_ERR("i2c(%p) data transfer error, status:%d/%d, last ack:%d",
					i2c->reg, msgs->len - i2c->reg->REG_CTRL.bit.DATACNT,
					msgs->len, !i2c->reg->REG_STATUS.bit.ACK);
				return -EIO;
			}
			msgs++;
		}
	} else {
		/* slave mode only support basic i2c write and read, which means that i2c read only read data, 
		not opearte after i2c write */

		for (int i = 0; i < num_msgs; i++) {
			/* master and slave mode both support 10-bit address and 7-bit address */

			/* I2c write or read */
			if ((msgs->flags & I2C_MSG_RW_MASK) == I2C_MSG_WRITE) {
				/* wait until host controller start receive*/
				evt = k_event_wait(data->evt, CSK_I2C_EVENT_SLAVE_TRANSMIT, true,
						   K_MSEC(I2C_CSK6_MAX_TIME_OUT_MS));
				if (!(evt & CSK_I2C_EVENT_SLAVE_TRANSMIT)) {
					LOG_ERR("Wait I2C complete time out.");
					return -ETIMEDOUT;
				}
				/* start slave transmit, one by one */
				int index = 0;
				for (index = 0; index < msgs->len; index++) {
					/* master receive byte count must less or equal than 256 bytes, because driver max transfer limit; in case
					index is bigger than msgs->len, user msgs->len must bigger or equal than master receive byte counter */
					I2C_SlaveTransmit(I2C_Handler, &msgs->buf[index], 1);

					/* wait until TRANSFER_INCOMPLETE, or TRANSFER_COMPLETE */
					evt = k_event_wait(
						data->evt,
						CSK_I2C_EVENT_TRANSFER_DONE |
							CSK_I2C_EVENT_TRANSFER_INCOMPLETE,
						true, K_MSEC(I2C_CSK6_MAX_TIME_OUT_MS));

					if (!(evt & (CSK_I2C_EVENT_TRANSFER_DONE |
						     CSK_I2C_EVENT_TRANSFER_INCOMPLETE))) {
						LOG_ERR("Wait I2C complete time out.");
						return -ETIMEDOUT;
					}
				}

			} else {
				/* Detect trigger slave receive interrupt and  master transfer done */
				evt = k_event_wait_all(data->evt, CSK_I2C_EVENT_TRANSFER_DONE | CSK_I2C_EVENT_SLAVE_RECEIVE, true,
						   K_MSEC(I2C_CSK6_MAX_TIME_OUT_MS));
				if (!(evt & (CSK_I2C_EVENT_TRANSFER_DONE | CSK_I2C_EVENT_SLAVE_RECEIVE))) {
					LOG_ERR("Wait I2C complete time out.");
					return -ETIMEDOUT;
				}

				/* get slave received bufer byte cnt */
				int cnt = I2C_GetDataCount(I2C_Handler);
				LOG_DBG("slave recevie data cnt:%d\n", cnt);

				/* fetch received data from driver, user msgs->len must bigger or equal than master send byte count;
					the size of data received from master maybe not equal to user msgs->len */
				int len = (msgs->len <= cnt) ? msgs->len : cnt;
				I2C_SlaveReceive(I2C_Handler, msgs->buf, len);
			}

			/* Move to the next message */
			msgs++;
		}
	}

	return ret;
}

static int i2c_csk_init(const struct device *dev)
{
	const struct i2c_csk_config *config = DEV_CFG(dev);
	struct i2c_csk_data *data = DEV_DATA(dev);

	/* Get device handle */
	data->handle = i2c_csk_get_ctx(dev);
	if (data->handle == NULL) {
		return -EIO;
	}
	void *I2C_Handler = data->handle;

	/* Initialize i2c */
	I2C_Initialize(I2C_Handler, i2c_csk_event_cb, data);
	I2C_PowerControl(I2C_Handler, CSK_POWER_FULL);
	i2c_csk_configure(I2C_Handler, I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_MASTER);
    
    // data->slave->callbacks->read_processed = NULL;
	pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);
	/* IRQ_CONNECT and irq_enable */
	config->irq_config_func(dev);

	return 0;
}

static void i2c_csk_isr(const struct device *dev)
{
	extern void i2c_irq_handler(I2C_RESOURCES * i2c);

	void *I2C_Handler = DEV_HANDLER(dev);
	// i2c_irq_handler(I2C_Handler);
	i2c_irq_handler_cbl(dev,I2C_Handler);
}

static const struct i2c_driver_api i2c_csk_driver_api = {
	.configure = i2c_csk_configure,
	.transfer = i2c_csk_transfer,
	.slave_register = NULL,
	.slave_unregister = NULL,
	.recover_bus = NULL,
};

#define I2C_DEVICE_INIT_CSK(n)                                                                     	\
	static void i2c_csk_config_func_##n(const struct device *dev);                             		\
	PINCTRL_DT_INST_DEFINE(n);                                                                  \
                                                                                                   	\
	static const struct i2c_csk_config i2c_csk_config_##n = {                                  		\
		.base = (I2C_RegDef *)DT_INST_REG_ADDR(n),                                         \
		.irq_config_func = i2c_csk_config_func_##n,                                        \
		.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),      								   \
	};                	                                                                         \
                                                                                                   \
	K_EVENT_DEFINE(csk6_i2c_event_##n);															\
	static struct i2c_csk_data i2c_csk_data_##n = { .evt = &csk6_i2c_event_##n };                       \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, &i2c_csk_init, NULL, &i2c_csk_data_##n, &i2c_csk_config_##n,      \
			      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                     \
			      &i2c_csk_driver_api);                                                \
                                                                                                   \
	static void i2c_csk_config_func_##n(const struct device *dev)                              \
	{                                                                                          \
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority), i2c_csk_isr,                \
			    DEVICE_DT_INST_GET(n), 0);                                             \
                                                                                                   \
		irq_enable(DT_INST_IRQN(n));                                                       \
	}

DT_INST_FOREACH_STATUS_OKAY(I2C_DEVICE_INIT_CSK)
