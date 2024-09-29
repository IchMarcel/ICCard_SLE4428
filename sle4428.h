#ifndef __SLE4428_H
#define __SLE4428_H

#include "bsp_gpio.h"

#define SLE4428ID1   0x92
#define SLE4428ID2   0x23
#define SLE4428ID3   0x10
#define SLE4428ID4   0x91

uint8_t SLE4428_Reset(void);
uint8_t SLE4428_ReadData_NoProtect(uint16_t start_address, uint16_t length, uint8_t *recvbuff);
uint8_t SLE4428_ReadData_Protect(uint16_t start_address, uint16_t length, uint16_t *recvbuff);
uint8_t SLE4428_Write_ErrCnt(void);
uint8_t SLE4428_Erase_ErrCnt(void);
uint8_t SLE4428_PSC_Byte1(void);
uint8_t SLE4428_PSC_Byte2(void);
uint8_t SLE4428_PSCVerification(void);
uint8_t SLE4428_WriteData_Noprotect(uint16_t start_address, uint8_t data);
uint8_t SLE4428_WriteData_protect(uint16_t start_address, uint8_t data);
void SLE4428_Init(void);

void SLE4428_IO_Test(void);


#endif






