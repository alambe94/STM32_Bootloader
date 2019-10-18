/*
 * bootloader.h
 *
 *  Created on: Sep 25, 2019
 *      Author: medprime
 */

#ifndef BOOTLOADER_H_
#define BOOTLOADER_H_


#include "stm32f4xx_hal.h"


void BL_Init();
void BL_Main_Loop();
uint8_t BL_Erase_Flash();
void BL_Add_Commands();
#endif /* BOOTLOADER_H_ */
