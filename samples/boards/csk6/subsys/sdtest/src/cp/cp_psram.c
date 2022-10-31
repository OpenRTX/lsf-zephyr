/*
    CP使用UART2（TX PA18  RX PA15）作为日志口
    代码运行在PSRAM中，同时一直访问PSRAM空间
*/


uint8_t *psram_buf;
uint8_t offset = 0;
void main(void)
{
	LOG_ERR("dsp start");

	psram_buf = (uint8_t *)0x60700000;

	while(1){
		for(uint32_t i = 0; i <	1000 * 1024; i++){
			psram_buf[i] = (uint8_t)(i + offset);
		}

		for(uint32_t i = 0; i <	1000 * 1024; i++){
			if(psram_buf[i] != (uint8_t)(i + offset)){
				LOG_ERR("psram data error: %d", i);
			}
		}	

		LOG_ERR("psram data ok %d", offset++);
	}
}