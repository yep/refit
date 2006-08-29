/*
 * dumpfv/dumpfv.c
 * Dump the firmware image from ROM to disk.
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

#include <libeg.h>

//
// main function
//

EFI_STATUS
EFIAPI
efi_main    (IN EFI_HANDLE           ImageHandle,
             IN EFI_SYSTEM_TABLE     *SystemTable)
{
    EFI_STATUS              Status;
    VOID *                  fw_rom_ptr;
    VOID *                  fw_ptr;
    UINT32                  fw_len;
    
    InitializeLib(ImageHandle, SystemTable);
    
    // firmware location in ROM
    fw_rom_ptr = (void *)0xffe00000;
    fw_len = 0x200000;
    
    // make a copy in RAM
    Print(L"Copying Firmware to RAM...\n");
    fw_ptr = AllocatePool(fw_len);
    if (fw_ptr == NULL) {
        Print(L"Out of memory!\n");
        return EFI_OUT_OF_RESOURCES;
    }
    CopyMem(fw_ptr, fw_rom_ptr, fw_len);
    
    // save to file on the ESP
    Print(L"Saving Firmware to firmware.fd on ESP...\n");
    Status = egSaveFile(NULL, L"firmware.fd", fw_ptr, fw_len);
    if (EFI_ERROR(Status)) {
        Print(L"Error egSaveFile: %x\n", Status);
        return Status;
    }
    
    Print(L"Done.\n");
    return EFI_SUCCESS;
}
