/**
 * @file
 * @details
 * @author
 * @date
 * @version
**/

/* include */
#include "app_update.h"
#include "bsp_hwrtc.h"
#include "app_paras.h"
#include "app_can.h"
#include "app_can1.h"
#include "easyflash.h"
#include "head_protocol.h"
#include "update_protocol.h"
#include "app_utility.h"
#include "app_log.h"
#include "easyflash.h"
#include "user_interface.h"

/* macro */
#define APP_UPDATE_RTC_BKP_DR      RTC_BKP_DR2   //RTC_BKP_DR1在rtthread的rtc中已经使用
#define APP_UPDATE_BUFF_MAX        128

#define DIFF_UPGRADE_OFFSET_ADDR    1024 * 1024
#define IAP_AREA_SIZE_MAX           DIFF_UPGRADE_OFFSET_ADDR + 1016 * 1024


/* type declaration */
typedef enum {
    DIFF_NO_ERR,    // 差分校验ok
    DIFF_HEADER_ERR,    // 差分包头校验错误
    DIFF_OLD_CRC_ERR,   // 旧 bin 校验错误
    DIFF_NEW_SIZE_ERR,  // 新 bin 长度错误
    DIFF_NEW_CRC_ERR,   // 新 bin 校验错误
}diff_state_e;

typedef enum {
    INRC_PACKAGE,   //  增量包
    FULL_PACKAGE,   // 全量包
}upgrade_package_e;
typedef struct
{
    uint16_t head;          //头信息
    uint16_t update_flag;      // 0-不升级  1-升级
    uint16_t update_size[2];   //升级包大小
    uint16_t update_addr[2];   //升级地址
    uint16_t check;
}update_data_infor_t;
typedef struct
{
    update_data_infor_t update_data_infor;
    uint16_t            head_data;
    uint8_t             update_buff[APP_UPDATE_BUFF_MAX];
    uint32_t            update_size;
    size_t              cur_size;
    uint32_t            pkg_num;

    upgrade_package_e   package_type;
}app_update_local_t;

/* variable declaration */
app_update_env_t app_update_env;
#define env app_update_env
app_update_local_t app_update_local;
#define local app_update_local

/* function declaration */
/**
 * @brief
 * @param
 * @return
 * @note   升级准备 AF CB 00 01 00 02 00 04 81
**/
static void app_update_ready(uint8_t *data, uint16_t size)
{
    char update_info[256];
    pupdata_protocol_t updata = (pupdata_protocol_t)data;
    pupdata_protocol_t reply_updata = (pupdata_protocol_t)local.update_buff;

    *(uint16_t *)reply_updata->head = *(uint16_t *)updata->head;
    reply_updata->command = READY_CMD;
    reply_updata->operate = REPLY_OPE;
    *(uint16_t *)reply_updata->lens = ntoh16(1);

    if((updata->operate == WRITE_OPE) && (ntoh16(*(uint16_t *)updata->lens) == 0x04) && (ntoh32(*(uint32_t *)updata->data) > 0x00))
    {
        env.update_state = UPDATE_READY;
        local.update_size = ntoh32(*(uint32_t *)updata->data);
        int update_size = (int)local.update_size;
        int counts = sprintf(update_info, "app update size = %d", update_size);
        update_info[counts] = '\0';
        app_log_msg(LOG_LVL_INFO, update_info);

        reply_updata->data[0] = APP_UPDATE_OPE_SUCCESS;
        reply_updata->data[1] = CheckSum(local.update_buff, sizeof(updata_protocol_t));
        app_can1_queue_send(CLB_BOARD, local.update_buff, sizeof(updata_protocol_t) + 2);
        env.update_state = UPDATE_START;
    }
    else
    {
        reply_updata->data[0] = APP_UPDATE_OPE_FAILED;
        reply_updata->data[1] = CheckSum(local.update_buff, sizeof(updata_protocol_t));
        app_can1_queue_send(CLB_BOARD, local.update_buff, sizeof(updata_protocol_t) + 2);
    }
}

/**
 * @brief
 * @param
 * @return
 * @note   固件传输 AF CB 01 01 00 0A 11 22 33 44 55 66 77 88 99 00 83
**/
static void app_update_trans(uint8_t *data, uint16_t size)
{
    static EfErrCode result;
    pupdata_protocol_t updata = (pupdata_protocol_t)data;
    pupdata_protocol_t reply_updata = (pupdata_protocol_t)local.update_buff;

    *(uint16_t *)reply_updata->head = *(uint16_t *)updata->head;
    reply_updata->command = TRANS_CMD;
    reply_updata->operate = REPLY_OPE;
    *(uint16_t *)reply_updata->lens = ntoh16(3);

    if(env.update_state == UPDATE_START)
    {
        int res = rt_memcmp(&updata->data[2] + sizeof(image_header_t), "ENDSLEY/BSDIFF43", 16);
        local.package_type = (res == 0) ? INRC_PACKAGE : FULL_PACKAGE;
        switch (local.package_type) {
            case INRC_PACKAGE:
                rt_kprintf("diff package\n");
                image_header_t *p = (image_header_t *)&updata->data[2];
                result = ef_erase_bak_app(p->ih_ep);
                if(EF_NO_ERR == result) result = ef_erase_bak_app_for_diff(DIFF_UPGRADE_OFFSET_ADDR, local.update_size);
                break;
            case FULL_PACKAGE:
                rt_kprintf("full package\n");
                result = ef_erase_bak_app(local.update_size);
                break;
        }
        local.cur_size = 0;
        local.pkg_num = 1;
        env.update_state = UPDATE_DOING;
    }
    if((updata->operate == WRITE_OPE) && (env.update_state == UPDATE_DOING) && (ntoh16(*(uint16_t *)updata->data) == local.pkg_num) && \
            (result == EF_NO_ERR))
    {
        uint16_t fw_size = ntoh16(*(uint16_t *)updata->lens) - 2; //数据包总长度-包序号长度
        rt_kprintf("pkg_id = %d---recv data =%d\n", ntoh16(*(uint16_t *)updata->data), fw_size);
        switch (local.package_type) {
            case INRC_PACKAGE:
                ef_write_data_to_bak_for_diff(&updata->data[2], fw_size, &local.cur_size, local.update_size, DIFF_UPGRADE_OFFSET_ADDR);
                break;
            case FULL_PACKAGE:
                ef_write_data_to_bak(&updata->data[2], fw_size, &local.cur_size, local.update_size);
                break;
        }
//        ef_write_data_to_bak(&updata->data[2], fw_size, &local.cur_size, local.update_size);
        *(uint16_t *)reply_updata->data = ntoh16(local.pkg_num);
        reply_updata->data[2] = APP_UPDATE_OPE_SUCCESS;
        reply_updata->data[3] = CheckSum(local.update_buff, sizeof(updata_protocol_t) + 3);
        app_can1_queue_send(CLB_BOARD, local.update_buff, sizeof(updata_protocol_t) + 4);
        local.pkg_num++;
    }
    else
    {
        *(uint16_t *)reply_updata->data = ntoh16(local.pkg_num);
        reply_updata->data[2] = APP_UPDATE_OPE_FAILED;
        reply_updata->data[3] = CheckSum(local.update_buff, sizeof(updata_protocol_t) + 3);
        app_can1_queue_send(CLB_BOARD, local.update_buff, sizeof(updata_protocol_t) + 4);
    }
}
/**
 * @brief
 * @param
 * @return
 * @note
**/
static diff_state_e app_update_diff_judge()
{
    uint8_t buff[sizeof(image_header_t)] = {0};
    ef_port_read(ef_get_bak_app_start_addr() + DIFF_UPGRADE_OFFSET_ADDR, (uint32_t *)buff, sizeof(image_header_t));
    image_header_t *p = (image_header_t *)buff;
    local.update_size = p->ih_ep;
    // 校验差分包包头
    uint32_t recv_crc = ntoh32(p->ih_hcrc);
    p->ih_hcrc = 0;
    if(recv_crc != crc32(buff, sizeof(image_header_t))) {
        rt_kprintf("diff package crc header error!\n");
        return DIFF_HEADER_ERR;
    }
    if(ntoh32(p->ih_ocrc) != crc32((uint8_t *)0x08020000,p->ih_load)) {
        rt_kprintf("old bin crc check error\n");
        return DIFF_OLD_CRC_ERR;
    }
    if(p->ih_ep != iap_patch((uint8_t *)0x08020000, p->ih_load, ef_get_bak_app_start_addr() + DIFF_UPGRADE_OFFSET_ADDR + sizeof(image_header_t), p->ih_size)) {
        rt_kprintf("new bin size error !\n");
        return DIFF_NEW_SIZE_ERR;
    }
    if(ntoh32(p->ih_dcrc) != ef_crc32(p->ih_ep)) {
        rt_kprintf("new bin cec error !\n");
        return DIFF_NEW_CRC_ERR;
    }
    return DIFF_NO_ERR;
}
/**
 * @brief
 * @param
 * @return
 * @note   固件传输完成
**/
static void app_update_finish(uint8_t *data, uint16_t size)
{
    pupdata_protocol_t updata = (pupdata_protocol_t)data;
    pupdata_protocol_t reply_updata = (pupdata_protocol_t)local.update_buff;

    *(uint16_t *)reply_updata->head = *(uint16_t *)updata->head;
    reply_updata->command = FINISH_CMD;
    reply_updata->operate = REPLY_OPE;
    *(uint16_t *)reply_updata->lens = ntoh16(1);

    if((updata->operate == WRITE_OPE) && (ntoh16(*(uint16_t *)updata->lens) == 0x00))
    {
        // 进行差分还原
        if((local.package_type == INRC_PACKAGE) && (DIFF_NO_ERR != app_update_diff_judge())) {
            rt_kprintf("diff chafen huanyuan error !\n");
            return ;
        }
        local.update_data_infor.head = APP_UPDATE_HEAD;
        local.update_data_infor.update_flag = 1;
        local.update_data_infor.update_size[0] = DATA32_H(local.update_size);
        local.update_data_infor.update_size[1] = DATA32_L(local.update_size);
        local.update_data_infor.update_addr[0] = DATA32_H(0x08020000);
        local.update_data_infor.update_addr[1] = DATA32_L(0x08020000);
        local.update_data_infor.check = CheckTotal((uint16_t *)&local.update_data_infor, sizeof(update_data_infor_t) / sizeof(uint16_t) - 1);
        bsp_hwrtc_bkup_write(APP_UPDATE_RTC_BKP_DR, (uint16_t *)&local.update_data_infor, sizeof(update_data_infor_t) / sizeof(uint16_t));

        local.update_size = 0;
        local.cur_size = 0;
        rt_kprintf("app update flag = %d, size = 0x%08X, addr = 0x%08X check = 0x%04x !\n", local.update_data_infor.update_flag, \
                        DATA32(local.update_data_infor.update_size[0], local.update_data_infor.update_size[1]), \
                        DATA32(local.update_data_infor.update_addr[0], local.update_data_infor.update_addr[1]), \
                        local.update_data_infor.check);
        app_log_msg(LOG_LVL_INFO, "app update success reset mcu");
        reply_updata->data[0] = APP_UPDATE_OPE_SUCCESS;
        reply_updata->data[1] = CheckSum(local.update_buff, sizeof(updata_protocol_t) + 1);
        app_can1_queue_send(CLB_BOARD, local.update_buff, sizeof(updata_protocol_t) + 2);
        rt_thread_delay(2000);
        rt_hw_cpu_reset();
    }
    else
    {
        reply_updata->data[0] = APP_UPDATE_OPE_FAILED;
        reply_updata->data[1] = CheckSum(local.update_buff, sizeof(updata_protocol_t));
        app_can1_queue_send(CLB_BOARD, local.update_buff, sizeof(updata_protocol_t) + 2);
    }
}

/**
 * @brief
 * @param
 * @return
 * @note
**/
void app_update_read_data(uint8_t *data, uint16_t size)
{
    sys_paras_t *paras = app_paras_get();
    if((data == RT_NULL) || (size == 0)) {
        return ;
    }
    if((data[0] == 0xAF) && (paras->id == data[2]) && (size == 4))
    {
        uint8_t send_data[4] = { 0 };
        memcpy(send_data, data, 4);
        send_data[3] = 0x01;
        app_can1_queue_send(CLB_BOARD, send_data, 4);
        rt_thread_delay(2000);
        rt_hw_cpu_reset();
    }
    if((data == RT_NULL) || (size == 0) || (size < sizeof(updata_protocol_t)) ||
            (*(uint16_t *)data != ntoh16(APP_UPDATE_HEAD)))
//    if((data == RT_NULL) || (size == 0) || (size < sizeof(updata_protocol_t)) )
    {
        return ;
    }
    uint8_t cal_crc = CheckSum(data, size - 1);
    if(data[size - 1] != cal_crc)
    {
        rt_kprintf("recv crc = 0x%02X, calculate crc = 0x%02X ! \n", data[size - 1], cal_crc);
        return ;
    }
    pupdata_protocol_t updata = (pupdata_protocol_t)data;
    if((ntoh16(*(uint16_t *)updata->lens) + sizeof(updata_protocol_t) + 1/*校验位*/) != size)
    {
        rt_kprintf("%d  %d\n", ntoh16(*(uint16_t *)updata->lens), sizeof(updata_protocol_t));
        rt_kprintf("update read lens error ! \n");
        return ;
    }

    switch (updata->command)
    {
        case READY_CMD:  //升级准备
            env.start_update_flag = 1;
            app_update_ready(data, size);
            break;
        case TRANS_CMD:  //固件传输
            app_update_trans(data, size);
            break;
        case FINISH_CMD: //固件传输完成
            app_update_finish(data, size);
            break;
        default:
            break;
    }
}

/**
 * @brief
 * @param
 * @return
 * @note
**/
void app_update_init(void)
{
    sys_paras_t *paras = app_paras_get();
    env.update_state = UPDATE_NULL;
    env.start_update_flag = 0;
    if((sizeof(update_data_infor_t) / sizeof(uint32_t)) <= APP_RTCBKUP_LENS_MAX)
    {
        memset(&local.update_data_infor, 0, sizeof(update_data_infor_t));
        bsp_hwrtc_bkup_write(APP_UPDATE_RTC_BKP_DR, (uint16_t *)&local.update_data_infor,
                sizeof(update_data_infor_t) / sizeof(uint16_t));
        if(paras->id == 0x01) {
            local.head_data = HEAD_CAN_QZQ1_UPDATE_INFO;
        }
        else if(paras->id == 0x02) {
            local.head_data = HEAD_CAN_QZQ2_UPDATE_INFO;
        }
        else if(paras->id == 0x03) {
            local.head_data = HEAD_CAN_QZQ3_UPDATE_INFO;
        }
        else if(paras->id == 0x04) {
            local.head_data = HEAD_CAN_QZQ4_UPDATE_INFO;
        }
        env.update_state = UPDATE_INITOK;
        app_log_msg(LOG_LVL_INFO, "app update init success");
    }
    else {
        app_log_msg(LOG_LVL_ERROR, "app update data greater than rtcbkup max");
    }
}
