#include "LPC17xx.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define PCLK 25000000
#define BAUDRATE 9600

#define SCK  (1 << 7)
#define MISO (1 << 8)
#define MOSI (1 << 9)
#define SSEL (1 << 6)

#define CommandReg         0x01
#define ModeReg            0x11
#define TxControlReg       0x14
#define TxAutoReg          0x15
#define TModeReg           0x2A
#define TPrescalerReg      0x2B
#define TReloadRegH        0x2C
#define TReloadRegL        0x2D
#define CommIEnReg         0x02
#define BitFramingReg      0x0D
#define FIFODataReg        0x09
#define FIFOLevelReg       0x0A
#define CommIrqReg         0x04
#define ErrorReg           0x06
#define ControlReg         0x0C

#define PCD_IDLE           0x00
#define PCD_RESETPHASE     0x0F
#define PCD_TRANSCEIVE     0x0C
#define PCD_AUTHENT        0x0E

#define PICC_REQIDL        0x26
#define PICC_ANTICOLL      0x93

#define MI_OK              1
#define MI_ERR             0
#define MI_NOTAGERR        2
#define MAX_LEN            16

struct CardInfo {
    uint8_t uid[5];
    char name[32];
    char cardType[16];
}; 
struct CardInfo cards[] = {
    {{0xD4, 0x95, 0xFC, 0x03, 0xBE}, "Pooviga N", "23ECR152"},
    {{0xB3, 0x2F, 0x83, 0x2D, 0x32}, "Pavithra s", "23ECR154"},
		{{0xF3, 0x74, 0x7C, 0x14, 0xEF}, "priya", "23ECR156"},
    {{0xCB, 0x20, 0x93, 0x5F, 0x27 }, "Praga", "23ECR167"}
};

#define NUM_CARDS (sizeof(cards) / sizeof(cards[0]))

void delay_ms(uint32_t ms) {
    LPC_SC->PCONP |= (1 << 1);
    LPC_TIM0->CTCR = 0x0;
    LPC_TIM0->PR = 25000 - 1;
    LPC_TIM0->TCR = 0x02;
    LPC_TIM0->TCR = 0x01;
    while (LPC_TIM0->TC < ms);
    LPC_TIM0->TCR = 0x00;
    LPC_TIM0->TC = 0;
}

void UART0_Init(void) {
    LPC_PINCON->PINSEL0 |= 0x50;
    LPC_UART0->LCR = 0x83;
    LPC_UART0->DLM = 0;
    LPC_UART0->DLL = PCLK / (16 * BAUDRATE);
    LPC_UART0->LCR = 0x03;
}

void UART0_SendChar(char c) {
    while (!(LPC_UART0->LSR & (1 << 5)));
    LPC_UART0->THR = c;
}

void UART0_SendString(const char *str) {
    while (*str) UART0_SendChar(*str++);
}

void SPI_INIT(void) {
    LPC_SC->PCONP |= (1 << 21);
    LPC_PINCON->PINSEL0 |= (0x02 << 14) | (0x02 << 16) | (0x02 << 18);
    LPC_GPIO0->FIODIR |= SSEL;
    LPC_GPIO0->FIOSET = SSEL;
    LPC_SSP1->CR0 = 0x0707;
    LPC_SSP1->CPSR = 8;
    LPC_SSP1->CR1 = 0x02;
}

uint8_t SPI_Transfer(uint8_t data) {
    LPC_SSP1->DR = data;
    while (LPC_SSP1->SR & (1 << 4));
    return LPC_SSP1->DR;
}

void CS_LOW(void)  { LPC_GPIO0->FIOCLR = SSEL; }
void CS_HIGH(void) { LPC_GPIO0->FIOSET = SSEL; }

void RFID_WriteReg(uint8_t reg, uint8_t val) {
    CS_LOW();
    SPI_Transfer((reg << 1) & 0x7E);
    SPI_Transfer(val);
    CS_HIGH();
}

uint8_t RFID_ReadReg(uint8_t reg) {
    uint8_t val;
    CS_LOW();
    SPI_Transfer(((reg << 1) & 0x7E) | 0x80);
    val = SPI_Transfer(0x00);
    CS_HIGH();
    return val;
}

void MFRC522_SetBitMask(uint8_t reg, uint8_t mask) {
    uint8_t tmp = RFID_ReadReg(reg);
    RFID_WriteReg(reg, tmp | mask);
}

void MFRC522_ClearBitMask(uint8_t reg, uint8_t mask) {
    uint8_t tmp = RFID_ReadReg(reg);
    RFID_WriteReg(reg, tmp & (~mask));
}

void RFID_AntennaOn(void) {
    uint8_t val = RFID_ReadReg(TxControlReg);
    if (!(val & 0x03)) {
        RFID_WriteReg(TxControlReg, val | 0x03);
    }
}

void RFID_Init(void) {
    RFID_WriteReg(CommandReg, PCD_RESETPHASE);
    delay_ms(50);
    RFID_WriteReg(TModeReg, 0x8D);
    RFID_WriteReg(TPrescalerReg, 0x3E);
    RFID_WriteReg(TReloadRegL, 0x1E);
    RFID_WriteReg(TReloadRegH, 0);
    RFID_WriteReg(TxAutoReg, 0x40);
    RFID_WriteReg(ModeReg, 0x3D);
    RFID_AntennaOn();
}

uint8_t RFID_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint32_t *backLen) {
    uint8_t status = MI_ERR;
    uint8_t irqEn = 0x00;
    uint8_t waitIRq = 0x00;
    uint8_t lastBits;
    uint8_t n;
    uint16_t i;

    if (command == PCD_AUTHENT) {
        irqEn = 0x12;
        waitIRq = 0x10;
    }
    if (command == PCD_TRANSCEIVE) {
        irqEn = 0x77;
        waitIRq = 0x30;
    }

    RFID_WriteReg(CommIEnReg, irqEn | 0x80);
    RFID_WriteReg(CommIrqReg, 0x7F);
    RFID_WriteReg(FIFOLevelReg, 0x80);
    RFID_WriteReg(CommandReg, PCD_IDLE);

    for (i = 0; i < sendLen; i++) {
        RFID_WriteReg(FIFODataReg, sendData[i]);
    }

    RFID_WriteReg(CommandReg, command);
    if (command == PCD_TRANSCEIVE) {
        MFRC522_SetBitMask(BitFramingReg, 0x80);
    }

    i = 2000;
    do {
        n = RFID_ReadReg(CommIrqReg);
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & waitIRq));

    MFRC522_ClearBitMask(BitFramingReg, 0x80);

    if (i != 0) {
        if (!(RFID_ReadReg(ErrorReg) & 0x1B)) {
            status = MI_OK;
            if (n & irqEn & 0x01) status = MI_NOTAGERR;

            if (command == PCD_TRANSCEIVE) {
                n = RFID_ReadReg(FIFOLevelReg);
                lastBits = RFID_ReadReg(ControlReg) & 0x07;
                *backLen = lastBits ? (n - 1) * 8 + lastBits : n * 8;

                if (n == 0) n = 1;
                if (n > MAX_LEN) n = MAX_LEN;

                for (i = 0; i < n; i++) {
                    backData[i] = RFID_ReadReg(FIFODataReg);
                }
            }
        } else {
            status = MI_ERR;
        }
    }

    return status;
}

uint8_t MFRC522_Request(uint8_t reqMode, uint8_t *tagType) {
    uint8_t status;
    uint32_t backBits;
    RFID_WriteReg(BitFramingReg, 0x07);
    tagType[0] = reqMode;
    status = RFID_ToCard(PCD_TRANSCEIVE, tagType, 1, tagType, &backBits);
    return status;
}

uint8_t MFRC522_Anticoll(uint8_t *serNum) {
    uint8_t status;
    uint8_t i;
    uint8_t serNumCheck = 0;
    uint32_t unLen;

    RFID_WriteReg(BitFramingReg, 0x00);
    serNum[0] = PICC_ANTICOLL;
    serNum[1] = 0x20;
    status = RFID_ToCard(PCD_TRANSCEIVE, serNum, 2, serNum, &unLen);

    if (status == MI_OK) {
        for (i = 0; i < 4; i++) serNumCheck ^= serNum[i];
        if (serNumCheck != serNum[4]) status = MI_ERR;
    }
    return status;
}

bool match_card(uint8_t *uid) {
	int count = 0;
	uint8_t arr[5];
	
    for (int i = 0; i < NUM_CARDS; i++) {
        bool match = true;
        for (int j = 0; j < 5; j++) {
            if (cards[i].uid[j] != uid[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            UART0_SendString("\nCard Matched: ");
            UART0_SendString(cards[i].name);
            UART0_SendString(" - ");
            UART0_SendString(cards[i].cardType);
            UART0_SendString("\r\n");
            return true;
        }
    }
    UART0_SendString("Unknown Card\r\n");
    return false;
}
int main(void) {
    uint8_t status;
    uint8_t tagType[2];
    uint8_t serNum[5];
    char buf[64];

    SystemInit();
    UART0_Init();
    SPI_INIT();
    RFID_Init();

    UART0_SendString("RFID Initialized\r\n");

    while (1) {
        status = MFRC522_Request(PICC_REQIDL, tagType);
        if (status == MI_OK) {
            status = MFRC522_Anticoll(serNum);
            if (status == MI_OK) {
                UART0_SendString("UID: ");
                for (int i = 0; i < 5; i++) {
                    sprintf(buf, "%02X ", serNum[i]);
                    UART0_SendString(buf);
									
                }
                UART0_SendString("\r\n");
								match_card(serNum);
            }
        } else {
            UART0_SendString("No card\r\n");
        }
        delay_ms(1000);
    }
}