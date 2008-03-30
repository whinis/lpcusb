#include "type.h"
#include "debug.h"

#include "spi.h"

#include "sdcard.h"

#define R1_IDLE_STATE			(1<<0)
#define R1_ERASE_RESET			(1<<1)
#define R1_ILLEGAL_COMMAND		(1<<2)
#define R1_COM_CRC_ERROR		(1<<3)
#define R1_ERASE_SEQ_ERROR		(1<<4)
#define R1_ADDRESS_ERROR		(1<<5)
#define R1_PARAMETER_ERROR		(1<<6)
#define R1_INVALID_ERROR		(1<<7)

#define R2_CARD_LOCKED			(1<<0)
#define R2_WP_ERROR				(1<<1)
#define R2_ERROR				(1<<2)
#define R2_CC_ERROR				(1<<3)
#define R2_CARD_ECC_FAILED		(1<<4)
#define R2_WP_VIOLATION			(1<<5)
#define R2_ERASE_PARAMETER		(1<<6)
#define R2_OUT_OF_RANGE			(1<<7)

#define CMD_GO_IDLE_STATE			0			
#define CMD_SEND_OP_COND			1
#define CMD_SWITCH_FUNC				6
#define CMD_SEND_CSD				9
#define CMD_SEND_CID				10
#define CMD_STOP_TRANSMISSION		12
#define CMD_SEND_STATUS				13
#define CMD_SET_BLOCKLEN			16
#define CMD_READ_SINGLE_BLOCK		17
#define CMD_READ_MULTIPLE_BLOCK		18
#define CMD_WRITE_BLOCK				24
#define CMD_WRITE_MULTIPLE_BLOCK	25
#define CMD_PROGRAM_CSD				27
#define CMD_SET_WRITE_PROT			28
#define CMD_CLR_WRITE_PROT			29
#define CMD_SEND_WRITE_PROT			30
#define CMD_ERASE_WR_BLK_START_ADDR	32
#define CMD_ERASE_WR_BLK_END_ADDR	33
#define CMD_ERASE					38
#define CMD_LOCK_UNLOCK				42
#define CMD_APP_CMD					55	
#define CMD_GEN_CMD					56
#define CMD_READ_OCR				58
#define CMD_CRC_ON_OFF				59

// tokens
#define TOKEN_START_MULT_BLOCK		0xFC
#define TOKEN_STOP_TRAN				0xFD
#define TOKEN_START_BLOCK			0xFE


#define SD_BLOCK_SIZE				512

// timing
#define	NCR			8
#define NAC			1024		// actually much more complex, TODO
#define NWR			1			// (bytes) time between write response and data block


// returns an R1 error code
static U8 SDWaitResp(int iTimeout)
{
	U8	bResp;
	int	i;
	
	bResp = 0xFF;
	for (i = 0; i < iTimeout; i++) {
		SPITransfer(1, NULL, &bResp);
		if (bResp != 0xFF) {
			break;
		}
	}
	return bResp;
}


// returns an R1 error code
static U8 SDCommand(U8 bCmd, U32 ulParam)
{
	U8	abBuf[6];
	U8	bResp;
	
	// check if card is busy
	SPITransfer(1, NULL, &bResp);
	if (bResp != 0xFF) {
		DBG("Card busy (0x%02X)!\n", bResp);
		return bResp;
	}
	
	// write command
	abBuf[0] = bCmd | 0x40;
	abBuf[1] = ulParam >> 24;
	abBuf[2] = ulParam >> 16;
	abBuf[3] = ulParam >> 8;
	abBuf[4] = ulParam >> 0;
	abBuf[5] = 0x95;
	SPITransfer(6, abBuf, NULL);
	
	// wait for response
	return SDWaitResp(NCR);
}


static BOOL SDReadDataToken(U8 bType, U8 *pbData, int iLen)
{
	U8	bResp;

	// wait for data token
	bResp = SDWaitResp(NAC);
	if (bResp != bType) {
		DBG("Expected start block token, got %X instead!\n", bResp);
		return FALSE;
	}
	
	// read data
	SPITransfer(iLen, NULL, pbData);
	
	// skip CRC
	SPITransfer(2, NULL, NULL);
	
	return TRUE;
}


static BOOL SDWriteDataToken(U8 bType, U8 *pbData, int iLen)
{
	U8	bResp;

	// NWR
	SPITransfer(1, NULL, NULL);

	// data token
	SPITransfer(1, &bType, NULL);
	if (iLen != 0) {
		// send data
	SPITransfer(iLen, pbData, NULL);
	// (fake) CRC
	SPITransfer(2, NULL, NULL);
	// get data response
	SPITransfer(1, NULL, &bResp);
	if ((bResp & 0x1F) != 5) {
		DBG("Received data response error (0x%02X)!\n", bResp);
		return FALSE;
	}
	}
	
	// wait while busy
	do {
		SPITransfer(1, NULL, &bResp);
	} while (bResp != 0xFF);
	
	return TRUE;
}


BOOL SDInit(void)
{
	int i;
	U8	bResp;

	// init SPI subsystem
	SPIInit();

	// set low SPI speed
	SPISetSpeed(400000);

	// send at least 74 clocks with no chip select
	SPITick(10);

	// send CMD_GO_IDLE_STATE
	for (i = 0; i < 100; i++) {
		bResp = SDCommand(CMD_GO_IDLE_STATE, 0);
		if (bResp != 0xFF) {
			break;
		}
	}
	if (bResp != R1_IDLE_STATE) {
		DBG("CMD_GO_IDLE_STATE failed (0x%02X)!\n", bResp);
		return FALSE;
	}
	
	// send CMD_SEND_OP_COND
	for (i = 0; i < 1024; i++) {
		bResp = SDCommand(CMD_SEND_OP_COND, 0);
		if (bResp == 0) {
			break;
		}
	}
	if (bResp != 0) {
		DBG("CMD_SEND_OP_COND failed (0x%02X)!\n", bResp);
		return FALSE;
	}
	
	// set high SPI speed
	SPISetSpeed(25000000);

	return TRUE;
}


BOOL SDReadBlock(U8 *pbData, U32 ulBlock)
{
	U32	ulAddr;
	U8	bResp;
	
	ulAddr = ulBlock * SD_BLOCK_SIZE;
	
	// write command
	if ((bResp = SDCommand(CMD_READ_SINGLE_BLOCK, ulAddr)) != 0) {
		DBG("CMD_READ_SINGLE_BLOCK failed (0x%02X)!\n", bResp);
		return FALSE;
	}
	
	// read data token
	if (!SDReadDataToken(TOKEN_START_BLOCK, pbData, SD_BLOCK_SIZE)) {
		DBG("SDReadDataToken failed!\n");
		return FALSE;
	}
	
	return TRUE;
}



BOOL SDWriteBlock(U8 *pbData, U32 ulBlock)
{
	U32	ulAddr;
	U8	bResp;
	
	ulAddr = ulBlock * SD_BLOCK_SIZE;
	
	// write command
	if ((bResp = SDCommand(CMD_WRITE_BLOCK, ulAddr)) != 0) {
		DBG("CMD_WRITE_BLOCK failed (0x%02X)!\n", bResp);
		return FALSE;
	}
	
	// write data token
	if (!SDWriteDataToken(TOKEN_START_BLOCK, pbData, SD_BLOCK_SIZE)) {
		DBG("SDWriteDataToken failed!\n");
		return FALSE;
	}
	
	return TRUE;
}


BOOL SDReadCSD(U8 *pbCSD)
{
	U8	bResp;

	// write command
	if ((bResp = SDCommand(CMD_SEND_CSD, 0)) != 0) {
		DBG("CMD_SEND_CSD failed (0x%02X)!\n", bResp);
		return FALSE;
	}
	
	// wait for data token
	if (!SDReadDataToken(TOKEN_START_BLOCK, pbCSD, 16)) {
		DBG("SDReadDataToken failed!\n");
		return FALSE;
	}
	
	return TRUE;
}
	
	
BOOL SDReadCID(U8 *pbCID)
{
	U8	bResp;

	// write command
	if ((bResp = SDCommand(CMD_SEND_CID, 0)) != 0) {
		DBG("CMD_SEND_CID failed (0x%02X)!\n", bResp);
		return FALSE;
	}
	
	// wait for data token
	if (!SDReadDataToken(TOKEN_START_BLOCK, pbCID, 16)) {
		DBG("SDReadDataToken failed!\n");
		return FALSE;
	}

	return TRUE;
}
	