/*
	LPCUSB, an USB device driver for LPC microcontrollers	
	Copyright (C) 2006 Bertrik Sikken (bertrik@sikken.nl)

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright
	   notice, this list of conditions and the following disclaimer.
	2. Redistributions in binary form must reproduce the above copyright
	   notice, this list of conditions and the following disclaimer in the
	   documentation and/or other materials provided with the distribution.
	3. The name of the author may not be used to endorse or promote products
	   derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
	IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
	OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
	IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, 
	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
	NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
	DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
	THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
	THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


/** @file
	Standard request handler.
	
	This modules handles the 'chapter 9' processing, specifically the
	standard device requests in table 9-3 from the universal serial bus
	specification revision 2.0
	
	Specific types of devices may specify additional requests (for example
	HID devices add a GET_DESCRIPTOR request for interfaces), but they
	will not be part of this module.

	@todo some requests have to return a request error if device not configured:
	@todo GET_INTERFACE, GET_STATUS, SET_INTERFACE, SYNCH_FRAME
	@todo this applies to the following if endpoint != 0:
	@todo SET_FEATURE, GET_FEATURE 
*/

#include "debug.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "usbstruct.h"
#include "usbapi.h"
#include "usbhw_lpc.h"

#define MAX_DESC_HANDLERS	4		/**< device, interface, endpoint, other */


/* general descriptor field offsets */
#define DESC_bLength					0	/**< length offset */
#define DESC_bDescriptorType			1	/**< descriptor type offset */	

/* config descriptor field offsets */
#define CONF_DESC_wTotalLength			2	/**< total length offset */
#define CONF_DESC_bConfigurationValue	5	/**< configuration value offset */	
#define CONF_DESC_bmAttributes			7	/**< configuration characteristics */

/* interface descriptor field offsets */
#define INTF_DESC_bAlternateSetting		3	/**< alternate setting offset */

/* endpoint descriptor field offsets */
#define ENDP_DESC_bEndpointAddress		2	/**< endpoint address offset */
#define ENDP_DESC_wMaxPacketSize		4	/**< maximum packet size offset */


/** Currently selected configuration */
static uint8_t				bConfiguration = 0;
/** Installed custom request handler */
static TFnHandleRequest	*pfnHandleCustomReq = NULL;
/** Pointer to registered descriptors */
static const uint8_t			*pabDescrip = NULL;



uint8_t *USBDescriptor = NULL;
uint8_t *USBConfigDescriptor = NULL;
uint8_t *USBInterfaceDescriptor = NULL;
uint8_t *USBStringDescriptors = NULL;
uint16_t descriptorSize = 0; //hold the size of the descriptor pointer
uint16_t configSize = 0;	//hold the size of the config pointer
uint16_t interfaceSize = 0;	//hold the size of the interface pointer
uint16_t stringSize = 0;	//hold the size of the string pointer
uint8_t configNum = 1;
uint8_t interfaceNum = 0;


/**
	Initalizes device descriptor using provided config data
	
	@param [in]	desc	Structure containing config information to be added
 */
void setDeviceDescriptor(TUSBDeviceDescriptor desc)
{
	USBDescriptor = (uint8_t*)malloc(0x12);
	memcpy(USBDescriptor, &desc, 0x12);
	USBDescriptor[0] = 0x12;
	USBDescriptor[1] = 0x01;
	descriptorSize = 0x12;
	configNum = 1;
}


/**
	resizes device descriptor 
 */
void expandDescriptor(uint8_t size)
{
	USBDescriptor = (uint8_t*)realloc(USBDescriptor,descriptorSize + size);
	descriptorSize += size;
}


/**
	resizes config descriptor 
 */
void expandConfigDescriptor(uint8_t size)
{
	USBConfigDescriptor = (uint8_t*)realloc(USBConfigDescriptor,configSize + size);
	configSize += size;
}

/**
	Initalizes config descriptor using provided config data
	
	@param [in]	desc	Structure containing config information to be added
 */

void initConfigDescriptor(TUSBConfiguration desc)
{
	USBConfigDescriptor = (uint8_t*)malloc(0x09);
	memcpy(USBConfigDescriptor, &desc, 0x09);
	configSize = 0x09;
	interfaceNum = 0;
	USBConfigDescriptor[0] = 0x09;
	USBConfigDescriptor[1] = 0x02;
	USBConfigDescriptor[4] = 0;
	USBConfigDescriptor[5] = configNum;
	configNum++;
}

/**
	Finalizes config descriptor and adds it to the device descriptor
 */

void finalizeConfigDescriptor()
{
	if (USBConfigDescriptor == NULL)
		return; //this should never be here. something is not setup, just return
	else
	{
		expandDescriptor(configSize);  //expand memory if new config added
	}
	memcpy(&USBConfigDescriptor[2], &configSize, 2);
	memcpy(USBDescriptor + descriptorSize - configSize, USBConfigDescriptor, configSize);
	free(USBConfigDescriptor);
	USBConfigDescriptor = NULL;
}



/**
	resizes interface descriptor 
 */
void expandInterfaceDescriptor(uint8_t size)
{
	USBInterfaceDescriptor = (uint8_t*)realloc(USBInterfaceDescriptor,interfaceSize + size);
	interfaceSize += size;
}


/**
	Initalizes interface using provided descriptor data
	
	@param [in]	desc	Structure containing interface information to be added
 */
void initInterfaceDescriptor(TUSBInterfaceDescriptor desc)
{
	USBInterfaceDescriptor = (uint8_t*)malloc(0x09);
	memcpy(USBInterfaceDescriptor, &desc, 0x09);
	USBInterfaceDescriptor[0] = 0x09;
	USBInterfaceDescriptor[1] = 0x04;
	USBInterfaceDescriptor[2] = USBConfigDescriptor[4];
	interfaceSize = 0x09;
	USBConfigDescriptor[4]++;
	USBInterfaceDescriptor[4] = 0;
}

/**
	Finalizes interface and adds it to current config descriptor
 */
 
void finalizeInterfaceDescriptor()
{
	if (USBInterfaceDescriptor == NULL)
		return; //this should never be here. something is not setup, just return
	else
	{
		expandConfigDescriptor(interfaceSize);    //expand memory if new config added
	}
	memcpy(USBConfigDescriptor + configSize - interfaceSize , USBInterfaceDescriptor, interfaceSize);
	free(USBInterfaceDescriptor);
	USBInterfaceDescriptor = NULL;
}
/**
	Registers a String Descriptor for the device and is optional

	@param [in]	desc	Structure containing endpoint information to be added
 */
void addEndpointDescriptor(TUSBEndpointDescriptor desc)
{
	if (USBInterfaceDescriptor == NULL )
		return; //this should never be here. something is not setup, just return
	else
	{
		expandInterfaceDescriptor(0x07);   //expand memory for new endpoint
	}
	memcpy(USBInterfaceDescriptor + interfaceSize - 0x07 , &desc, 0x07);
	USBInterfaceDescriptor[interfaceSize - 0x07] = 0x07;
	USBInterfaceDescriptor[interfaceSize - 0x07 + 1] = 0x05;
	USBInterfaceDescriptor[4]++;
}

void addFunctionalDescriptor(TUSBFunctionalDescriptor desc)
{
	if (USBInterfaceDescriptor == NULL)
		return; //this should never be here. something is not setup, just return
	else
	{
		expandInterfaceDescriptor(desc.bLength);    //expand memory for new endpoint
	}
	memcpy(USBInterfaceDescriptor + interfaceSize - desc.bLength + 2, &desc.data, desc.bLength-2);
	USBInterfaceDescriptor[interfaceSize - desc.bLength] = desc.bLength;
	USBInterfaceDescriptor[interfaceSize - desc.bLength + 1] = desc.bDescriptorType;
}

/**
	Registers a String Descriptor for the device and is optional

	@param [in]	string	pointer to string
	@param [in]	len	length of string
 */
void addStringDescriptor(uint16_t *string,uint8_t len)
{
	USBStringDescriptors = realloc(USBStringDescriptors, stringSize +len + 2);
	USBStringDescriptors[stringSize] = len;
	USBStringDescriptors[stringSize+1]	= 0x03;
	memcpy(&USBStringDescriptors[stringSize + 2], string, len);
	stringSize += len + 2;
}

void addStringDescriptorChar(char *string, uint8_t len)
{
	USBStringDescriptors = realloc(USBStringDescriptors, stringSize + (len*2) + 2);
	USBStringDescriptors[stringSize] = len*2;
	USBStringDescriptors[stringSize + 1]	= 0x03;
	for (int i = 0; i < len*2; i+=2)
	{
		USBStringDescriptors[stringSize + 1 + i] = string[i];
		USBStringDescriptors[stringSize + 1 + i +1] = 0;
	}
	stringSize += (len*2) + 2;
}


/**
	Registers a pointer to a descriptor block containing all descriptors
	for the device. This is built from other descriptor blocks and then freed
 */
void setUSBDescriptor()
{
	if (USBDescriptor == NULL)
		return; //this should never be here. something is not setup, just return
	if(!(USBConfigDescriptor == NULL || (USBConfigDescriptor[2] + USBConfigDescriptor[3] >> 8) < 25 || USBConfigDescriptor[4] < 1))
	{
		expandDescriptor(configSize);
		finalizeConfigDescriptor();
	}
	if (USBStringDescriptors != NULL)
	{
		USBDescriptor = realloc(USBDescriptor, descriptorSize + stringSize);
		memcpy(USBDescriptor, USBStringDescriptors, stringSize);
		free(USBStringDescriptors);
		USBStringDescriptors = NULL;
	}
	USBRegisterDescriptors(USBDescriptor);
}



/**
	Registers a pointer to a descriptor block containing all descriptors
	for the device.

	@param [in]	pabDescriptors	The descriptor byte array
 */
void USBRegisterDescriptors(const uint8_t *pabDescriptors)
{
	pabDescrip = pabDescriptors;
}


/**
	Parses the list of installed USB descriptors and attempts to find
	the specified USB descriptor.
		
	@param [in]		wTypeIndex	Type and index of the descriptor
	@param [in]		wLangID		Language ID of the descriptor (currently unused)
	@param [out]	*piLen		Descriptor length
	@param [out]	*ppbData	Descriptor data
	
	@return true if the descriptor was found, false otherwise
 */
bool USBGetDescriptor(uint16_t wTypeIndex, uint16_t wLangID, int *piLen, uint8_t **ppbData)
{
	uint8_t	bType, bIndex;
	uint8_t	*pab;
	int iCurIndex;
	
	ASSERT(pabDescrip != NULL);

	bType = GET_DESC_TYPE(wTypeIndex);
	bIndex = GET_DESC_INDEX(wTypeIndex);
	
	pab = (uint8_t *)pabDescrip;
	iCurIndex = 0;
	
	while (pab[DESC_bLength] != 0) {
		if (pab[DESC_bDescriptorType] == bType) {
			if (iCurIndex == bIndex) {
				// set data pointer
				*ppbData = pab;
				// get length from structure
				if (bType == DESC_CONFIGURATION) {
					// configuration descriptor is an exception, length is at offset 2 and 3
					*piLen =	(pab[CONF_DESC_wTotalLength]) |
								(pab[CONF_DESC_wTotalLength + 1] << 8);
				}
				else {
					// normally length is at offset 0
					*piLen = pab[DESC_bLength];
				}
				return true;
			}
			iCurIndex++;
		}
		// skip to next descriptor
		pab += pab[DESC_bLength];
	}
	// nothing found
	DBG("Desc %x not found!\n", wTypeIndex);
	return false;
}


/**
	Configures the device according to the specified configuration index and
	alternate setting by parsing the installed USB descriptor list.
	A configuration index of 0 unconfigures the device.
		
	@param [in]		bConfigIndex	Configuration index
	@param [in]		bAltSetting		Alternate setting number
	
	@todo function always returns true, add stricter checking?
	
	@return true if successfully configured, false otherwise
 */
static bool USBSetConfiguration(uint8_t bConfigIndex, uint8_t bAltSetting)
{
	uint8_t	*pab;
	uint8_t	bCurConfig, bCurAltSetting;
	uint8_t	bEP;
	uint16_t	wMaxPktSize;
	
	ASSERT(pabDescrip != NULL);

	if (bConfigIndex == 0) {
		// unconfigure device
		USBHwConfigDevice(false);
	}
	else {
		// configure endpoints for this configuration/altsetting
		pab = (uint8_t *)pabDescrip;
		bCurConfig = 0xFF;
		bCurAltSetting = 0xFF;

		while (pab[DESC_bLength] != 0) {

			switch (pab[DESC_bDescriptorType]) {

			case DESC_CONFIGURATION:
				// remember current configuration index
				bCurConfig = pab[CONF_DESC_bConfigurationValue];
				break;

			case DESC_INTERFACE:
				// remember current alternate setting
				bCurAltSetting = pab[INTF_DESC_bAlternateSetting];
				break;

			case DESC_ENDPOINT:
				if ((bCurConfig == bConfigIndex) &&
					(bCurAltSetting == bAltSetting)) {
					// endpoint found for desired config and alternate setting
					bEP = pab[ENDP_DESC_bEndpointAddress];
					wMaxPktSize = 	(pab[ENDP_DESC_wMaxPacketSize]) |
									(pab[ENDP_DESC_wMaxPacketSize + 1] << 8);
					// configure endpoint
					USBHwEPConfig(bEP, wMaxPktSize);
				}
				break;

			default:
				break;
			}
			// skip to next descriptor
			pab += pab[DESC_bLength];
		}
		
		// configure device
		USBHwConfigDevice(true);
	}

	return true;
}


/**
	Local function to handle a standard device request
		
	@param [in]		pSetup		The setup packet
	@param [in,out]	*piLen		Pointer to data length
	@param [in,out]	ppbData		Data buffer.

	@return true if the request was handled successfully
 */
static bool HandleStdDeviceReq(TSetupPacket *pSetup, int *piLen, uint8_t **ppbData)
{
	uint8_t	*pbData = *ppbData;

	switch (pSetup->bRequest) {
	
	case REQ_GET_STATUS:
		// bit 0: self-powered
		// bit 1: remote wakeup = not supported
		pbData[0] = 0;
		pbData[1] = 0;
		*piLen = 2;
		break;
		
	case REQ_SET_ADDRESS:
		USBHwSetAddress(pSetup->wValue);
		break;

	case REQ_GET_DESCRIPTOR:
		DBG("D%x", pSetup->wValue);
		return USBGetDescriptor(pSetup->wValue, pSetup->wIndex, piLen, ppbData);

	case REQ_GET_CONFIGURATION:
		// indicate if we are configured
		pbData[0] = bConfiguration;
		*piLen = 1;
		break;

	case REQ_SET_CONFIGURATION:
		if (!USBSetConfiguration(pSetup->wValue & 0xFF, 0)) {
			DBG("USBSetConfiguration failed!\n");
			return false;
		}
		// configuration successful, update current configuration
		bConfiguration = pSetup->wValue & 0xFF;	
		break;

	case REQ_CLEAR_FEATURE:
	case REQ_SET_FEATURE:
		if (pSetup->wValue == FEA_REMOTE_WAKEUP) {
			// put DEVICE_REMOTE_WAKEUP code here
		}
		if (pSetup->wValue == FEA_TEST_MODE) {
			// put TEST_MODE code here
		}
		return false;

	case REQ_SET_DESCRIPTOR:
		DBG("Device req %d not implemented\n", pSetup->bRequest);
		return false;

	default:
		DBG("Illegal device req %d\n", pSetup->bRequest);
		return false;
	}
	
	return true;
}


/**
	Local function to handle a standard interface request
		
	@param [in]		pSetup		The setup packet
	@param [in,out]	*piLen		Pointer to data length
	@param [in]		ppbData		Data buffer.

	@return true if the request was handled successfully
 */
static bool HandleStdInterfaceReq(TSetupPacket	*pSetup, int *piLen, uint8_t **ppbData)
{
	uint8_t	*pbData = *ppbData;

	switch (pSetup->bRequest) {

	case REQ_GET_STATUS:
		// no bits specified
		pbData[0] = 0;
		pbData[1] = 0;
		*piLen = 2;
		break;

	case REQ_CLEAR_FEATURE:
	case REQ_SET_FEATURE:
		// not defined for interface
		return false;
	
	case REQ_GET_INTERFACE:	// TODO use bNumInterfaces
        // there is only one interface, return n-1 (= 0)
		pbData[0] = 0;
		*piLen = 1;
		break;
	
	case REQ_SET_INTERFACE:	// TODO use bNumInterfaces
		// there is only one interface (= 0)
		if (pSetup->wValue != 0) {
			return false;
		}
		*piLen = 0;
		break;

	default:
		DBG("Illegal interface req %d\n", pSetup->bRequest);
		return false;
	}

	return true;
}


/**
	Local function to handle a standard endpoint request
		
	@param [in]		pSetup		The setup packet
	@param [in,out]	*piLen		Pointer to data length
	@param [in]		ppbData		Data buffer.

	@return true if the request was handled successfully
 */
static bool HandleStdEndPointReq(TSetupPacket	*pSetup, int *piLen, uint8_t **ppbData)
{
	uint8_t	*pbData = *ppbData;

	switch (pSetup->bRequest) {
	case REQ_GET_STATUS:
		// bit 0 = endpointed halted or not
		pbData[0] = (USBHwEPGetStatus(pSetup->wIndex) & EP_STATUS_STALLED) ? 1 : 0;
		pbData[1] = 0;
		*piLen = 2;
		break;
		
	case REQ_CLEAR_FEATURE:
		if (pSetup->wValue == FEA_ENDPOINT_HALT) {
			// clear HALT by unstalling
			USBHwEPStall(pSetup->wIndex, false);
			break;
		}
		// only ENDPOINT_HALT defined for endpoints
		return false;
	
	case REQ_SET_FEATURE:
		if (pSetup->wValue == FEA_ENDPOINT_HALT) {
			// set HALT by stalling
			USBHwEPStall(pSetup->wIndex, true);
			break;
		}
		// only ENDPOINT_HALT defined for endpoints
		return false;

	case REQ_SYNCH_FRAME:
		DBG("EP req %d not implemented\n", pSetup->bRequest);
		return false;

	default:
		DBG("Illegal EP req %d\n", pSetup->bRequest);
		return false;
	}
	
	return true;
}


/**
	Default handler for standard ('chapter 9') requests
	
	If a custom request handler was installed, this handler is called first.
		
	@param [in]		pSetup		The setup packet
	@param [in,out]	*piLen		Pointer to data length
	@param [in]		ppbData		Data buffer.

	@return true if the request was handled successfully
 */
bool USBHandleStandardRequest(TSetupPacket	*pSetup, int *piLen, uint8_t **ppbData)
{
	// try the custom request handler first
	if ((pfnHandleCustomReq != NULL) && pfnHandleCustomReq(pSetup, piLen, ppbData)) {
		return true;
	}
	
	switch (REQTYPE_GET_RECIP(pSetup->bmRequestType)) {
	case REQTYPE_RECIP_DEVICE:		return HandleStdDeviceReq(pSetup, piLen, ppbData);
	case REQTYPE_RECIP_INTERFACE:	return HandleStdInterfaceReq(pSetup, piLen, ppbData);
	case REQTYPE_RECIP_ENDPOINT: 	return HandleStdEndPointReq(pSetup, piLen, ppbData);
	default: 						return false;
	}
}


/**
	Registers a callback for custom device requests
	
	In USBHandleStandardRequest, the custom request handler gets a first
	chance at handling the request before it is handed over to the 'chapter 9'
	request handler.
	
	This can be used for example in HID devices, where a REQ_GET_DESCRIPTOR
	request is sent to an interface, which is not covered by the 'chapter 9'
	specification.
		
	@param [in]	pfnHandler	Callback function pointer
 */
void USBRegisterCustomReqHandler(TFnHandleRequest *pfnHandler)
{
	pfnHandleCustomReq = pfnHandler;
}

