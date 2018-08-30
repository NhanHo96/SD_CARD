#include "sd.h"
#include "fatfs.h"
// define SMD for SD card (SPI MODE)	----------------------------------- 0x40 la ma bat buoc khi goi lenh "Start bit + transmission bit + 6 bit m� lenh"  01xxxxxx
#define CMD0 (0x40+0) // reset sd card ve trang thai roi
#define CMD1 (0x40+1) // yeu cau goi noi dung thanh ghi OCR
#define ACMD41 (0xC0+41) // SEND_OP_COND (SDC) NHAN DANG THE NHO
#define CMD8 (0x40+8) // YEU CAU GOI NOI DUNG THONG TIN CSD o dang block
#define CMD9 (0x40+9) // SEND_CSD
#define CMD16 (0x40+16) // SET_BLOCKLEN
#define CMD17 (0x40+17) // READ_SINGLE_BLOCK
#define CMD24 (0x40+24) // WRITE_BLOCK
#define CMD55 (0x40+55) // APP_CMD bao sd card lenh tiep theo ACMD41
#define CMD58 (0x40+58) // READ_OCR
//-----------------------------------------------------------
extern volatile uint16_t Timer1;
extern SPI_HandleTypeDef hspi2;
sd_info_ptr sdinfo;

//	Error SD	-----------------------------------------------
static void Error (void)
{
  LD_ON;
}
//	Cho cap nguon SD	---------------------------------------
void SD_PowerOn(void)
{
  Timer1 = 0;
  while(Timer1<1);
}
//-------------------------------------------------------------
uint8_t SPIx_WriteRead(uint8_t Byte)
{
	uint8_t receivedbyte = 0;
  if(HAL_SPI_TransmitReceive(&hspi2,(uint8_t*) &Byte,(uint8_t*) &receivedbyte,1,0x1000)!=HAL_OK)
  {
    Error();
  }
  return receivedbyte;
}
//------------------------------------------------------------
void SPI_SendByte(uint8_t bt)
{
  SPIx_WriteRead(bt);
}
//------------------------------------------------------------
uint8_t SPI_ReceiveByte(void)
{
  uint8_t bt = SPIx_WriteRead(0xFF);
  return bt;
}
//------------------------------------------------------------
void SPI_Release(void)
{
  SPIx_WriteRead(0xFF);
}
//-----------------------------------------------
uint8_t SPI_wait_ready(void)
{
  uint8_t res;
  uint16_t cnt;
  cnt=0;
  do { //busy
    res=SPI_ReceiveByte();
    cnt++;
  } while ( (res!=0xFF)&&(cnt<0xFFFF) );
  if (cnt>=0xFFFF) return 1;
  return res;
}
//	Chon SMD SD Card	---------------------------------------
static uint8_t SD_cmd (uint8_t cmd, uint32_t arg)
{
  uint8_t n, res;
	// ACMD<n> is the command sequense of CMD55-CMD<n>
	if (cmd & 0x80)// neu cmd55
	{
		cmd &= 0x7F;
		res = SD_cmd(CMD55, 0);
		if (res > 1) return res;
	}
	// Select the card
	SS_SD_DESELECT();
	SPI_ReceiveByte();
	SS_SD_SELECT();
	SPI_ReceiveByte();
	// Send a command packet
	SPI_SendByte(cmd); // Start + Command index
	SPI_SendByte((uint8_t)(arg >> 24)); // Argument[31..24]
	SPI_SendByte((uint8_t)(arg >> 16)); // Argument[23..16]
	SPI_SendByte((uint8_t)(arg >> 8)); // Argument[15..8]
	SPI_SendByte((uint8_t)arg); // Argument[7..0]
	n = 0x01; // Dummy CRC + Stop
	if (cmd == CMD0) {n = 0x95;} // Valid CRC for CMD0(0)
	if (cmd == CMD8) {n = 0x87;} // Valid CRC for CMD8(0x1AA)
	SPI_SendByte(n);	
  // Receive a command response
  n = 10; // Wait for a valid response in timeout of 10 attempts
  do {
    res = SPI_ReceiveByte();
  } while ((res & 0x80) && --n);
  return res;
}
//------------------------------------------------------------
uint8_t SD_Read_Block (uint8_t *buff, uint32_t lba)
{
  uint8_t result;
  uint16_t cnt;
	result=SD_cmd (CMD17, lba); //lenh yeu cau doc block CMD17
	if (result!=0x00) return 5;
	  SPI_Release();
  cnt=0;
  do{
    result=SPI_ReceiveByte();
    cnt++;
  } while ( (result!=0xFE)&&(cnt<0xFFFF) );
  if (cnt>=0xFFFF) return 5;
  for (cnt=0;cnt<512;cnt++) buff[cnt]=SPI_ReceiveByte(); 
  SPI_Release(); 
  SPI_Release();
  return 0;
}
//--------------------------------------------------------------
uint8_t SD_Write_Block (uint8_t *buff, uint32_t lba)
{
  uint8_t result;
  uint16_t cnt;
  result=SD_cmd(CMD24,lba); //CMD24   write single block
  if (result!=0x00) return 6; 
  SPI_Release();
  SPI_SendByte (0xFE); // 0xFE n�y l� data token, b�o cho card mcu san s�ng
  for (cnt=0;cnt<512;cnt++) SPI_SendByte(buff[cnt]); 
  SPI_Release(); 
  SPI_Release();
  result=SPI_ReceiveByte();
  if ((result&0x05)!=0x05) return 6; 
  cnt=0;
  do { 
    result=SPI_ReceiveByte();
    cnt++;
  } while ( (result!=0xFF)&&(cnt<0xFFFF) );
  if (cnt>=0xFFFF) return 6;
  return 0;
}
//	initial SD Card	------------------------------------------
uint8_t sd_ini(void)
{
	uint8_t i,cmd;
  int16_t tmr;
  uint32_t temp;
  LD_OFF;
  sdinfo.type = 0;
	uint8_t ocr[4];
	temp = hspi2.Init.BaudRatePrescaler;		//GAN TEMP=TAN SO KHAI BAO TRONG CUBE
	hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128; //GIAM TOC DO TRUYEN NHAN SD CADRD 156.25 kbbs
	HAL_SPI_Init(&hspi2);//KHOI DONG SPI
	SS_SD_DESELECT(); //SET CS ==> STM LA MASTER
	for(i=0;i<10;i++)  //SEND 10byte 0xFF TO SD THROUGH MOSI DE TAO IT NHAT 80 XUNG CLOCK TREN CHAN CLK
		SPI_Release();
	hspi2.Init.BaudRatePrescaler = temp;		//CAU HINH LAI BAURATE SPI2
	HAL_SPI_Init(&hspi2);
	SS_SD_SELECT();	//RESET CS  ==> STM LA SLAVE
	//SPI_SendByte(0x35);		//????
	//SPI_SendByte(0x53);		//????
	if (SD_cmd(CMD0, 0) == 1) // Enter Idle state
  {
		SPI_Release();
		if (SD_cmd(CMD8, 0x1AA) == 1) // SDv2
		{
			for (i = 0; i < 4; i++) ocr[i] = SPI_ReceiveByte();
			// Get trailing return value of R7 resp
			if (ocr[2] == 0x01 && ocr[3] == 0xAA) // The card can work at vdd range of 2.7-3.6V
			{
				for (tmr = 12000; tmr && SD_cmd(ACMD41, 1UL << 30); tmr--)
					 ;// Wait for leaving idle state (ACMD41 with HCS bit)
				if (tmr && SD_cmd(CMD58, 0) == 0) 
				{ // Check CCS bit in the OCR
						for (i = 0; i < 4; i++) 
								ocr[i] = SPI_ReceiveByte();
						sdinfo.type = (ocr[0] & 0x40) ? CT_SD2 | CT_BLOCK : CT_SD2; // SDv2 (HC or SC)
				}
			}
		}
		else //SDv1 or MMCv3
		{
			if (SD_cmd(ACMD41, 0) <= 1)
			{
				sdinfo.type = CT_SD1; cmd = ACMD41; // SDv1
			}
			else
			{
				sdinfo.type = CT_MMC; cmd = CMD1; // MMCv3
			}
			for (tmr = 25000; tmr && SD_cmd(cmd, 0); tmr--) ; // Wait for leaving idle state
			if (!tmr || SD_cmd(CMD16, 512) != 0) // Set R/W block length to 512
			sdinfo.type = 0;
		}
  }
  else
  {
    return 1;
  }
  //sprintf(str1,"Type SD: 0x%02X\r\n",sdinfo.type);
  return 0;
}


