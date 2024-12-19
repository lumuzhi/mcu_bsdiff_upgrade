/**
 * @file
 * @details
 * @author
 * @date
 * @version
**/

#ifndef __APP_UPDATE_H__
#define __APP_UPDATE_H__

/* include */
#include "app_board.h"

/* macro */
#define APP_UPDATE_HEAD       0xAFCB
#define APP_RTCBKUP_LENS_MAX  80

/* type declaration */

typedef enum
{
    UPDATE_NULL,
    UPDATE_INITOK,  //初始化完成
    UPDATE_READY,   //就绪
    UPDATE_START,   //开始
    UPDATE_DOING,   //进行中
    UPDATE_FINISH,  //完成
}update_state_e;
typedef struct
{
    update_state_e update_state;
    uint8_t         start_update_flag;
}app_update_env_t;

extern app_update_env_t app_update_env;

/* variable */

/* function */
void app_update_init(void);
void app_update_read_data(uint8_t *data, uint16_t size);
void app_update_data_deal(uint8_t *data, uint16_t size);

#endif /*__APP_UPDATE_H__*/
