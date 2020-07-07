#ifndef BOOTLOADER_H_
#define BOOTLOADER_H_

#ifdef STM32F103xB
#define USE_USB_CDC 1
#define BL_AUTO_BAUD 1
#endif

#ifdef STM32F103xE
#define USE_USB_CDC 1
#define BL_AUTO_BAUD 1
#endif

#ifdef STM32F401xE
#define USE_USB_CDC 0
#define BL_AUTO_BAUD 1
#endif

#ifdef STM32F407xx
#define USE_USB_CDC 1
#define BL_AUTO_BAUD 0
#endif

void BL_Main(void);

#endif /* BOOTLOADER_H_ */
