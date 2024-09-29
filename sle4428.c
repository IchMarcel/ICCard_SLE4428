#include "sle4428.h"
#include "sys.h"
#include <stdio.h>

#define RST_H()  HAL_GPIO_WritePin(IC_RST_GPIO_Port, IC_RST_Pin, GPIO_PIN_SET)
#define RST_L()  HAL_GPIO_WritePin(IC_RST_GPIO_Port, IC_RST_Pin, GPIO_PIN_RESET)

#define CLK_H()  HAL_GPIO_WritePin(IC_CLK_GPIO_Port, IC_CLK_Pin, GPIO_PIN_SET)
#define CLK_L()  HAL_GPIO_WritePin(IC_CLK_GPIO_Port, IC_CLK_Pin, GPIO_PIN_RESET)

#define DATA_H()  HAL_GPIO_WritePin(IC_SDA_GPIO_Port, IC_SDA_Pin, GPIO_PIN_SET)
#define DATA_L()  HAL_GPIO_WritePin(IC_SDA_GPIO_Port, IC_SDA_Pin, GPIO_PIN_RESET)

#define IC_PWR_ON()  HAL_GPIO_WritePin(ChipPwrCtrl_GPIO_Port, ChipPwrCtrl_Pin, GPIO_PIN_SET)
#define IC_PWR_OFF() HAL_GPIO_WritePin(ChipPwrCtrl_GPIO_Port, ChipPwrCtrl_Pin, GPIO_PIN_RESET)

//IO方向设置
#define DATA_IN()  {GPIOB->MODER&=0XFF0FFFFF;GPIOB->MODER|=0X00100000;}
#define DATA_OUT() {GPIOB->MODER&=0XFF0FFFFF;GPIOB->MODER|=0X00500000;GPIOB->OTYPER|=0X0800;}

#define DATA_READ() GPIOB->IDR&0x800

#define SLE4428_Delayus(n) delay_us(n)
#define SLE4428_Delayms(n) delay_ms(n)

#define SLE4428_Delay()    delay_us(50)


//IC卡引脚初始化
void Bsp_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB, ChipPwrCtrl_Pin|IC_CLK_Pin|IC_RST_Pin|IC_SDA_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(RUN_LED_GPIO_Port, RUN_LED_Pin, GPIO_PIN_SET);

	/*Configure GPIO pin : ChipPwrCtrl_Pin */
	GPIO_InitStruct.Pin = ChipPwrCtrl_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(ChipPwrCtrl_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : IC_CLK_Pin */
	GPIO_InitStruct.Pin = IC_CLK_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(IC_CLK_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : IC_SDA_Pin */
	GPIO_InitStruct.Pin = IC_SDA_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;//DATA引脚配置为开漏输出
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(IC_SDA_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : IC_RST_Pin */
	GPIO_InitStruct.Pin = IC_RST_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(IC_RST_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : RUN_LED_Pin */
	GPIO_InitStruct.Pin = RUN_LED_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(RUN_LED_GPIO_Port, &GPIO_InitStruct);
}



//SLE4428时钟脉冲信号
static void SLE4428_ClkPluse(void)
{
	CLK_L();
	//SLE4428_Delayus(50);
	SLE4428_Delay();
	CLK_H();
	//SLE4428_Delayus(50);
	SLE4428_Delay();
}

//SLE4428不带保护位读一个byte数据
static uint8_t SLE4428_ReadByte_NoProtect(void)
{
	uint8_t u8data = 0;
	
	//DATA_IN();//置数据线为输入方式
	SLE4428_DataIn();
	
	for(uint8_t i=0; i<8; i++)
	{
		u8data >>= 1;
		SLE4428_ClkPluse();
		
		if(DATA_READ()) u8data|=0x80;   //读数据位
	}
	CLK_L();
	//DATA_OUT();
	SLE4428_DataOut();
	
	return u8data;
}

//SLE4428带保护位读一个byte数据
//0x003f 0代表启用保护位 数据不可修改
//0x013f 1代表未启用保护位 数据可修改
static uint16_t SLE4428_ReadByte_Protect(void)
{
	uint16_t receive = 0;
	
	//DATA_IN();//置数据线为输入方式
	SLE4428_DataIn();
	
	for(uint8_t i=0;i<9;i++)
	{
		SLE4428_ClkPluse();
		receive >>= 1;
		if(DATA_READ()) receive|=0x100;   //读数据位,接收的数据位放入x
	}				   
	CLK_L();

	//DATA_OUT()
	SLE4428_DataOut();
	
	return receive;
}

//向SLE4428写入一个byte数据
static void SLE4428_WriteByte(uint8_t data)
{
	//DATA_OUT();
	SLE4428_DataOut();
	
	for(uint8_t i=0; i<8; i++)
	{
		if(data & 0x80)
			DATA_H();
		else
			DATA_L();
		
		SLE4428_ClkPluse();
		data <<= 1;
	}
	
	CLK_L();
}

uint8_t SLE4428_Reset(void)
{
	uint8_t ReadBuf[4] = {0};
	
	//产生复位时序
	RST_L();
	CLK_L();
	SLE4428_Delay();//SLE4428_Delayus(50);
	RST_H();
	SLE4428_Delay();//SLE4428_Delayus(50);
	CLK_H();
	SLE4428_Delay();//SLE4428_Delayus(50);
	CLK_L();
	SLE4428_Delay();//SLE4428_Delayus(50);
	RST_L();
	
	//复位之后，读开头4字节
	for(uint8_t i = 0; i < 4; i++)
	{
		ReadBuf[i] = SLE4428_ReadByte_NoProtect();
	}
	
	if ((ReadBuf[0] == SLE4428ID1) && (ReadBuf[1] == SLE4428ID2) &&
	    (ReadBuf[2] == SLE4428ID3) && (ReadBuf[3] == SLE4428ID4))
	{	
		return 1;			//复位值正确,返回复位成功
	}
	else
	{

		return 0;			//复位值错误,返回复位失败
	}
}

//SLE4428不带保护位读取一段数据
uint8_t SLE4428_ReadData_NoProtect(uint16_t start_address, uint16_t length, uint8_t *recvbuff)
{
	uint8_t Byte1,Byte2,Byte3 = 0;
	uint16_t cmd_addr = 0;
	
	for(uint16_t i=0; i<16; i++)
	{
		cmd_addr <<= 1;
		cmd_addr |= (start_address & 0x01);
		start_address >>= 1;
	}
	cmd_addr >>= 6;
	
	Byte1 = 0x70 | (cmd_addr & 0x03);
	Byte2 = cmd_addr >> 2;
	Byte3 = 0x00;
	
	RST_H();
	SLE4428_Delayus(50);
	
	SLE4428_WriteByte(Byte1);
	SLE4428_WriteByte(Byte2);
	SLE4428_WriteByte(Byte3);
	
	SLE4428_Delayus(50);
	RST_L();
	
	SLE4428_ClkPluse();
	
	for(uint8_t i=0; i<length; i++)
	{
		recvbuff[i] = SLE4428_ReadByte_NoProtect();
	}
	
	return 0;
}

//SLE4428带保护位读取一段数据
uint8_t SLE4428_ReadData_Protect(uint16_t start_address, uint16_t length, uint16_t *recvbuff)
{
	uint8_t Byte1,Byte2,Byte3 = 0;
	uint16_t cmd_addr = 0;
	
	for(uint16_t i=0; i<16; i++)
	{
		cmd_addr <<= 1;
		cmd_addr |= (start_address & 0x01);
		start_address >>= 1;
	}
	cmd_addr >>= 6;
	
	Byte1 = 0x30 | (cmd_addr & 0x03);
	Byte2 = cmd_addr >> 2;
	Byte3 = 0x00;
	
	RST_H();
	SLE4428_Delayus(50);
	
	SLE4428_WriteByte(Byte1);
	SLE4428_WriteByte(Byte2);
	SLE4428_WriteByte(Byte3);
	
	SLE4428_Delayus(50);
	RST_L();	
	SLE4428_Delayus(50);
	
	SLE4428_ClkPluse();
	
	for(uint8_t i=0; i<length; i++)
	{
		recvbuff[i] = SLE4428_ReadByte_Protect();
	}
	
	return 0;
}

//SLE4428写错误计数器
uint8_t SLE4428_Write_ErrCnt(void)
{
	static uint8_t cmd3 = 0;
	uint16_t temp = 0;
	SLE4428_ReadData_Protect(1021,1, &temp);
	
	for(uint8_t i=0; i<8; i++)
	{
		if(temp & 0x80)
		{
			cmd3 = temp & 0x7f;
			cmd3 >>= i;
			break;
		}
		else
		{
			temp <<= 1;
		}
	}
	
	RST_H();
	SLE4428_Delayus(50);
	
	SLE4428_WriteByte(0x4f);
	SLE4428_WriteByte(0xbf);
	SLE4428_WriteByte(cmd3);
	
	SLE4428_Delayus(50);
	RST_L();
	SLE4428_Delayus(50);
	//DATA_IN();
	SLE4428_DataIn();
	
	for(uint8_t i=0; i<103; i++)
	{
		SLE4428_ClkPluse();
	}
	CLK_L();
	
	if(DATA_READ())
		return 0;
	else
		return 1;
}

//SLE4428擦除错误计数器
uint8_t SLE4428_Erase_ErrCnt(void)
{
	RST_H();
	SLE4428_Delayus(50);
	
	SLE4428_WriteByte(0xCF);  //write and erase without protect bit
	SLE4428_WriteByte(0xBF);  //address of error count
	SLE4428_WriteByte(0xFF);  //erase error count
	
	SLE4428_Delayus(50);
	RST_L();
	SLE4428_Delayus(50);
	//DATA_IN();
	SLE4428_DataIn();
	
	for(uint8_t i=0; i<103; i++)
	{
		SLE4428_ClkPluse();
	}
	CLK_L();
	
	
	if(DATA_READ())
		return 0;
	else
		return 1;
}

//SLE4428密码验证 第一字节
uint8_t SLE4428_PSC_Byte1(void)
{	
	RST_H();
	SLE4428_Delayus(50);
	
	SLE4428_WriteByte(0xB3);
	SLE4428_WriteByte(0x7F);
	SLE4428_WriteByte(0xFF);
	
	SLE4428_Delayus(50);
	RST_L();
	SLE4428_Delayus(50);
	//DATA_IN();
	SLE4428_DataIn();
	
	for(uint8_t i=0; i<3; i++)
	{
		SLE4428_ClkPluse();
	}
	CLK_L();
	
	if(DATA_READ())
		return 0;
	else
		return 1;
}

//SLE4428密码验证 第二字节
uint8_t SLE4428_PSC_Byte2(void)
{	
	RST_H();
	SLE4428_Delayus(50);
	
	SLE4428_WriteByte(0xB3);
	SLE4428_WriteByte(0xFF);
	SLE4428_WriteByte(0xFF);
	
	SLE4428_Delayus(50);
	RST_L();
	SLE4428_Delayus(50);
	//DATA_IN();
	SLE4428_DataIn();
	
	for(uint8_t i=0; i<3; i++)
	{
		SLE4428_ClkPluse();
	}
	CLK_L();

	if(DATA_READ())
		return 0;
	else
		return 1;
}

uint8_t SLE4428_PSCVerification(void)
{
	uint16_t PSC_Byte1, PSC_Byte2 = 0;
	
	SLE4428_Write_ErrCnt();
	SLE4428_Delayms(100);
	SLE4428_PSC_Byte1();
	SLE4428_Delayms(100);
	SLE4428_PSC_Byte2();
	SLE4428_Delayms(100);
	SLE4428_Erase_ErrCnt();
	SLE4428_Delayms(100);
	
	SLE4428_ReadData_Protect(1022, 1, &PSC_Byte1);
	SLE4428_ReadData_Protect(1023, 1, &PSC_Byte2);
	
	if((PSC_Byte1 == 0x1FF) && (PSC_Byte2 == 0x1FF))
		return 1;
	else
		return 0;
}

//不带保护位向SLE4428写入数据
uint8_t SLE4428_WriteData_Noprotect(uint16_t start_address, uint8_t data)
{
	uint8_t Byte1,Byte2,Byte3 = 0;
	uint16_t cmd_addr = 0;
	
	for(uint16_t i=0; i<16; i++)
	{
		cmd_addr <<= 1;
		cmd_addr |= (start_address & 0x01);
		start_address >>= 1;
	}
	cmd_addr >>= 6;
	
	Byte1 = 0xCC | (cmd_addr & 0x03);
	Byte2 = cmd_addr >> 2;
	
	for(uint8_t i=0; i<8; i++)
	{
		Byte3 <<= 1;
		if(data & 0x01)
			Byte3 |= 0x01;
		data >>= 1;
	}
	
	RST_H();
	SLE4428_Delayus(50);
	
	SLE4428_WriteByte(Byte1);
	SLE4428_WriteByte(Byte2);
	SLE4428_WriteByte(Byte3);
	
	SLE4428_Delayus(50);
	RST_L();	
	SLE4428_Delayus(50);
	DATA_H();
	
	for(uint8_t i=0; i<203; i++)
	{
		SLE4428_ClkPluse();
	}
	CLK_L();
	
	//DATA_IN();
	SLE4428_DataIn();
	if(DATA_READ())
		return 0;
	else
		return 1;
}

//带保护位向SLE4428写入数据
uint8_t SLE4428_WriteData_protect(uint16_t start_address, uint8_t data)
{
	uint8_t Byte1,Byte2,Byte3 = 0;
	uint16_t cmd_addr = 0;
	
	for(uint16_t i=0; i<16; i++)
	{
		cmd_addr <<= 1;
		cmd_addr |= (start_address & 0x01);
		start_address >>= 1;
	}
	cmd_addr >>= 6;
	
	Byte1 = 0x8C | (cmd_addr & 0x03);
	Byte2 = cmd_addr >> 2;
	
	for(uint8_t i=0; i<8; i++)
	{
		Byte3 <<= 1;
		if(data & 0x01)
			Byte3 |= 0x01;
		data >>= 1;
	}
	
	RST_H();
	SLE4428_Delayus(50);
	
	SLE4428_WriteByte(Byte1);
	SLE4428_WriteByte(Byte2);
	SLE4428_WriteByte(Byte3);
	
	SLE4428_Delayus(50);
	RST_L();	
	SLE4428_Delayus(50);
	DATA_H();
	
	for(uint8_t i=0; i<203; i++)
	{
		SLE4428_ClkPluse();
	}
	CLK_L();
	
	//DATA_IN();
	SLE4428_DataIn();
	if(DATA_READ())
		return 0;
	else
		return 1;
}

//SLE4428初始化
void SLE4428_Init(void)
{
	IC_PWR_ON();
}

void SLE4428_IO_Test(void)
{
//	RST_H();
//	CLK_H();
//	DATA_H();

	RST_L();
	CLK_L();
	DATA_L();
}

