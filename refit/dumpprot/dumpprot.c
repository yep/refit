/*
 * dumpprot.c
 * dump all protocols
 *
 * Switches to text mode, then launches e.efi or elilo.efi from the same
 * directory.
 *
 * Copyright (c) 2006 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "efi.h"
#include "efilib.h"
#include "efiUgaDraw.h"
#include "efiUgaIo.h"
#include "ConsoleControl.h"

//
// helper functions
//

static VOID MyGuidToString(OUT CHAR16 *Buffer, IN EFI_GUID *Guid)
{
    SPrint (Buffer, 0, L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            Guid->Data1,                    
            Guid->Data2,
            Guid->Data3,
            Guid->Data4[0],
            Guid->Data4[1],
            Guid->Data4[2],
            Guid->Data4[3],
            Guid->Data4[4],
            Guid->Data4[5],
            Guid->Data4[6],
            Guid->Data4[7]
            );
}

static CHAR16 * MyDevicePathToStr(IN EFI_DEVICE_PATH *DevicePath)
{
    CHAR16 GuidBuffer[64];
    if (DevicePathType(DevicePath) == MEDIA_DEVICE_PATH && DevicePathSubType(DevicePath) == 6) {
        MyGuidToString(GuidBuffer, (EFI_GUID *)( (CHAR8 *)DevicePath + 4 ));
        return PoolPrint(L"FVFile(%s)", GuidBuffer);
    } else {
        return DevicePathToStr(DevicePath);
    }
}

//
// protocol investigators
//

// TODO: device path, uga, component name, loaded image

VOID InvLoadedImage(IN EFI_HANDLE Handle, IN VOID *ProtocolInterface)
{
    EFI_LOADED_IMAGE *LoadedImage = (EFI_LOADED_IMAGE *)ProtocolInterface;
    EFI_STATUS      Status;
    EFI_DEVICE_PATH *DevicePath;
    
    Status = BS->HandleProtocol(LoadedImage->DeviceHandle, &DevicePathProtocol, (VOID **) &DevicePath);
    if (!EFI_ERROR(Status))
        Print(L" Device %s", MyDevicePathToStr(DevicePath));
    
    Print(L" File %s", MyDevicePathToStr(LoadedImage->FilePath));
}

VOID InvDevPath(IN EFI_HANDLE Handle, IN VOID *ProtocolInterface)
{
    EFI_DEVICE_PATH *DevicePath = (EFI_DEVICE_PATH *)ProtocolInterface;
    
    Print(L" %s", MyDevicePathToStr(DevicePath));
}

VOID InvComponentName(IN EFI_HANDLE Handle, IN VOID *ProtocolInterface)
{
    EFI_COMPONENT_NAME_PROTOCOL *CompName = (EFI_COMPONENT_NAME_PROTOCOL *)ProtocolInterface;
    EFI_STATUS      Status;
    CHAR16          *DriverName;
    
    Status = CompName->GetDriverName(CompName, "eng", &DriverName);
    if (!EFI_ERROR(Status))
        Print(L" %s", DriverName);
    else
        Print(L" (%R)", Status);
}

//
// protocol information
//

typedef VOID (*INVESTIGATE)(IN EFI_HANDLE Handle, IN VOID *ProtocolInterface);

typedef struct {
    EFI_GUID    ID;
    CHAR16      *Name;
    INVESTIGATE InvestigateFunc;
} PROTOCOL_INFO;

PROTOCOL_INFO KnownProtocols[] = {
    { LOADED_IMAGE_PROTOCOL,            L"LoadedImage", InvLoadedImage },
    { DEVICE_PATH_PROTOCOL,             L"DevPath", InvDevPath },
    { EFI_DRIVER_BINDING_PROTOCOL_GUID, L"DriverBinding", NULL },
    { EFI_DRIVER_CONFIGURATION_PROTOCOL_GUID, L"DriverConfiguration", NULL },
    { EFI_DRIVER_DIAGNOSTICS_PROTOCOL_GUID, L"DriverDiagnostics", NULL },
    { EFI_COMPONENT_NAME_PROTOCOL_GUID, L"ComponentName", InvComponentName },

    { SIMPLE_TEXT_INPUT_PROTOCOL,       L"SimpleInput", NULL },
    { EFI_SIMPLE_POINTER_PROTOCOL_GUID, L"SimplePointer", NULL },
    { SIMPLE_TEXT_OUTPUT_PROTOCOL,      L"SimpleTextOut", NULL },
    { EFI_UGA_DRAW_PROTOCOL_GUID,       L"UGADraw", NULL },
    { EFI_UGA_IO_PROTOCOL_GUID,         L"UGAIO", NULL },
    { { 0xa45b3a0d, 0x2e55, 0x4c03, 0xad, 0x9c, 0x27, 0xd4,
        0x82, 0xb, 0x50, 0x7e },        L"UGASplash", NULL },
    { EFI_CONSOLE_CONTROL_PROTOCOL_GUID, L"ConsoleControl", NULL },

    { DISK_IO_PROTOCOL,                 L"DiskIO", NULL },
    { BLOCK_IO_PROTOCOL,                L"BlockIO", NULL },
    { SIMPLE_FILE_SYSTEM_PROTOCOL,      L"FileSystem", NULL },
    { LOAD_FILE_PROTOCOL,               L"LoadFile", NULL },

    { EFI_PCI_IO_PROTOCOL_GUID,         L"PCIIO", NULL },
    { EFI_USB_HC_PROTOCOL_GUID,         L"USBHost", NULL },
    { EFI_USB_IO_PROTOCOL_GUID,         L"USBIO", NULL },

    { { 0x03c4e624, 0xac28, 0x11d3, 0x9a, 0x2d, 0x00, 0x90,
        0x29, 0x3f, 0xc1, 0x4d },       L"PxeDhcp4", NULL },
    { { 0x02b3d5f2, 0xac28, 0x11d3, 0x9a, 0x2d, 0x00, 0x90,
        0x27, 0x3f, 0xc1, 0x4d },       L"Tcp", NULL },

    { { 0x389F751F, 0x1838, 0x4388, 0x83, 0x90, 0xCD, 0x81,
        0x54, 0xBD, 0x27, 0xF8 },       L"FirmwareVolume", NULL },
    { { 0x7aa35a69, 0x506c, 0x444f, 0xa7, 0xaf, 0x69, 0x4b,
        0xf5, 0x6f, 0x71, 0xc8 },       L"FirmwareVolumeDispatch", NULL },
    { { 0x448F5DA4, 0x6DD7, 0x4FE1, 0x93, 0x07, 0x69, 0x22,
        0x41, 0x92, 0x21, 0x5D },       L"SectionExtraction", NULL },
    { { 0xFC1BCDB0, 0x7D31, 0x49aa, 0x93, 0x6A, 0xA4, 0x60,
        0x0D, 0x9D, 0xD0, 0x83 },       L"CRC32GuidedSectionExtraction", NULL },
    { { 0xDE28BC59, 0x6228, 0x41BD, 0xBD, 0xF6, 0xA3, 0xB9,
        0xAD, 0xB5, 0x8D, 0xA1 },       L"FVBlock", NULL },
    { { 0x53a4c71b, 0xb581, 0x4170, 0x91, 0xb3, 0x8d, 0xb8,
        0x7a, 0x4b, 0x5c, 0x46 },       L"FVBlockExtension", NULL },
    { { 0x5cb5c776,0x60d5,0x45ee,0x88,0x3c,0x45,0x27,
        0x8,0xcd,0x74,0x3f },           L"LoadPE32Image", NULL },
    { { 0xe84cf29c, 0x191f, 0x4eae, 0x96, 0xe1, 0xf4, 0x6a,
        0xec, 0xea, 0xea, 0x0b },       L"TianoDecompress", NULL },

    { { 0xdb9a1e3d, 0x45cb, 0x4abb, 0x85, 0x3b, 0xe5, 0x38,
        0x7f, 0xdb, 0x2e, 0x2d },       L"LegacyBIOS", NULL },
    { { 0x783658a3, 0x4172, 0x4421, 0xa2, 0x99, 0xe0, 0x09,
        0x07, 0x9c, 0x0c, 0xb4 },       L"LegacyBIOSPlatform", NULL },

    { {0,0,0,0,0,0,0,0,0,0,0},          NULL, NULL },
};

//
// guid printer
//


//
// main function
//

EFI_STATUS
EFIAPI
efi_main    (IN EFI_HANDLE           ImageHandle,
             IN EFI_SYSTEM_TABLE     *SystemTable)
{
    EFI_STATUS          Status;
    UINTN               Index, PIndex, IIndex;
    UINTN               HandleCount;
    EFI_HANDLE          *HandleBuffer;
    EFI_HANDLE          Handle;
    UINTN               ProtocolCount;
    EFI_GUID            **ProtocolBuffer;
    EFI_GUID            *ProtocolID;
    VOID                *ProtocolInterface;
    CHAR16              GUIDBuf[64];
    
    InitializeLib(ImageHandle, SystemTable);
    
    Status = LibLocateHandle (AllHandles, NULL, NULL,
                              &HandleCount, &HandleBuffer);
    if (EFI_ERROR (Status)) {
        Status = EFI_NOT_FOUND;
        return Status;
    }
    
    for (Index = 0; Index < HandleCount; Index++) {
        
        Handle = HandleBuffer[Index];
        Print(L"Handle %02x @ %08x\n", Index, (UINT32)Handle);
        
        Status = BS->ProtocolsPerHandle(Handle, &ProtocolBuffer, &ProtocolCount);
        if (EFI_ERROR(Status)) {
            Print(L"  Error: %R\n", Status);
            continue;
        }
        
        for (PIndex = 0; PIndex < ProtocolCount; PIndex++) {
            ProtocolID = ProtocolBuffer[PIndex];
            
            MyGuidToString(GUIDBuf, ProtocolID);
            Print(L"  %s", GUIDBuf);
            
            for (IIndex = 0; KnownProtocols[IIndex].Name; IIndex++) {
                if (CompareMem(ProtocolID, &(KnownProtocols[IIndex].ID), sizeof(EFI_GUID)) == 0) {
                    
                    Print(L" %s", KnownProtocols[IIndex].Name);
                    if (KnownProtocols[IIndex].InvestigateFunc) {
                        Status = BS->HandleProtocol(Handle, ProtocolID, &ProtocolInterface);
                        if (EFI_ERROR(Status)) {
                            Print(L" (%R)", Status);
                        } else {
                            (KnownProtocols[IIndex].InvestigateFunc)(Handle, ProtocolInterface);
                        }
                    }
                    
                }
            }
            
            Print(L"\n");
        }
        
        FreePool(ProtocolBuffer);
        
    }
    
    FreePool (HandleBuffer);
    
    return EFI_SUCCESS;
}
