/*
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>
#include <sys/printk.h>
#include <device.h>
#include <drivers/spi.h>
#include "PSRAMManager.h"

typedef struct __result_combination{
    uint32_t wr_addr;
    uint32_t rd_addr;
    uint32_t wr_d;
    uint32_t rd_d;
    struct __result_combination * next;
}result_combination;

result_combination* error_head;
static uint32_t volatile MBXEvent = 0;
static void LinkList_Initialize(result_combination** head){
    (*head) = (result_combination*)malloc(sizeof(result_combination));

    (*head)->wr_addr = 0x0;
    (*head)->rd_addr = 0x0;
    (*head)->wr_d = 0x0;
    (*head)->rd_d = 0x0;
    (*head)->next = NULL;
}

static void LinkList_increament(result_combination* head, uint32_t wr_addr, uint32_t rd_addr, uint32_t wr_d, uint32_t rd_d){
    result_combination* pointer = NULL;

    if(head == NULL){
        return;
    }

    pointer = head;

    while(pointer->next != NULL){
        pointer = pointer->next;
    }

    // write data
    pointer->wr_addr = wr_addr;
    pointer->rd_addr = rd_addr;
    pointer->wr_d = wr_d;
    pointer->rd_d = rd_d;

    pointer->next = (result_combination*)malloc(sizeof(result_combination));
    pointer->next->next = NULL;
    if (pointer->next == NULL){
        return;
    }
}

static void LinkList_Display(result_combination* head){
    result_combination* pointer = NULL;

    if(head == NULL || head->next == NULL){
        return;
    }

    pointer = head;

    do{
        printk("[FAILED]: Write address ---> 0x%x \r\n"
              "        Read address ---> 0x%x\r\n"
              "[WRITE DATA]           [READ DATA]\r\n"
              "  0x%x                     0x%x\r\n", pointer->wr_addr, pointer->rd_addr, pointer->wr_d, pointer->rd_d);
        pointer = pointer->next;
    } while(pointer->next != NULL);
}

static void LinkList_Distroy(result_combination* head){
    result_combination* pointer = NULL;
    result_combination* pre_pointer = NULL;

    if(head == NULL){
        return;
    }

    pointer = head;

    while(pointer != NULL){
        pre_pointer = pointer;
        pointer = pointer->next;
        free(pre_pointer);
    }

}

void main(void)
{
    printk("psram test start!\n");
    uint32_t i = 0,error_times = 0;


    uint32_t wr_data = 1;
    uint32_t rd_data = 0;
    printk("p1sram test start!\n");

    LinkList_Initialize(&error_head);
    uint32_t cnt = 0;       // 总次数
    while(1){
        uint32_t* wr_address = (uint32_t*)0x30000000;
        uint32_t* rd_address = (uint32_t*)0x30000000;
        for(i = 0; i <= (1024*1024*2); i++){
            cnt++;
            // get random data
            wr_data = rand();

            // write random data to PSRAM
            (*wr_address) = wr_data;
            // read it back
            rd_data = (*rd_address);

            if(rd_data == wr_data){
                // PASS
            } else{
                error_times++;
                LinkList_increament(error_head, (uint32_t)wr_address, (uint32_t)rd_address, wr_data, rd_data);
            }

            // if(i%10000 == 0){
            //     printk("i:%d,rd:%d,wd:%d\n", i, rd_data, wr_data);

            // }
            wr_address++;
            rd_address++;

        }
        printk("RESULT ERROE TIMES: [%d/%d]\n", error_times, cnt);
    }

    printk("SUCCESS SHOW BELOW:");
    LinkList_Display(error_head);
    printk("RESULT ERROE TIMES: [%d/%d]", error_times, (1024*1024));
    LinkList_Distroy(error_head);
}
