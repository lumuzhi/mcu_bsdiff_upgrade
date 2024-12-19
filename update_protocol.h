/**
 * @file
 * @details
 * @author
 * @date
 * @version
**/

#ifndef __UPDATE_PROTOCOL_H__
#define __UPDATE_PROTOCOL_H__

/* include */
#include "app_board.h"

/* macro */
#define APP_UPDATE_HEAD            0xAFCB

#define APP_UPDATE_OPE_SUCCESS     0
#define APP_UPDATE_OPE_FAILED      1
#define APP_UPDATE_OPE_BUSY        2

/* type declaration */
typedef enum
{
    READY_CMD,      //准备
    /*
                命令：AF CB 00（00 命令） 01（01 写操作） 00 04（00 04 数据长度） XX XX XX XX（升级包大小） XX（校验）
                响应：AF CB 00（00 命令） 02（02 响应） 00 01（00 01数据长度） XX（成功/失败/忙碌） XX（校验）
    */
    TRANS_CMD,    //固件传输
    /*
                命令：AF CB 01（01 命令） 01（01 写操作） XX XX（数据长度） XX XX（包序号）XX XX（数据包内容） XX（校验）
                响应：AF CB 01（01 命令） 02（02 响应） XX XX（数据长度） XX XX（包序号）XX（成功/失败/忙碌） XX（校验）
    */
    FINISH_CMD,   //固件接收完成
    /*
                命令：AF CB 02（02 命令） 01（01 写操作） 00 00（数据长度） XX（校验）
                响应：AF CB 02（02 命令） 02（02 响应） 00 01（数据长度） XX（成功/失败/忙碌） XX（校验）
    */
}command_e;
typedef enum
{
    READ_OPE,     //读操作
    WRITE_OPE,    //写操作
    REPLY_OPE,    //响应
}operate_e;
typedef struct
{
    uint8_t   head[2];
    command_e command;     //命令
    operate_e operate;     //操作
    uint8_t   lens[2];     //data中的数据部分长度（不包含校验位）
    uint8_t   data[];      //包含数据内容和校验位（和校验）
}updata_protocol_t, *pupdata_protocol_t;


#endif /*__UPDATE_PROTOCOL_H__*/





