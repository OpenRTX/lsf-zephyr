#include "tp_ts.h"
#include "bl_fw.h"


#ifdef CTP_USE_HW_I2C
static int bl_i2c_transfer(unsigned char i2c_addr, unsigned char *buf, int len,unsigned char rw)
{
	int ret;
    switch(rw)
    {
        case I2C_WRITE:
			ret = CTP_FLASH_I2C_WRITE(i2c_addr, buf, len);
			break;
		case I2C_READ:
			ret = CTP_FLASH_I2C_READ(i2c_addr, buf, len);
			break;			
    }
	if(ret<0){
		bl_log_trace("bl_i2c_transfer:i2c transfer error___\n");
		return -1;
	}

	return 0;
}

static int bl_read_fw(unsigned char i2c_addr,unsigned char reg_addr, unsigned char *buf, int len)
{
	int ret;

	ret = CTP_FLASH_I2C_WRITE(i2c_addr, &reg_addr, 1);
	UDELAY(100);
	if(ret<0)
       {
		goto IIC_COMM_ERROR;
       }
	ret = CTP_FLASH_I2C_READ(i2c_addr, buf, len);
	if(ret<0)
       {
		goto IIC_COMM_ERROR;
       }	

IIC_COMM_ERROR:	
	if(ret<0){
		bl_log_trace("bl_read_fw:i2c transfer error___\n");
		return -1;
	}		
	return 0;
}
#endif

#ifdef BL_DEBUG_SUPPORT
void bl_debug_for_touch(unsigned char* debugInfo)
{
	unsigned int i = 0;
	short freq =0x00;
	short maxNoise = 0x00;
	short minNoise = 0x00;
	//bl_log_trace("debugData start:\n");
	freq = (debugInfo[0]<<8)|debugInfo[1];
	maxNoise = (debugInfo[2]<<8)|debugInfo[3];
	minNoise = (debugInfo[4]<<8)|debugInfo[5];
	bl_log_trace("freq = %d maxNoise = %d minNoise = %d\n",freq,maxNoise,minNoise);
	//bl_log_trace("debugData end\n");
}
#endif

#ifdef BL_ESD_PROTECT
void bl_ts_esd_recovery(void)
{
    #ifdef BL_POWER_CONTROL_SUPPORT
	    bl_ts_reset_powerup();
	#else
	    #ifdef RESET_PIN_WAKEUP
		    bl_ts_reset_wakeup();
		#endif
        #ifdef INT_PIN_WAKEUP
		    bl_ts_int_wakeup();
        #endif
	#endif
}
void bl_esd_check_handler(void)
{
	int ret = 0;
	static unsigned char count = 0;
	unsigned char cmdIicCheck = 0x01;
	unsigned char cmdEsdCheck = 0xf9;
	unsigned char bufIicCheck = 0;
	static unsigned char bufEsdCheck[4] = {0x0,0x0,0x0,0x0};
	static unsigned char bufEsdCheckBak[4] = {0x0,0x0,0x0,0x0};
	unsigned char esdRecoveryFlag = 0;
	bl_log_trace("bl_esd_check_handler start\n");
	ret = bl_read_fw(CTP_SLAVE_ADDR, cmdIicCheck, &bufIicCheck,1);
	if(ret<0)
    {
        bl_log_trace("bl_esd_check_handler:iic check error\n");
		esdRecoveryFlag = 1;
    }
	
	if(!esdRecoveryFlag)
    {
        ret = bl_read_fw(CTP_SLAVE_ADDR, cmdEsdCheck, bufEsdCheck,sizeof(bufEsdCheck));
		if(ret < 0)
        {
            esdRecoveryFlag = 1;
        }
    }

	count++;
    bl_log_trace("bl_esd_check_handler:esdRecoveryFlag = %x,bufIicCheck = %x;count = %x,bufEsdCheck:%x %x %x %x:%x %x %x %x\n",esdRecoveryFlag,bufIicCheck,bufIicCheck,
		bufEsdCheck[0],bufEsdCheck[1],bufEsdCheck[2],bufEsdCheck[3],bufEsdCheckBak[0],bufEsdCheckBak[1],bufEsdCheckBak[2],bufEsdCheckBak[3]);
	if((esdRecoveryFlag == 1)
		||(bufIicCheck == 0x99)
		||(bufIicCheck == 0x1)
		||(bufIicCheck == 0x97)
		||(((count > 1)&&(memcmp(bufEsdCheck,bufEsdCheckBak,sizeof(bufEsdCheck))))))
	{
		bl_ts_esd_recovery();
	}

    memcpy(bufEsdCheckBak,bufEsdCheck,sizeof(bufEsdCheck));
    
    if(count>200)
    {
    	count = 2;	
    }
		
	bl_log_trace("bl_esd_check_handler end\n");
}
#endif

int bl_soft_reset_switch_int_wakemode()
{
    unsigned char cmd[4];
	int ret = 0x00;

	cmd[0] = RW_REGISTER_CMD;
	cmd[1] = ~cmd[0];
	cmd[2] = CHIP_ID_REG;
    cmd[3] = 0xe8;
	
	printk("%s:[%d]\n", __func__, __LINE__);
	ret = bl_i2c_transfer(CTP_SLAVE_ADDR, cmd,4,I2C_WRITE);
	printk("%s:[%d]\n", __func__, __LINE__);
	if(ret < 0){
		bl_log_trace("bl_soft_reset_switch_int_wakemode failed:i2c write flash error___\n");
	}
	
    return ret;
}
#if (UPDATE_MODE==I2C_UPDATE_MODE)
void bl_enter_update_with_i2c(void)
{
    unsigned char cmd[200] = {0x00};
	int i = 0;
	int ret = 0;

	for(i = 0; i < sizeof(cmd); i += 2)
    {
        cmd[i] = 0x5a;
		cmd[i + 1] = 0xa5;
    }

	ret = bl_i2c_transfer(CTP_SLAVE_ADDR, cmd, sizeof(cmd), I2C_WRITE);
	if(ret < 0)
    {
        bl_log_trace("bl_enter_update_with_i2c failed:send 5a a5 error___\n");
        goto error;
    }
	MDELAY(50);

error:
	return ;
}

void bl_exit_update_with_i2c(void)
{
    int ret = 0;
	unsigned char cmd[2] = {0x5a, 0xa5};
    MDELAY(20);
	ret = bl_i2c_transfer(CTP_SLAVE_ADDR, cmd, sizeof(cmd), I2C_WRITE);
	if(ret < 0)
    {
        bl_log_trace("bl_exit_update_with_i2c failed:send 5a a5 error___\n");
        goto error;
    }
	MDELAY(20);
	#if defined(RESET_PIN_WAKEUP)
	bl_ts_reset_wakeup();
	#endif
error:
	return;
}
#endif

#if (UPDATE_MODE==INT_UPDATE_MODE)
void bl_enter_update_with_int(void)
{
    bl_ts_set_intmode(0);
    bl_ts_set_intup(0);

	printk("%s:[%d]\n", __func__, __LINE__);
    bl_soft_reset_switch_int_wakemode();

	printk("%s:[%d]\n", __func__, __LINE__);
	MDELAY(50);
}

void bl_exit_update_with_int(void)
{
	printk("%s:[%d]\n", __func__, __LINE__);
    MDELAY(20);
	bl_ts_set_intup(1);
	MDELAY(20);
	bl_ts_set_intmode(1);
	#if defined(RESET_PIN_WAKEUP)
	bl_ts_reset_wakeup();
	#endif
}
#endif

int bl_get_chip_id(unsigned char *buf)
{

	unsigned char cmd[3];
	int ret = 0x00;
		
	bl_log_trace("bl_get_chip_id\n");

	cmd[0] = RW_REGISTER_CMD;
	cmd[1] = ~cmd[0];
	cmd[2] = CHIP_ID_REG;

	ret = bl_i2c_transfer(BL_FLASH_I2C_ADDR, cmd,3,I2C_WRITE);
	if(ret < 0){
		bl_log_trace("bl_get_chip_id:i2c write flash error___\n");
		goto GET_CHIP_ID_ERROR;
	}
	
	ret = bl_i2c_transfer(BL_FLASH_I2C_ADDR, buf,1,I2C_READ); 
	if(ret < 0){
		bl_log_trace("bl_get_chip_id:i2c read flash error___\n");
		goto GET_CHIP_ID_ERROR;
	}

GET_CHIP_ID_ERROR:
	return ret;
}

int bl_get_prj_id(unsigned char *buf)
{
	bl_log_trace("bl_get_prj_id\n");
	return bl_read_fw(CTP_SLAVE_ADDR,BL_PRJ_ID_REG, buf, 1);    
}

int bl_get_fwArgPrj_id(unsigned char *buf)
{
	bl_log_trace("bl_get_fwArgPrj_id\n");
	return bl_read_fw(CTP_SLAVE_ADDR,BL_FWVER_PJ_ID_REG, buf, 3);
}

#ifdef BL_UPDATE_FIRMWARE_ENABLE
static int bl_get_protect_flag(void)
{
    unsigned char ret = 0;
    unsigned char protectFlag = 0x00;
	bl_log_trace("bl_get_protect_flag\n");
	ret = bl_read_fw(CTP_SLAVE_ADDR,BL_PROTECT_REG, &protectFlag, 1);
	if(ret < 0)
    {
    	bl_log_trace("bl_get_protect_flag failed,ret = %x\n",ret);
		return 0;
    }
	if(protectFlag == 0x55)
    {
        bl_log_trace("bl_get_protect_flag:protectFlag = %x\n",protectFlag);
		return 1;
    }
	return 0;
}

static int bl_get_specific_argument(unsigned int *arguOffset, unsigned char *cobID, unsigned char* fw_data, unsigned int fw_size, unsigned char arguCount)
{
    unsigned char convertCobId[12] = {0x00};
    unsigned char i = 0;
    unsigned int cobArguAddr = fw_size - arguCount * BL_ARGUMENT_FLASH_SIZE;
    bl_log_trace("fw_size is %x\n", fw_size);
    bl_log_trace("arguCount is %d\n", arguCount);
    bl_log_trace("cobArguAddr is %x\n",cobArguAddr);
	
    for(i = 0; i < sizeof(convertCobId); i++)
    {		
        if(i%2)
        {
            convertCobId[i] = cobID[i/2] & 0x0f;
        }		
        else
        {
            convertCobId[i] = (cobID[i/2] & 0xf0) >> 4;
        }
        bl_log_trace("before convert:convertCobId[%d] is %x\n",i,convertCobId[i]);
        if(convertCobId[i] < 10)
        {
            convertCobId[i] = '0' + convertCobId[i];
        }
        else
        {
            convertCobId[i] = 'a' + convertCobId[i] - 10;
        }
        bl_log_trace("after convert:convertCobId[%d] is %x\n",i,convertCobId[i]);
    }
	
    bl_log_trace("convertCobId is:\n");
    for(i= 0; i < 12; i++)
    {
        bl_log_trace("%x  ", convertCobId[i]);
    }
    bl_log_trace("\n");
	
    for(i = 0; i < arguCount; i++)
    {
        if(memcmp(convertCobId, fw_data + cobArguAddr + i * BL_ARGUMENT_FLASH_SIZE + BL_COB_ID_OFFSET, 12))
        {
            bl_log_trace("This argu is not the specific argu\n");
        }
        else
        {
            *arguOffset = cobArguAddr + i * BL_ARGUMENT_FLASH_SIZE;
            bl_log_trace("This argu is the specific argu, and arguOffset is %x\n",*arguOffset);
            break;
        }
    }

	if(i == arguCount)
    {
        *arguOffset = BL_ARGUMENT_BASE_OFFSET;
        return -1;
    }
	else
    {
        return 0;
    }
}

static unsigned char bl_get_argument_count(unsigned char* fw_data, unsigned int fw_size)
{
    unsigned char i = 0;
	unsigned addr = 0;
	addr = fw_size;
	bl_log_trace("addr is %x\n", addr);
	while(addr > (BL_ARGUMENT_BASE_OFFSET + BL_ARGUMENT_FLASH_SIZE))
    {
        addr = addr - BL_ARGUMENT_FLASH_SIZE;
        if(memcmp(fw_data+addr, ARGU_MARK, sizeof(ARGU_MARK) - 1))
        {
            bl_log_trace("arguMark found flow complete");
            break;		
        }
        else
        {
            i++;
            bl_log_trace("arguMark founded\n");
        }
    }
    bl_log_trace("The argument count is %d\n",i);
    return i;
}

static unsigned int bl_get_cob_project_down_size_arguCnt(unsigned char* fw_data,unsigned int fw_size, unsigned char *arguCnt)
{
    unsigned int downSize = 0;

	*arguCnt = bl_get_argument_count(fw_data,fw_size);
	downSize = fw_size - (*arguCnt) * BL_ARGUMENT_FLASH_SIZE - FLASH_PAGE_SIZE;
	return downSize;
}

static unsigned char bl_is_cob_project(unsigned char* fw_data, int fw_size)
{
    unsigned char arguKey[4] = {0xaa,0x55,0x09,0x09};
	unsigned char* pfw;

	pfw = fw_data+fw_size-4;
	
    if(fw_size%FLASH_PAGE_SIZE)
    {
        return 0;
    }
	else
    {
	    if(memcmp(arguKey,pfw,4))
	    {
	        return 0;
	    }
        else
        {
            return 1;
        }
    }
}

static int bl_get_cob_id(unsigned char *buf)
{
	bl_log_trace("bl_get_cob_id\n");
	return bl_read_fw(CTP_SLAVE_ADDR,COB_ID_REG, buf, 6);
}

static int bl_get_fw_checksum(unsigned short *fw_checksum)
{
	unsigned char buf[3];
	unsigned char checksum_ready = 0;
	int retry = 5;
	int ret = 0x00;

	bl_log_trace("bl_get_fw_checksum\n");

	buf[0] = CHECKSUM_CAL_REG;
	buf[1] = CHECKSUM_CAL;
	ret = bl_i2c_transfer(CTP_SLAVE_ADDR, buf,2,I2C_WRITE);
	if(ret < 0){
		bl_log_trace("bl_get_fw_checksum:write checksum cmd error___\n");
		return -1;
	}
	MDELAY(FW_CHECKSUM_DELAY_TIME);
	
	ret = bl_read_fw(CTP_SLAVE_ADDR,CHECKSUM_REG, buf, 3);
	if(ret < 0){
		bl_log_trace("bl_get_fw_checksum:read checksum error___\n");
		return -1;
	}

	checksum_ready = buf[0];

	while((retry--) && (checksum_ready != CHECKSUM_READY)){

		MDELAY(50);
		ret = bl_read_fw(CTP_SLAVE_ADDR,CHECKSUM_REG, buf, 3);
		if(ret < 0){
			bl_log_trace("bl_get_fw_checksum:read checksum error___\n");
			return -1;
		}

		checksum_ready = buf[0];
	}
	
	if(checksum_ready != CHECKSUM_READY){
		bl_log_trace("bl_get_fw_checksum:read checksum fail___\n");
		return -1;
	}
	*fw_checksum = (buf[1]<<8)+buf[2];

	return 0;
}

static void bl_get_fw_bin_checksum_for_self_ctp(unsigned char *fw_data,unsigned short *fw_bin_checksum, int fw_size, int specifyArgAddr)
{
	int i = 0;
	int temp_checksum = 0x0;

    for(i = 0; i < BL_ARGUMENT_BASE_OFFSET; i++)
    {
        temp_checksum += fw_data[i];
    }
    for(i = specifyArgAddr; i < specifyArgAddr + VERTIFY_START_OFFSET; i++)
    {
    	temp_checksum += fw_data[i];
    }
    for(i = specifyArgAddr + VERTIFY_START_OFFSET; i < specifyArgAddr + VERTIFY_START_OFFSET + 4; i++)
    {
    	temp_checksum += fw_data[i];
    }
    for(i = BL_ARGUMENT_BASE_OFFSET + VERTIFY_START_OFFSET + 4; i < fw_size; i++)
    {
    	temp_checksum += fw_data[i];
    }

    for(i = fw_size; i < MAX_FLASH_SIZE; i++)
    {
    	temp_checksum += 0xff;
    }
    
    *fw_bin_checksum = temp_checksum & 0xffff;
}

static void bl_get_fw_bin_checksum_for_compatible_ctp(unsigned char *fw_data,unsigned short *fw_bin_checksum, int fw_size)
{
	int i = 0;
	int temp_checksum = 0x0;

    for(i = 0; i < fw_size; i++)
    {
    	temp_checksum += fw_data[i];
    }
    for(i = fw_size; i < MAX_FLASH_SIZE; i++)
    {
    	temp_checksum += 0xff;
    }
    
    *fw_bin_checksum = temp_checksum & 0xffff;
}

static int bl_erase_flash(void)
{
	unsigned char cmd[2];
	
	bl_log_trace("bl_erase_flash\n");
	
	cmd[0] = ERASE_ALL_MAIN_CMD; 
	cmd[1] = ~cmd[0];

	return bl_i2c_transfer(BL_FLASH_I2C_ADDR,cmd, 0x02,I2C_WRITE);	
}

static int bl_write_flash(unsigned char cmd, int flash_start_addr, unsigned char *buf, int len)
{
	unsigned char cmd_buf[6+FLASH_WSIZE];
	unsigned short flash_end_addr;
	int ret;
		
	bl_log_trace("bl_write_flash\n");
	
	if(!len){
		bl_log_trace("___write flash len is 0x00,return___\n");
		return -1;	
	}

	flash_end_addr = flash_start_addr + len -1;

	if(flash_end_addr >= MAX_FLASH_SIZE){
		bl_log_trace("___write flash end addr is overflow,return___\n");
		return -1;	
	}

	cmd_buf[0] = cmd;
	cmd_buf[1] = ~cmd;
	cmd_buf[2] = flash_start_addr >> 0x08;
	cmd_buf[3] = flash_start_addr & 0xff;
	cmd_buf[4] = flash_end_addr >> 0x08;
	cmd_buf[5] = flash_end_addr & 0xff;

	memcpy(&cmd_buf[6],buf,len);

	ret = bl_i2c_transfer(BL_FLASH_I2C_ADDR,cmd_buf, len+6,I2C_WRITE);	
	if(ret < 0){
		bl_log_trace("i2c transfer error___\n");
		return -1;
	}

	return 0;
}

static int bl_read_flash(unsigned char cmd, int flash_start_addr, unsigned char *buf, int len)
{
	char ret =0;
	unsigned char cmd_buf[6];
	unsigned short flash_end_addr;

	flash_end_addr = flash_start_addr + len -1;
	cmd_buf[0] = cmd;
	cmd_buf[1] = ~cmd;
	cmd_buf[2] = flash_start_addr >> 0x08;
	cmd_buf[3] = flash_start_addr & 0xff;
	cmd_buf[4] = flash_end_addr >> 0x08;
	cmd_buf[5] = flash_end_addr & 0xff;
	ret = bl_i2c_transfer(BL_FLASH_I2C_ADDR,cmd_buf,6,I2C_WRITE);
	if(ret < 0)
	{
	    bl_log_trace("bl_read_flash:i2c transfer write error\n");
		return -1;
	}
	ret = bl_i2c_transfer(BL_FLASH_I2C_ADDR,buf,len,I2C_READ);
	if(ret < 0)
	{
	    bl_log_trace("bl_read_flash:i2c transfer read error\n");
		return -1;
	}

	return 0;
}

static int bl_download_fw_self_ctp(unsigned char *pfwbin,int specificArgAddr, int fwsize)
{
    unsigned int i;
    unsigned short size,len;
    unsigned short addr;
    unsigned char verifyBuf[4] = {0xff, 0xff, 0xff, 0xff};
    bl_log_trace("bl_download_fw\n");
    
    verifyBuf[2] = pfwbin[BL_ARGUMENT_BASE_OFFSET+VERTIFY_START_OFFSET+2];
    verifyBuf[3] = pfwbin[BL_ARGUMENT_BASE_OFFSET+VERTIFY_START_OFFSET+3]; 
    bl_log_trace("bl_download_fw:verifyBuf = %x %x %x %x\n",verifyBuf[0],verifyBuf[1],verifyBuf[2],verifyBuf[3]);
    if(bl_erase_flash()){
    	bl_log_trace("___erase flash fail___\n");
    	return -1;
    }
    
    MDELAY(50);
    
    //Write data before BL_ARGUMENT_BASE_OFFSET
    for(i=0;i< BL_ARGUMENT_BASE_OFFSET;)
    {
    	size = BL_ARGUMENT_BASE_OFFSET - i;
    	if(size > FLASH_WSIZE){
    		len = FLASH_WSIZE;
    	}else{
    		len = size;
    	}
    
    	addr = i;
    
    	if(bl_write_flash(WRITE_MAIN_CMD,addr, &pfwbin[i],len)){
    		return -1;
    	}
    	i += len;
    	MDELAY(5);
    }
    
    //Write the data from BL_ARGUMENT_BASE_OFFSET to VERTIFY_START_OFFSET
    for(i=BL_ARGUMENT_BASE_OFFSET;i< (VERTIFY_START_OFFSET+BL_ARGUMENT_BASE_OFFSET);)
    {
    	size = VERTIFY_START_OFFSET + BL_ARGUMENT_BASE_OFFSET - i;
    	if(size > FLASH_WSIZE){
    		len = FLASH_WSIZE;
    	}else{
    		len = size;
    	}
    
    	addr = i;
    
    	if(bl_write_flash(WRITE_MAIN_CMD,addr, &pfwbin[i+specificArgAddr-BL_ARGUMENT_BASE_OFFSET],len)){
    		return -1;
    	}
    	i += len;
    	MDELAY(5);
    }
    
    //Write the four bytes verifyBuf from VERTIFY_START_OFFSET
    for(i=(VERTIFY_START_OFFSET + BL_ARGUMENT_BASE_OFFSET);i< (VERTIFY_START_OFFSET + BL_ARGUMENT_BASE_OFFSET + sizeof(verifyBuf));)
    {
    	size = VERTIFY_START_OFFSET + BL_ARGUMENT_BASE_OFFSET + sizeof(verifyBuf) - i;
    	if(size > FLASH_WSIZE){
    		len = FLASH_WSIZE;
    	}else{
    		len = size;
    	}
    
    	addr = i;
    
    	if(bl_write_flash(WRITE_MAIN_CMD,addr, &verifyBuf[i-VERTIFY_START_OFFSET-BL_ARGUMENT_BASE_OFFSET],len)){
    		return -1;
    	}
    	i += len;
    	MDELAY(5);
    }
    
    //Write data after verityBuf from VERTIFY_START_OFFSET + 4
    for(i=(BL_ARGUMENT_BASE_OFFSET + VERTIFY_START_OFFSET + 4);i< fwsize;)
    {
    	size = fwsize - i;
    	if(size > FLASH_WSIZE){
    		len = FLASH_WSIZE;
    	}else{
    		len = size;
    	}
    
    	addr = i;
    
    	if(bl_write_flash(WRITE_MAIN_CMD,addr, &pfwbin[i],len)){
    		return -1;
    	}
    	i += len;
    	MDELAY(5);
    }
    
    return 0;	
}

static int bl_download_fw_for_compatible_ctp(unsigned char *pfwbin, int fwsize)
{
	unsigned int i;
	unsigned short size,len;
	unsigned short addr;
	bl_log_trace("bl_download_fw_for_compatible_ctp\n");
	
	if(bl_erase_flash()){
		bl_log_trace("___erase flash fail___\n");
		return -1;
	}

	MDELAY(50);

    for(i = 0;i < fwsize;)
    {
    	size = fwsize - i;
    	if(size > FLASH_WSIZE){
    		len = FLASH_WSIZE;
    	}else{
    		len = size;
    	}
    
    	addr = i;
    
    	if(bl_write_flash(WRITE_MAIN_CMD,addr, &pfwbin[i],len)){
    		return -1;
    	}
    	i += len;
    	MDELAY(5);
    }

	return 0;	
}


static int bl_read_flash_vertify(void)
{
	unsigned char cnt = 0;
	int ret = 0;
	unsigned char vertify[2] = {0xaa,0x55};
	unsigned char vertify1[2] = {0};

	bl_log_trace("bl_read_flash_vertify start\n");
	
	SET_WAKEUP_LOW;
	while(cnt < 3)
	{
	    cnt++;
        ret = bl_read_flash(READ_MAIN_CMD, BL_ARGUMENT_BASE_OFFSET + VERTIFY_START_OFFSET, vertify1, sizeof(vertify1));
		if(ret < 0)
		{
			bl_log_trace("bl_write_flash_vertify: read fail\n");
			continue;
		}

		if(memcmp(vertify, vertify1, sizeof(vertify)) == 0)
		{
			ret = 0;
			break;
		}
		else
        {
            ret = -1;
        }
	}
	SET_WAKEUP_HIGH;
	return ret;
}

static int bl_write_flash_vertify(void)
{
	unsigned char cnt = 0;
	int ret = 0;
	unsigned char vertify[2] = {0xaa,0x55};
	unsigned char vertify1[2] = {0};

    bl_log_trace("bl_write_flash_vertify start\n");
	
	SET_WAKEUP_LOW;
	while(cnt < 3)
	{
	    cnt++;
	    ret = bl_write_flash(WRITE_MAIN_CMD, BL_ARGUMENT_BASE_OFFSET + VERTIFY_START_OFFSET, vertify, sizeof(vertify));
		if(ret < 0)
		{
			bl_log_trace("bl_write_flash_vertify: write fail\n");
			continue;
		}
		
		MDELAY(10);

        ret = bl_read_flash(READ_MAIN_CMD, BL_ARGUMENT_BASE_OFFSET + VERTIFY_START_OFFSET, vertify1, sizeof(vertify1));
		if(ret < 0)
		{
			bl_log_trace("bl_write_flash_vertify: read fail\n");
			continue;
		}

		if(memcmp(vertify, vertify1, sizeof(vertify)) == 0)
		{
			ret = 0;
			break;
		}
		else
        {
            ret = -1;
        }
	}
	SET_WAKEUP_HIGH;
	return ret;
}

static int bl_update_flash_for_self_ctp(unsigned char update_type, unsigned char *pfwbin,int fwsize, int specificArgAddr)
{
	int retry = 0;
	int ret = 0;
	unsigned short fw_checksum = 0x0;
	unsigned short fw_bin_checksum = 0x0;
	retry =3;
	while(retry--)
	{
		SET_WAKEUP_LOW;

	    ret = bl_download_fw_self_ctp(pfwbin,specificArgAddr,fwsize);

		if(ret<0)
		{
			bl_log_trace("bl fw update start bl_download_fw error retry=%d\n",retry);
			continue;
		}
		
		MDELAY(50);

		SET_WAKEUP_HIGH;
	
        bl_get_fw_bin_checksum_for_self_ctp(pfwbin,&fw_bin_checksum, fwsize,specificArgAddr);
        ret = bl_get_fw_checksum(&fw_checksum);
        fw_checksum -= 0xff;
        bl_log_trace("bl fw update end,fw checksum = 0x%x,fw_bin_checksum =0x%x\n",fw_checksum, fw_bin_checksum);

		
		if((ret < 0) || ((update_type == FW_ARG_UPDATE)&&(fw_checksum != fw_bin_checksum)))
		{
			bl_log_trace("bl fw update start bl_download_fw bl_get_fw_checksum error");
			continue;
		}

		if((update_type == FW_ARG_UPDATE)&&(fw_checksum == fw_bin_checksum))
		{
            ret = bl_write_flash_vertify();
			if(ret < 0)
				continue;
		}
		break;
	}

	if(retry < 0)
	{
		bl_log_trace("bl fw update error\n");
		return -1;
	}

	bl_log_trace("bl fw update success___\n");	

	return 0;
}

static int bl_update_flash_for_compatible_ctp(unsigned char update_type, unsigned char *pfwbin,int fwsize)
{
	int retry = 0;
	int ret = 0;
	unsigned short fw_checksum = 0x0;
	unsigned short fw_bin_checksum = 0x0;
	retry =3;
	while(retry--)
	{
		SET_WAKEUP_LOW;

	    ret = bl_download_fw_for_compatible_ctp(pfwbin, fwsize);

		if(ret<0)
		{
			bl_log_trace("bl fw update start bl_update_flash_for_compatible_ctp error retry=%d\n",retry);
			continue;
		}
		
		MDELAY(50);

		SET_WAKEUP_HIGH;		

	    bl_get_fw_bin_checksum_for_compatible_ctp(pfwbin,&fw_bin_checksum, fwsize);
	    ret = bl_get_fw_checksum(&fw_checksum);
		bl_log_trace("bl fw update end,fw checksum = 0x%x,fw_bin_checksum =0x%x\n",fw_checksum, fw_bin_checksum);
		
		if((ret < 0) || ((update_type == FW_ARG_UPDATE)&&(fw_checksum != fw_bin_checksum)))
		{
			bl_log_trace("bl fw update start bl_update_flash_for_compatible_ctp bl_get_fw_checksum error");
			continue;
		}

		break;
	}

	if(retry < 0)
	{
		bl_log_trace("bl fw update error\n");
		return -1;
	}

	bl_log_trace("bl fw update success___\n");	

	return 0;
}


static unsigned char choose_update_type_for_self_ctp(unsigned char isBlank, unsigned char* fw_data, unsigned char fwVer, unsigned char arguVer, unsigned short fwChecksum, unsigned short fwBinChecksum,int specifyArgAddr)
{
    unsigned char update_type = NONE_UPDATE;
    if(isBlank)
	{
		update_type = FW_ARG_UPDATE;
		bl_log_trace("Update case 0:FW_ARG_UPDATE\n");
	}
	else
	{
        if((fwVer != fw_data[specifyArgAddr + BL_FWVER_MAIN_OFFSET])
		    ||(arguVer != fw_data[specifyArgAddr + BL_FWVER_ARGU_OFFSET])
		    ||(fwChecksum != fwBinChecksum))
	    {
            update_type = FW_ARG_UPDATE;
		    bl_log_trace("Update case 1:FW_ARG_UPDATE\n");
	    }
	    else
	    {
            update_type = NONE_UPDATE;
            bl_log_trace("Update case 4:NONE_UPDATE\n");
	    }
	}
    return update_type;
}

static unsigned char choose_update_type_for_compatible_ctp(unsigned isBlank, unsigned char* fw_data, unsigned char prjID, unsigned short checksum)
{
    unsigned char update_type = NONE_UPDATE;
    if(isBlank)
	{
		update_type = FW_ARG_UPDATE;
		bl_log_trace("Update case 0:FW_ARG_UPDATE\n");
	}
	else
	{
        if((prjID != fw_data[PJ_ID_OFFSET])||(checksum == 0))
	    {
            update_type = FW_ARG_UPDATE;
		    bl_log_trace("Update case 1:FW_ARG_UPDATE\n");
	    }
	    else
	    {
            update_type = NONE_UPDATE;
            bl_log_trace("Update case 4:NONE_UPDATE\n");
	    }
	}
    return update_type;
}
static int bl_update_fw_for_self_ctp(unsigned char fileType,unsigned char* fw_data, int fw_size)
{
	unsigned char fwArgPrjID[3];    //firmware version/argument version/project identification
	int ret = 0x00;
	unsigned char isBlank = 0x0;    //Indicate the IC have any firmware
	unsigned short fw_checksum = 0x0;  //The checksum for firmware in IC
	unsigned short fw_bin_checksum = 0x0;  //The checksum for firmware in file
	unsigned char update_type = NONE_UPDATE;  
	unsigned int downSize = 0x0;       //The available size of firmware data in file 
	unsigned char cobID[6] = {0};           //The identification for COB project
	unsigned int specificArguAddr = BL_ARGUMENT_BASE_OFFSET;   //The specific argument base address in firmware date with cobID 
	unsigned char arguCount = 0x0;      //The argument count for COB firmware
	unsigned char IsCobPrj = 0;         //Judge the project type depend firmware file
	unsigned char projectFlag = 0;      //protect flag
    bl_log_trace("bl_update_fw_for_self_ctp start\n");
	
//check protect flag
    projectFlag = bl_get_protect_flag();
    if(bl_get_protect_flag() && (fileType == HEADER_FILE_UPDATE))
    {
        bl_log_trace("bl_update_fw_for_self_ctp:projectFlag = %x, fileType = %x", projectFlag, fileType);
		return 0;
    }
	
//Step 1:Obtain project type
    IsCobPrj = bl_is_cob_project(fw_data, fw_size);
    bl_log_trace("bl_update_fw_for_self_ctp:IsCobPrj = %x", IsCobPrj);

//Step 2:Obtain IC version number
	MDELAY(5);
	ret = bl_get_fwArgPrj_id(fwArgPrjID);
	if((ret < 0) 
		|| ((ret == 0) && (fwArgPrjID[0] == 0))
		|| ((ret == 0) && (fwArgPrjID[0] == 0xff)) 
		|| ((ret == 0) && (fwArgPrjID[0] == BL_FWVER_PJ_ID_REG))
		|| (bl_read_flash_vertify() < 0))
	{
	    isBlank = 1;
		bl_log_trace("bl_update_fw_for_self_ctp:This is blank IC ret = %x fwArgPrjID[0]=%x\n",ret,fwArgPrjID[0]);
    }
	else
    {
        isBlank = 0;
		bl_log_trace("bl_update_fw_for_self_ctp:ret=%x fwID=%x argID=%x prjID=%x\n",ret,fwArgPrjID[0],fwArgPrjID[1], fwArgPrjID[2]);
    }
    bl_log_trace("bl_update_fw_for_self_ctp:isBlank = %x\n",isBlank);
	//isBlank = 1;//Force update firmware
//Step 3:Specify download size
    if(IsCobPrj)
    {
    	downSize = bl_get_cob_project_down_size_arguCnt(fw_data,fw_size,&arguCount);
    	bl_log_trace("bl_update_fw_for_self_ctp:downSize = %x,arguCount = %x\n",downSize,arguCount);
    }
    else
    {
    	downSize = fw_size;
    	bl_log_trace("bl_update_fw_for_self_ctp:downSize = %x\n",downSize);
    }

UPDATE_SECOND_FOR_COB:
//Step 4:Update the fwArgPrjID
    if(!isBlank)
    {
        bl_get_fwArgPrj_id(fwArgPrjID);
    }
	
//Step 5:Specify the argument data for cob project
    if(IsCobPrj && !isBlank)
    {
        MDELAY(50);
        ret = bl_get_cob_id(cobID);
		if(ret < 0)
        {
            bl_log_trace("bl_update_fw_for_self_ctp:bl_get_cob_id error\n");
			ret = -1;
			goto UPDATE_ERROR;
        }
		else
		{
		    bl_log_trace("bl_update_fw_for_self_ctp:cobID = %x %x %x %x %x %x\n",cobID[0],cobID[1],cobID[2],cobID[3],cobID[4],cobID[5]);
		}
		ret = bl_get_specific_argument(&specificArguAddr,cobID,fw_data,fw_size,arguCount);
		if(ret < 0)
		{
            bl_log_trace("Can't found argument for CTP module,use default argu:\n");
		}
		bl_log_trace("bl_update_fw_for_self_ctp:specificArguAddr = %x\n",specificArguAddr);
       }
	
	bl_log_trace("fw_data[] = %x  fw_data[] = %x", fw_data[BL_ARGUMENT_BASE_OFFSET + VERTIFY_START_OFFSET],fw_data[BL_ARGUMENT_BASE_OFFSET + VERTIFY_END_OFFSET]);

//Step 6:Obtain IC firmware checksum when the version number is same between IC and host firmware
    bl_log_trace("isBlank = %d  ver1 = %d ver2 = %d binVer1 = %d binVer2 = %d specificAddr = %x", isBlank,fwArgPrjID[0],fwArgPrjID[1],fw_data[specificArguAddr+BL_FWVER_MAIN_OFFSET],fw_data[specificArguAddr + BL_FWVER_ARGU_OFFSET], specificArguAddr);
    if(!isBlank)
	{
		bl_get_fw_bin_checksum_for_self_ctp(fw_data, &fw_bin_checksum, downSize, specificArguAddr);
		ret = bl_get_fw_checksum(&fw_checksum);
		if((ret < 0) || (fw_checksum != fw_bin_checksum)){
			bl_log_trace("bl_update_fw_for_self_ctp:Read checksum fail fw_checksum = %x\n",fw_checksum);
			fw_checksum = 0x00;
		}
		bl_log_trace("bl_update_fw_for_self_ctp:fw_checksum = 0x%x,fw_bin_checksum = 0x%x___\n",fw_checksum, fw_bin_checksum);
    }

//Step 7:Select update fw+arg or only update arg
    update_type = choose_update_type_for_self_ctp(isBlank, fw_data, fwArgPrjID[0], fwArgPrjID[1], fw_checksum, fw_bin_checksum,specificArguAddr);

//Step 8:Start Update depend condition
    if(update_type != NONE_UPDATE)
    {
        ret = bl_update_flash_for_self_ctp(update_type, fw_data, downSize,specificArguAddr);
        if(ret < 0)
        {
            bl_log_trace("bl_update_fw_for_self_ctp:bl_update_flash failed\n");
			goto UPDATE_ERROR;
        }
    }
	 
//Step 9:Execute second update flow when project firmware is cob and last update_type is FW_ARG_UPDATE
    if((ret == 0)&&(IsCobPrj)&&(isBlank))
    {
        isBlank = 0;
        bl_log_trace("bl_update_fw_for_self_ctp:bl_update_flash for COB project need second update with blank IC:isBlank = %d\n",isBlank);
        goto UPDATE_SECOND_FOR_COB;
    }
	bl_log_trace("bl_update_fw_for_self_ctp exit\n");

UPDATE_ERROR:
	return ret;
}

static int bl_update_fw_for_compatible_ctp(unsigned char fileType,unsigned char* fw_data, int fw_size)
{
	unsigned char fwArgPrjID;	//firmware version
	int ret = 0x00;
	unsigned short fw_bin_checksum = 0x0;
	unsigned short fw_checksum = 0x0;
	unsigned char isBlank = 0x0;	//Indicate the IC have any firmware
	unsigned char update_type = NONE_UPDATE;  
	unsigned char projectFlag = 0;      //protect flag
	bl_log_trace("bl_update_fw_for_compatible_ctp start\n");

//check protect flag
    projectFlag = bl_get_protect_flag();
    if(bl_get_protect_flag() && (fileType == HEADER_FILE_UPDATE))
    {
        bl_log_trace("bl_update_fw_for_compatible_ctp:projectFlag = %x, fileType = %x", projectFlag, fileType);
		return 0;
    }
	
//Step 1:Obtain IC version number
	MDELAY(5);
	ret = bl_get_prj_id(&fwArgPrjID);
	if((ret < 0)
		|| (fileType == BIN_FILE_UPDATE)
		|| ((ret == 0) && (fwArgPrjID == 0))
		|| ((ret == 0) && (fwArgPrjID == 0xff)) 
		|| ((ret == 0) && (fwArgPrjID == BL_PRJ_ID_REG)))
	{
		isBlank = 1;
		bl_log_trace("bl_update_fw_for_compatible_ctp:This is blank IC ret = %x fwArgPrjID=%x\n",ret,fwArgPrjID);
	}
	else
	{
		isBlank = 0;
		bl_log_trace("bl_update_fw_for_compatible_ctp:ret=%x fwID=%x\n",ret,fwArgPrjID);
	}
//step 2:check checksum

    bl_log_trace("isBlank = %d  fwArgPrjID = %d  binFwArgPrjID = %d\n", isBlank,fwArgPrjID, fw_data[PJ_ID_OFFSET]);
    if(!isBlank)
    {
    	bl_get_fw_bin_checksum_for_compatible_ctp(fw_data, &fw_bin_checksum, fw_size);
    	ret = bl_get_fw_checksum(&fw_checksum);
    	if((ret < 0) || (fw_checksum != fw_bin_checksum)){
    		bl_log_trace("bl_update_fw_for_compatible_ctp:Read checksum fail fw_checksum = %x\n",fw_checksum);
    		fw_checksum = 0x00;
    	}
    	bl_log_trace("bl_update_fw_for_compatible_ctp:fw_checksum = 0x%x,fw_bin_checksum = 0x%x___\n",fw_checksum, fw_bin_checksum);
    }

//Step 2:Select update fw+arg or only update arg
	update_type = choose_update_type_for_compatible_ctp(isBlank, fw_data, fwArgPrjID, fw_checksum);

//Step 3:Start Update depend condition
	if(update_type != NONE_UPDATE)
	{
		ret = bl_update_flash_for_compatible_ctp(update_type, fw_data, fw_size);
		if(ret < 0)
		{
			bl_log_trace("bl_update_fw_for_compatible_ctp:bl_update_flash failed\n");
			goto UPDATE_ERROR;
		}
	}
	 
UPDATE_ERROR:
	bl_log_trace("bl_update_fw_for_compatible_ctp exit\n");
	return ret;
}

static int bl_update_fw(unsigned char fileType,unsigned char ctpType, unsigned char* pFwData, unsigned int fwLen)
{
    int ret = 0;
	bl_log_trace("bl_update_fw:ctpType = %x\n",ctpType); 
    switch(ctpType)
    {
        case SELF_CTP:
        case SELF_INTERACTIVE_CTP:
			ret = bl_update_fw_for_self_ctp(fileType,pFwData,fwLen);
			break;

		case COMPATIBLE_CTP:
			ret = bl_update_fw_for_compatible_ctp(fileType,pFwData,fwLen);
			break;			
    }
	return ret;
}
#ifdef BL_AUTO_UPDATE_FARMWARE
int bl_auto_update_fw(void)
{
	int ret = 0;
    unsigned int fwLen = sizeof(fwbin);

	bl_log_trace("bl_auto_update_fw:fwLen = %x\n",fwLen); 
	ret = bl_update_fw(HEADER_FILE_UPDATE,CTP_TYPE,(unsigned char *)fwbin, fwLen);
	
	if(ret < 0)
	{
		bl_log_trace("bl_auto_update_fw: bl_update_fw fail\n");  
	}
	else
	{
		bl_log_trace("bl_auto_update_fw: bl_update_fw success\n");  
	}
	
	return ret;
}
#endif
#endif
