/*
	LPCUSB, an USB device driver for LPC microcontrollers	
	Copyright (C) 2006 Bertrik Sikken (bertrik@sikken.nl)

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
	This is the SCSI layer of the USB mass storage application example.
	This layer depends directly on the blockdev layer.
*/


#include <string.h>		// memcpy

#include "type.h"
#include "usbdebug.h"

#include "blockdev.h"
#include "msc_scsi.h"


#define BLOCKSIZE		512

#define INVALID_FIELD_IN_CDB	0x052400
#define INVALID_CMD_OPCODE		0x052000

//	Sense code, which is set on error conditions
static U32			dwSense;	// hex: 00aabbcc, where aa=KEY, bb=ASC, cc=ASCQ

static const U8		abInquiry[] = {
	0x00,		// PDT = direct-access device
	0x80,		// removeable medium bit = set
	0x04,		// version = complies to SPC2r20
	0x02,		// response data format = SPC2r20
	0x1F,		// additional length
	0x00,
	0x00,
	0x00,
	'L','P','C','U','S','B',' ',' ',	// vendor
	'M','a','s','s',' ','s','t','o',	// product
	'r','a','g','e',' ',' ',' ',' ',
	'0','.','1',' '						// revision
};

//	Data for "request sense" command. The 0xFF are filled in later
static U8 const abSense[] = { 0x70, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x0A, 
							  0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
							  0x00, 0x00 };

//	Buffer for holding one block of disk data
static U8			abBlockBuf[512];


typedef struct {
	U8		bOperationCode;
	U8		abLBA[3];
	U8		bLength;
	U8		bControl;
} TCDB6;


/*************************************************************************
	SCSIReset
	=========
		Resets any SCSI state
		
**************************************************************************/
void SCSIReset(void)
{
	dwSense = 0;
}


/*************************************************************************
	SCSIHandleCmd
	=============
		Verifies a SCSI CDB and indicates the direction and amount of data
		that the device wants to transfer.
		
	IN		pbCDB		Command data block
			iCDBLen		Command data block len
	OUT		*piRspLen	Length of intended response data:
			*pfDevIn	TRUE if data is transferred from device-to-host
	
	Returns TRUE if CDB is verified, FALSE otherwise.
	If this call fails, a sense code is set in dwSense.
**************************************************************************/
BOOL SCSIHandleCmd(U8 *pbCDB, int iCDBLen, int *piRspLen, BOOL *pfDevIn)
{
	int		i;
	TCDB6	*pCDB;
	U32		dwLen, dwLBA;
	
	pCDB = (TCDB6 *)pbCDB;
	
	// default direction is from device to host
	*pfDevIn = TRUE;
	
	switch (pCDB->bOperationCode) {

	// test unit ready (6)
	case 0x00:
		DBG("TEST UNIT READY\n");
		*piRspLen = 0;
		return TRUE;
	
	// request sense (6)
	case 0x03:
		DBG("REQUEST SENSE (%06X)\n", dwSense);
		// check params
		*piRspLen = MIN(18, pCDB->bLength);
		return TRUE;
	
	// inquiry (6)
	case 0x12:
		DBG("INQUIRY\n");
		// see SPC20r20, 4.3.4.6
		*piRspLen = MIN(36, pCDB->bLength);
		return TRUE;
		
	// read capacity (10)
	case 0x25:
		DBG("READ CAPACITY\n");
		*piRspLen = 8;
		return TRUE;
		
	// read (10)
	case 0x28:
		dwLBA = (pbCDB[2] << 24) | (pbCDB[3] << 16) | (pbCDB[4] << 8) | (pbCDB[5]);
		dwLen = (pbCDB[7] << 8) | pbCDB[8];
		DBG("READ10, LBA=%d, len=%d\n", dwLBA, dwLen);
		*piRspLen = dwLen * BLOCKSIZE;
		return TRUE;

	// write (10)
	case 0x2A:
		dwLBA = (pbCDB[2] << 24) | (pbCDB[3] << 16) | (pbCDB[4] << 8) | (pbCDB[5]);
		dwLen = (pbCDB[7] << 8) | pbCDB[8];
		DBG("WRITE10, LBA=%d, len=%d\n", dwLBA, dwLen);
		*piRspLen = dwLen * BLOCKSIZE;
		*pfDevIn = FALSE;
		return TRUE;

	default:
		DBG("Unhandled SCSI: ");		
		for (i = 0; i < iCDBLen; i++) {
			DBG(" %02X", pbCDB[i]);
		}
		DBG("\n");
		// unsupported command
		dwSense = INVALID_CMD_OPCODE;
		*piRspLen = 0;
		return FALSE;
	}
}


/*************************************************************************
	SCSIHandleData
	==============
		Handles a block of SCSI data.
		
	IN		pbCDB		Command data block
			iCDBLen		Command data block len
	IN/OUT	pbData		Data buffer
	IN		dwOffset	Offset in data
	
	Returns TRUE if the data was processed successfully
**************************************************************************/
BOOL SCSIHandleData(U8 *pbCDB, int iCDBLen, U8 *pbData, U32 dwOffset)
{
	TCDB6	*pCDB;
	U32		dwLBA;
	U32		dwBufPos, dwBlockNr;
	U32		dwNumBlocks, dwMaxBlock;
	
	pCDB = (TCDB6 *)pbCDB;
	
	switch (pCDB->bOperationCode) {

	// test unit ready
	case 0x00:
		return (dwSense == 0);
	
	// request sense
	case 0x03:
		memcpy(pbData, abSense, 18);
		// fill in KEY/ASC/ASCQ
		pbData[2] = (dwSense >> 16) & 0xFF;
		pbData[12] = (dwSense >> 8) & 0xFF;
		pbData[13] = (dwSense >> 0) & 0xFF;
		// reset sense data
		dwSense = 0;
		break;
	
	// inquiry
	case 0x12:
		memcpy(pbData, abInquiry, sizeof(abInquiry));
		break;
		
	// read capacity
	case 0x25:
		// get size of drive (bytes)
		BlockDevGetSize(&dwNumBlocks);
		// calculate highest LBA
		dwMaxBlock = (dwNumBlocks - 1) / 512;
		
		pbData[0] = (dwMaxBlock >> 24) & 0xFF;
		pbData[1] = (dwMaxBlock >> 16) & 0xFF;
		pbData[2] = (dwMaxBlock >> 8) & 0xFF;
		pbData[3] = (dwMaxBlock >> 0) & 0xFF;
		pbData[4] = (BLOCKSIZE >> 24) & 0xFF;
		pbData[5] = (BLOCKSIZE >> 16) & 0xFF;
		pbData[6] = (BLOCKSIZE >> 8) & 0xFF;
		pbData[7] = (BLOCKSIZE >> 0) & 0xFF;
		break;
		
	// read10
	case 0x28:
		dwLBA = (pbCDB[2] << 24) | (pbCDB[3] << 16) | (pbCDB[4] << 8) | (pbCDB[5]);

		// copy data from block buffer
		dwBufPos = (dwOffset & 511);
		if (dwBufPos == 0) {
			// read new block
			dwBlockNr = dwLBA + (dwOffset / 512);
			DBG("R");
			if (BlockDevRead(dwBlockNr, abBlockBuf) < 0) {
				DBG("BlockDevRead failed\n");
				return FALSE;
			}
		}
		// inefficient but simple
		memcpy(pbData, abBlockBuf + dwBufPos, 64);
		break;

	// write10
	case 0x2A:
		dwLBA = (pbCDB[2] << 24) | (pbCDB[3] << 16) | (pbCDB[4] << 8) | (pbCDB[5]);
		
		// copy data to block buffer
		dwBufPos = (dwOffset & 511);
		memcpy(abBlockBuf + dwBufPos, pbData, 64);
		if (dwBufPos == (512-64)) {
			// write new block
			dwBlockNr = dwLBA + (dwOffset / 512);
			DBG("W");
			if (BlockDevWrite(dwBlockNr, abBlockBuf) < 0) {
				DBG("BlockDevWrite failed\n");
				return FALSE;
			}
		}
		break;
		
	default:
		return FALSE;
	}
	
	return TRUE;
}

