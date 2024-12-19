#ifndef __VFILE_H__
#define __VFILE_H__
/*******************************************************************************************************/
#include "stdint.h"

typedef struct
{
    uint32_t curptr;
    uint32_t offset;
    uint32_t size;
}vFile;

/*******************************************************************************************************/
vFile *vfopen(const uint32_t dp, uint32_t size);
int vfread(vFile *fp, uint8_t *buff, int len);
uint32_t vfgetpos(vFile *fp, uint32_t *position);
int vfsetpos(vFile *fp, uint32_t position);
int vfclose(vFile *fp);
uint32_t vfgetlen(vFile *fp);

#endif
/*******************************************************************************************************
**                            End Of File
********************************************************************************************************/
