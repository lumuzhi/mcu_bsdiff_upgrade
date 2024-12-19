## fork from ruiwarn/mcu_bsdiff_upgrade
原本想直接 rt_malloc 20k 大小空间用作差分包缓存区，但剩余 ram 不足以进行差分算法还原
主控f103re芯片，ram只有64k，片上flash 512k，板载W25Q256(32M)用作easyflash，ENV 8k， Log 30M，剩余为IAP。项目使用rt-thread

 * |----------------------------|   Storage Size
 * | Environment variables area |   ENV area size @see ENV_AREA_SIZE
 * |----------------------------|
 * |      Saved log area        |   Log area size @see LOG_AREA_SIZE
 * |----------------------------|
 * |(IAP)Downloaded application |   IAP already downloaded application, unfixed size
 * |----------------------------|

IAP区域被划分为ota区和差分包区，IAP区域总大小 2M - 8K，把最后 1016k 作差分包区
每次还原和写入都是128字节
## 示例
阅读app_update.c update_protocol.h