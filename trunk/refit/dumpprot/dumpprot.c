/*
 * dumpprot.c
 * Dump all handles and protocols
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
#include "efiConsoleControl.h"

//
// helper functions
//

static VOID MyGuidToString(OUT CHAR16 *Buffer, IN EFI_GUID *Guid)
{
    SPrint (Buffer, 0, L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            (UINTN)Guid->Data1,                    
            (UINTN)Guid->Data2,
            (UINTN)Guid->Data3,
            (UINTN)Guid->Data4[0],
            (UINTN)Guid->Data4[1],
            (UINTN)Guid->Data4[2],
            (UINTN)Guid->Data4[3],
            (UINTN)Guid->Data4[4],
            (UINTN)Guid->Data4[5],
            (UINTN)Guid->Data4[6],
            (UINTN)Guid->Data4[7]
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

// TODO: UGADraw, TextOut

VOID InvLoadedImage(IN EFI_HANDLE Handle, IN VOID *ProtocolInterface)
{
    EFI_LOADED_IMAGE *LoadedImage = (EFI_LOADED_IMAGE *)ProtocolInterface;
    EFI_STATUS      Status;
    EFI_DEVICE_PATH *DevicePath;
    
    Status = BS->HandleProtocol(LoadedImage->DeviceHandle, &DevicePathProtocol, (VOID **) &DevicePath);
    if (!EFI_ERROR(Status))
        Print(L" Device \"%s\"", MyDevicePathToStr(DevicePath));
    
    Print(L" File \"%s\"", MyDevicePathToStr(LoadedImage->FilePath));
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
        Print(L" \"%s\"", DriverName);
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
    { EFI_COMPONENT_NAME_PROTOCOL_GUID, L"ComponentName", InvComponentName },
    { {0,0,0,0,0,0,0,0,0,0,0},          NULL, NULL },
};

#include "protocols.h"

//
// Extract firmware device path(s) from handle database
//

static VOID ExtractFirmwareDevicePaths(EFI_DEVICE_PATH **PathList, UINTN MaxPaths)
{
    EFI_STATUS          Status;
    UINTN               HandleCount = 0;
    UINTN               HandleIndex;
    EFI_HANDLE          *Handles;
    EFI_HANDLE          Handle;
    UINTN               PathCount = 0;
    UINTN               PathIndex;
    EFI_LOADED_IMAGE    *LoadedImage;
    EFI_DEVICE_PATH     *DevicePath;
    BOOLEAN             Seen;

    MaxPaths--;  // leave space for the terminating NULL pointer
    PathList[PathCount] = NULL;  // makes it okay to return early without results

    // get all LoadedImage handles
    Status = LibLocateHandle(ByProtocol, &LoadedImageProtocol, NULL,
                             &HandleCount, &Handles);
    if (EFI_ERROR(Status)) {
        Print(L"Error: Can't get list of handles with LoadedImage protocol.\n");
        return;
    }
    for (HandleIndex = 0; HandleIndex < HandleCount && PathCount < MaxPaths; HandleIndex++) {
        Handle = Handles[HandleIndex];
        
        Status = BS->HandleProtocol(Handle, &LoadedImageProtocol, (VOID **) &LoadedImage);
        if (EFI_ERROR(Status)) {
            Print(L"Warning: Can't get LoadedImage protocol.\n");
            continue;
        }
        
        Status = BS->HandleProtocol(LoadedImage->DeviceHandle, &DevicePathProtocol, (VOID **) &DevicePath);
        if (EFI_ERROR(Status)) {
            //Print(L"Warning: Can't get device path for image.\n");
            continue;
        }
        
        // Check if we have this device path in the list already
        Seen = FALSE;
        for (PathIndex = 0; PathIndex < PathCount; PathIndex++) {
            if (DevicePathSize(DevicePath) != DevicePathSize(PathList[PathIndex]))
                continue;
            if (CompareMem(DevicePath, PathList[PathIndex], DevicePathSize(DevicePath)) == 0) {
                Seen = TRUE;
                break;
            }
        }
        if (Seen)
            continue;
        
        // Found a new device path!
        Print(L"Device \"%s\"\n", DevicePathToStr(DevicePath));
        PathList[PathCount++] = DuplicateDevicePath(DevicePath);
    }
    FreePool(Handles);

    PathList[PathCount] = NULL;
}

static UINT8 LegacyLoaderMediaPathData[] = {
    0x04, 0x06, 0x14, 0x00, 0xEB, 0x85, 0x05, 0x2B,
    0xB8, 0xD8, 0xA9, 0x49, 0x8B, 0x8C, 0xE2, 0x1B,
    0x01, 0xAE, 0xF2, 0xB7, 0x7F, 0xFF, 0x04, 0x00,
};
EFI_DEVICE_PATH *LegacyLoaderMediaPath = (EFI_DEVICE_PATH *)LegacyLoaderMediaPathData;

static VOID ExtractLegacyLoaderPaths(EFI_DEVICE_PATH **PathList, UINTN MaxPaths)
{
    EFI_STATUS          Status;
    UINTN               HandleCount = 0;
    UINTN               HandleIndex;
    EFI_HANDLE          *Handles;
    EFI_HANDLE          Handle;
    UINTN               PathCount = 0;
    UINTN               PathIndex;
    EFI_LOADED_IMAGE    *LoadedImage;
    EFI_DEVICE_PATH     *DevicePath;
    BOOLEAN             Seen;
    
    MaxPaths--;  // leave space for the terminating NULL pointer
    PathList[PathCount] = NULL;  // makes it okay to return early without results
    
    // get all LoadedImage handles
    Status = LibLocateHandle(ByProtocol, &LoadedImageProtocol, NULL,
                             &HandleCount, &Handles);
    if (EFI_ERROR(Status)) {
        Print(L"Error: Can't get list of handles with LoadedImage protocol.\n");
        return;
    }
    for (HandleIndex = 0; HandleIndex < HandleCount && PathCount < MaxPaths; HandleIndex++) {
        Handle = Handles[HandleIndex];
        
        Status = BS->HandleProtocol(Handle, &LoadedImageProtocol, (VOID **) &LoadedImage);
        if (EFI_ERROR(Status)) {
            Print(L"Warning: Can't get LoadedImage protocol.\n");
            continue;
        }
        
        Status = BS->HandleProtocol(LoadedImage->DeviceHandle, &DevicePathProtocol, (VOID **) &DevicePath);
        if (EFI_ERROR(Status)) {
            //Print(L"Warning: Can't get device path for image.\n");
            continue;
        }
        
        // Only grab memory range nodes
        if (DevicePathType(DevicePath) != HARDWARE_DEVICE_PATH || DevicePathSubType(DevicePath) != HW_MEMMAP_DP)
            continue;
        
        // Check if we have this device path in the list already
        // WARNING: This assumes the first node in the device path is unique!
        Seen = FALSE;
        for (PathIndex = 0; PathIndex < PathCount; PathIndex++) {
            if (DevicePathNodeLength(DevicePath) != DevicePathNodeLength(PathList[PathIndex]))
                continue;
            if (CompareMem(DevicePath, PathList[PathIndex], DevicePathNodeLength(DevicePath)) == 0) {
                Seen = TRUE;
                break;
            }
        }
        if (Seen)
            continue;
        
        // Found a new device path!
        Print(L"Device \"%s\"\n", DevicePathToStr(DevicePath));
        PathList[PathCount++] = AppendDevicePath(DevicePath, LegacyLoaderMediaPath);
    }
    FreePool(Handles);
    
    PathList[PathCount] = NULL;
}

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
    BOOLEAN             Found;
    EFI_DEVICE_PATH     *FirmwareDevicePaths[16];
    
    InitializeLib(ImageHandle, SystemTable);
    
    Status = LibLocateHandle (AllHandles, NULL, NULL,
                              &HandleCount, &HandleBuffer);
    if (EFI_ERROR (Status)) {
        Status = EFI_NOT_FOUND;
        return Status;
    }
    
    for (Index = 0; Index < HandleCount; Index++) {
        
        Handle = HandleBuffer[Index];
#if defined(EFI64) || defined(EFIX64)
        Print(L"Handle %02x @ %016x\n", Index, (UINT64)Handle);
#else
        Print(L"Handle %02x @ %08x\n", Index, (UINT32)Handle);
#endif
        
        Status = BS->ProtocolsPerHandle(Handle, &ProtocolBuffer, &ProtocolCount);
        if (EFI_ERROR(Status)) {
            Print(L"  Error: %R\n", Status);
            continue;
        }
        
        for (PIndex = 0; PIndex < ProtocolCount; PIndex++) {
            ProtocolID = ProtocolBuffer[PIndex];
            
            MyGuidToString(GUIDBuf, ProtocolID);
            Print(L"  %s", GUIDBuf);
            
            Found = FALSE;
            
            for (IIndex = 0; KnownProtocols[IIndex].Name && !Found; IIndex++) {
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
                    Found = TRUE;
                    
                }
            }
            
            for (IIndex = 0; MoreKnownProtocols[IIndex].Name && !Found; IIndex++) {
                if (CompareMem(ProtocolID, &(MoreKnownProtocols[IIndex].ID), sizeof(EFI_GUID)) == 0) {
                    
                    Print(L" %s", MoreKnownProtocols[IIndex].Name);
                    Found = TRUE;
                    
                }
            }
            
            Print(L"\n");
        }
        
        FreePool(ProtocolBuffer);
        
    }
    
    FreePool (HandleBuffer);
    
    
    Print(L"\nDiscovered firmware image device paths:\n");
    ExtractFirmwareDevicePaths(FirmwareDevicePaths, 16);
    for (Index = 0; FirmwareDevicePaths[Index]; Index++) {
        Print(L"Device path #%d:\n", Index+1);
        Print(L"%s\n", MyDevicePathToStr(FirmwareDevicePaths[Index]));
        DumpHex(2, 0, DevicePathSize(FirmwareDevicePaths[Index]), FirmwareDevicePaths[Index]);
    }
    
    
    Print(L"\nDiscovered legacy loader device paths:\n");
    ExtractLegacyLoaderPaths(FirmwareDevicePaths, 16);
    for (Index = 0; FirmwareDevicePaths[Index]; Index++) {
        Print(L"Device path #%d:\n", Index+1);
        Print(L"%s\n", MyDevicePathToStr(FirmwareDevicePaths[Index]));
        DumpHex(2, 0, DevicePathSize(FirmwareDevicePaths[Index]), FirmwareDevicePaths[Index]);
    }
    
    
    return EFI_SUCCESS;
}
