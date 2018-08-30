#ifndef MAIN_H_
#define MAIN_H_

#include "stm32f1xx_hal.h"
#include "fatfs.h"
#include "sd.h"

SPI_HandleTypeDef hspi2;
TIM_HandleTypeDef htim2;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM2_Init(void);
void initial(void);
FRESULT ReadLongFile(void);
void Read_SD(void);
void Write_SD(void);
void _Error_Handler(char * file, int line);
void Error_Handler(void);

volatile uint16_t Timer1=0;
uint8_t sect[512];
extern char str1[60];

uint32_t byteswritten,bytesread;
uint8_t result;
extern char USER_Path[4]; /* logical drive path */
FATFS SDFatFs;
FATFS *fs;
FIL MyFile;
FRESULT res; 
uint8_t wtext[]="Hello from STM32!!!";
FILINFO fileInfo;
char *fn;
DIR dir;
DWORD fre_clust, fre_sect, tot_sect;
#endif /* MAIN_H_ */
