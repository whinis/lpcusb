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


/**
	Definitions of structures of standard USB packets
*/

#ifndef _USBSTRUCT_H_
#define _USBSTRUCT_H_

#include <stdint.h>

/** setup packet definitions */
typedef struct {
	uint8_t	bmRequestType;			/**< characteristics of the specific request */
	uint8_t	bRequest;				/**< specific request */
	uint16_t	wValue;					/**< request specific parameter */
	uint16_t	wIndex;					/**< request specific parameter */
	uint16_t	wLength;				/**< length of data transfered in data phase */
} TSetupPacket;


#define REQTYPE_GET_DIR(x)		(((x)>>7)&0x01)
#define REQTYPE_GET_TYPE(x)		(((x)>>5)&0x03)
#define REQTYPE_GET_RECIP(x)	((x)&0x1F)

#define REQTYPE_DIR_TO_DEVICE	0
#define REQTYPE_DIR_TO_HOST		1

#define REQTYPE_TYPE_STANDARD	0
#define REQTYPE_TYPE_CLASS		1
#define REQTYPE_TYPE_VENDOR		2
#define REQTYPE_TYPE_RESERVED	3

#define REQTYPE_RECIP_DEVICE	0
#define REQTYPE_RECIP_INTERFACE	1
#define REQTYPE_RECIP_ENDPOINT	2
#define REQTYPE_RECIP_OTHER		3

/* standard requests */
#define	REQ_GET_STATUS			0x00
#define REQ_CLEAR_FEATURE		0x01
#define REQ_SET_FEATURE			0x03
#define REQ_SET_ADDRESS			0x05
#define REQ_GET_DESCRIPTOR		0x06
#define REQ_SET_DESCRIPTOR		0x07
#define REQ_GET_CONFIGURATION	0x08
#define REQ_SET_CONFIGURATION	0x09
#define REQ_GET_INTERFACE		0x0A
#define REQ_SET_INTERFACE		0x0B
#define REQ_SYNCH_FRAME			0x0C

/* class requests HID */
#define HID_GET_REPORT			0x01
#define HID_GET_IDLE			0x02
#define HID_GET_PROTOCOL	 	0x03
#define HID_SET_REPORT			0x09
#define HID_SET_IDLE			0x0A
#define HID_SET_PROTOCOL		0x0B

/* feature selectors */
#define FEA_ENDPOINT_HALT		0x00
#define FEA_REMOTE_WAKEUP		0x01
#define FEA_TEST_MODE			0x02

/*
	USB descriptors
*/

/** USB descriptor header */
typedef struct {
	uint8_t	bLength;			/**< descriptor length */
	uint8_t	bDescriptorType;	/**< descriptor type */
} TUSBDescHeader;


typedef struct
{
	uint8_t	bLength; /**< descriptor length */
	uint8_t	bDescriptorType; /**< descriptor type always 0x05 */
	uint8_t data[10];
	
}TUSBFunctionalDescriptor;

typedef struct
{
	uint8_t	bLength; /**< descriptor length */
	uint8_t	bDescriptorType; /**< descriptor type always 0x05 */
	uint8_t bEndpointAddress; /**
	**	Bits 0..3b Endpoint Number.
		Bits 4..6b Reserved. Set to Zero
		Bits 7 Direction 0 = Out, 1 = In (Ignored for Control Endpoints)*/

	uint8_t  bmAttributes; /** 	
	**its 0..1 Transfer Type

    00 = Control
    01 = Isochronous
    10 = Bulk
    11 = Interrupt

Bits 2..7 are reserved. If Isochronous endpoint,
Bits 3..2 = Synchronisation Type (Iso Mode)

    00 = No Synchonisation
    01 = Asynchronous
    10 = Adaptive
    11 = Synchronous

Bits 5..4 = Usage Type (Iso Mode)

    00 = Data Endpoint
    01 = Feedback Endpoint
    10 = Explicit Feedback Data Endpoint
    11 = Reserved*/
	
	uint8_t wMaxPacketSize[2]; /**  Maximum Packet Size this endpoint is capable of sending or receiving*/
	uint8_t bInterval; /**  	
	**Interval for polling endpoint data transfers. Value in frame counts.
	**Ignored for Bulk & Control Endpoints. Isochronous must equal 1 and 
	**field may range from 1 to 255 for interrupt endpoints.*/
}TUSBEndpointDescriptor;

typedef struct
{
	uint8_t	bLength; /**< descriptor length */
	uint8_t	bDescriptorType; /**< descriptor type always 0x04 */
	uint8_t bInterfaceNumber; /**Number of Interface*/

	uint8_t bAlternateSetting; /** 	Value used to select alternative setting*/
	
	uint8_t bNumEndpoints; /**  Number of Endpoints used for this interface */
	uint8_t bInterfaceClass; /**  	Class Code (Assigned by USB Org)*/
	uint8_t bmAbInterfaceSubClassttributes /** Subclass Code (Assigned by USB Org)*/; 
	
	uint8_t bInterfaceProtocol ; /**  Protocol Code (Assigned by USB Org) */
	uint8_t iInterface; /**  	Index of String Descriptor Describing this interface*/
}TUSBInterfaceDescriptor;

typedef struct
{
	uint8_t	bLength; /**< descriptor length */
	uint8_t	bDescriptorType; /**< descriptor type always 0x02 */
	uint8_t wTotalLength[2]; /**Total length in bytes of data returned*/

	uint8_t bNumInterfaces;/** 	Number of Interfaces*/
	
	uint8_t bConfigurationValue; /**  Value to use as an argument to select this configuration */
	uint8_t iConfiguration; /**  	Index of String Descriptor describing this configuration*/
	uint8_t bmAttributes; 
	/**  	D7 Reserved, set to 1. (USB 1.0 Bus Powered)

D6 Self Powered

D5 Remote Wakeup

D4..0 Reserved, set to 0.*/
	uint8_t bMaxPower; /**  	Maximum Power Consumption in 2mA units */
}TUSBConfiguration;

//Check https://beyondlogic.org/usbnutshell/usb5.shtml for more information
typedef struct {
	uint8_t	bLength; /**< descriptor length */
	uint8_t	bDescriptorType; /**< descriptor type, always 0x01 */
	uint8_t bcdUSB[2]; /**USB Specification Number which device complies too.*/

	uint8_t bDeviceClass;
	/**Class Code (Assigned by USB Org)

	If equal to Zero, each interface specifies it’s own class code

	If equal to 0xFF, the class code is vendor specified.

	Otherwise field is valid Class Code.*/
	
	uint8_t bDeviceSubClass; /**  Subclass Code (Assigned by USB Org) */
	uint8_t bDeviceProtocol; /** Protocol Code (Assigned by USB Org)*/
	uint8_t bMaxPacketSize; /**  	Maximum Packet Size for Zero Endpoint. Valid Sizes are 8, 16, 32, 64*/
	uint8_t idVendor[2]; /** Vendor ID (Assigned by USB Org)*/
	uint8_t idProduct[2]; /** Product ID (Assigned by Manufacturer)*/
	uint8_t bcdDevice[2]; /** Device Release Number*/
	uint8_t	iManufacturer; /**  Index of Manufacturer String Descriptor, 0 for no string*/
	uint8_t iProduct; /** Index of Product String Descriptor, 0 for no string*/
	uint8_t iSerialNumber; /**  Index of Serial Number String Descriptor, 0 for no string*/
	uint8_t bNumConfigurations /** Number of Possible Configurations*/;
	
} TUSBDeviceDescriptor;

typedef struct
{
	uint8_t	bLength; /**< descriptor length */
	uint8_t	bDescriptorType; /**< descriptor type always 0x03*/
	char *unicodeString; /**unicode encoded string*/
}TUSBStringDescriptor;

#define DESC_DEVICE				1
#define DESC_CONFIGURATION		2
#define DESC_STRING				3
#define DESC_INTERFACE			4
#define DESC_ENDPOINT			5
#define DESC_DEVICE_QUALIFIER	6
#define DESC_OTHER_SPEED		7
#define DESC_INTERFACE_POWER	8


#define DESC_HID_HID			0x21
#define DESC_HID_REPORT			0x22
#define DESC_HID_PHYSICAL		0x23

#define GET_DESC_TYPE(x)		(((x)>>8)&0xFF)
#define GET_DESC_INDEX(x)		((x)&0xFF)

#endif /* _USBSTRUCT_H_ */

