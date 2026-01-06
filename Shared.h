#ifndef _SHARED_H_
#define _SHARED_H_

typedef struct _KEY_EVENT_DATA {
    unsigned short MakeCode;
    unsigned short Flags; 
} KEY_EVENT_DATA, * PKEY_EVENT_DATA;

#define IOCTL_GET_KEY_EVENT  CTL_CODE(FILE_DEVICE_KEYBOARD, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#endif