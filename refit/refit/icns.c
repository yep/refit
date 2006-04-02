/*
 * refit/icns.c
 * Loader for .icns icon files
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

#include "lib.h"

#ifndef TEXTONLY

//
// well-known icons
//

typedef struct {
    REFIT_IMAGE *Image;
    CHAR16 *Path;
    UINTN PixelSize;
} BUILTIN_ICON;

BUILTIN_ICON BuiltinIconTable[] = {
    { NULL, L"\\icons\\os_mac.icns", 128 },
    { NULL, L"\\icons\\os_linux.icns", 128 },
    { NULL, L"\\icons\\os_win.icns", 128 },
    { NULL, L"\\icons\\os_unknown.icns", 128 },
    { NULL, L"\\icons\\func_about.icns", 48 },
    { NULL, L"\\icons\\func_exit.icns", 48 },
    { NULL, L"\\icons\\func_reset.icns", 48 },
    { NULL, L"\\icons\\tool_shell.icns", 48 },
    { NULL, L"\\icons\\vol_internal.icns", 32 },
    { NULL, L"\\icons\\vol_external.icns", 32 },
    { NULL, L"\\icons\\vol_optical.icns", 32 },
    { NULL, L"\\icons\\os_hwtest.icns", 128 },
};
#define BUILTIN_ICON_COUNT (12)

REFIT_IMAGE * BuiltinIcon(IN UINTN Id)
{
    CHAR16 *FullPath;
    
    if (Id >= BUILTIN_ICON_COUNT)
        return NULL;
    
    if (BuiltinIconTable[Id].Image == NULL) {
        FullPath = PoolPrint(L"%s%s", SelfDirPath, BuiltinIconTable[Id].Path);
        BuiltinIconTable[Id].Image = LoadIcnsFallback(SelfDir, FullPath, BuiltinIconTable[Id].PixelSize);
        FreePool(FullPath);
    }
    
    return BuiltinIconTable[Id].Image;
}

//
// Decompress pixel data
//

static CHAR8 * DecompressPixelData(IN CHAR8 *CompDataPtr, IN UINTN CompDataLen, IN UINTN RawDataLen)
{
    CHAR8 *RawDataPtr;
    CHAR8 *p1, *p2;
    CHAR8 *EndPtr;
    CHAR8 value;
    UINTN code, len, i;
    
    RawDataPtr = AllocatePool(RawDataLen);
    EndPtr = CompDataPtr + CompDataLen;
    
    p1 = CompDataPtr;
    p2 = RawDataPtr;
    while (p1 < EndPtr) {
        code = *p1++;
        if (code & 0x80) {
            len = code - 125;
            value = *p1++;
            for (i = 0; i < len; i++)
                *p2++ = value;
        } else {
            len = code + 1;
            for (i = 0; i < len; i++)
                *p2++ = *p1++;
        }
        // TODO: lots of range checks
    }
    
    return RawDataPtr;
}

//
// Load an image from a .icns file
//

REFIT_IMAGE * LoadIcns(IN EFI_FILE_HANDLE BaseDir, IN CHAR16 *FileName, IN UINTN PixelSize)
{
    EFI_STATUS      Status;
    EFI_FILE_HANDLE IconFile;
    EFI_FILE_INFO   *IconFileInfo;
    UINT64          ReadSize;
    UINTN           BufferSize;
    CHAR8           *Buffer, *BufferEnd;
    CHAR8           *Ptr, *DataPtr, *MaskPtr, *RawDataPtr;
    CHAR8           *ImageData, *PtrI, *PtrR, *PtrG, *PtrB, *PtrA, *DestPtr;
    UINTN           FetchPixelSize, PixelCount, i;
    UINT32          BlockLen, DataLen, MaskLen;
    REFIT_IMAGE     *Image;
    
    Status = BaseDir->Open(BaseDir, &IconFile, FileName, EFI_FILE_MODE_READ, 0);
    if (CheckError(Status, L"while loading an icon")) {
        Print(L" Path: '%s'  Size: %d\n", FileName, PixelSize);
        return NULL;
    }
    
    IconFileInfo = LibFileInfo(IconFile);
    if (IconFileInfo == NULL) {
        // TODO: print and register the error
        return NULL;
    }
    ReadSize = IconFileInfo->FileSize;
    if (ReadSize > 1024*1024)
        ReadSize = 1024*1024;
    FreePool(IconFileInfo);
    
    BufferSize = (UINTN)ReadSize;   // was limited to 1 MB before, so this is safe
    Buffer = AllocatePool(BufferSize);
    Status = IconFile->Read(IconFile, &BufferSize, Buffer);
    if (CheckError(Status, L"while loading an icon")) {
        FreePool(Buffer);
        return NULL;
    }
    Status = IconFile->Close(IconFile);
    
    if (Buffer[0] != 'i' || Buffer[1] != 'c' || Buffer[2] != 'n' || Buffer[3] != 's') {
        // not an icns file...
        // TODO: print and register the error
        FreePool(Buffer);
        return NULL;
    }
    
    FetchPixelSize = PixelSize;
    for (;;) {
        DataPtr = NULL;
        MaskPtr = NULL;
        
        Ptr = Buffer + 8;
        BufferEnd = Buffer + ReadSize;
        while (Ptr + 8 <= BufferEnd) {
            BlockLen = ((UINT32)Ptr[4] << 24) + ((UINT32)Ptr[5] << 16) + ((UINT32)Ptr[6] << 8) + (UINT32)Ptr[7];
            if (Ptr + BlockLen > BufferEnd)   // block continues beyond end of file
                break;
            
            if (FetchPixelSize == 128) {
                if (Ptr[0] == 'i' && Ptr[1] == 't' && Ptr[2] == '3' && Ptr[3] == '2') {
                    if (Ptr[8] == 0 && Ptr[9] == 0 && Ptr[10] == 0 && Ptr[11] == 0) {
                        DataPtr = Ptr + 12;
                        DataLen = BlockLen - 12;
                    }
                } else if (Ptr[0] == 't' && Ptr[1] == '8' && Ptr[2] == 'm' && Ptr[3] == 'k') {
                    MaskPtr = Ptr + 8;
                    MaskLen = BlockLen - 8;
                }
                
            } else if (FetchPixelSize == 48) {
                if (Ptr[0] == 'i' && Ptr[1] == 'h' && Ptr[2] == '3' && Ptr[3] == '2') {
                    DataPtr = Ptr + 8;
                    DataLen = BlockLen - 8;
                } else if (Ptr[0] == 'h' && Ptr[1] == '8' && Ptr[2] == 'm' && Ptr[3] == 'k') {
                    MaskPtr = Ptr + 8;
                    MaskLen = BlockLen - 8;
                }
                
            } else if (FetchPixelSize == 32) {
                if (Ptr[0] == 'i' && Ptr[1] == 'l' && Ptr[2] == '3' && Ptr[3] == '2') {
                    DataPtr = Ptr + 8;
                    DataLen = BlockLen - 8;
                } else if (Ptr[0] == 'l' && Ptr[1] == '8' && Ptr[2] == 'm' && Ptr[3] == 'k') {
                    MaskPtr = Ptr + 8;
                    MaskLen = BlockLen - 8;
                }
                
            } else if (FetchPixelSize == 16) {
                if (Ptr[0] == 'i' && Ptr[1] == 's' && Ptr[2] == '3' && Ptr[3] == '2') {
                    DataPtr = Ptr + 8;
                    DataLen = BlockLen - 8;
                } else if (Ptr[0] == 's' && Ptr[1] == '8' && Ptr[2] == 'm' && Ptr[3] == 'k') {
                    MaskPtr = Ptr + 8;
                    MaskLen = BlockLen - 8;
                }
                
            }
            
            Ptr += BlockLen;
        }
        
        /* for the future: try to load a different size and scale it
        if (DataPtr == NULL && FetchPixelSize == 32) {
            FetchPixelSize = 128;
            continue;
        }
         */
        break;
    }
    
    if (DataPtr != NULL) {
        // we found an image
        
        PixelCount = FetchPixelSize * FetchPixelSize;
        ImageData = AllocatePool(PixelCount * 4);
        
        if (DataLen < PixelCount * 3) {
            // uncompress pixel data
            RawDataPtr = DecompressPixelData(DataPtr, DataLen, PixelCount * 3);
            // copy data, planar
            PtrR = RawDataPtr;
            PtrG = RawDataPtr + PixelCount;
            PtrB = RawDataPtr + PixelCount * 2;
            DestPtr = ImageData;
            if (MaskPtr) {
                PtrA = MaskPtr;
                for (i = 0; i < PixelCount; i++) {
                    *DestPtr++ = *PtrB++;
                    *DestPtr++ = *PtrG++;
                    *DestPtr++ = *PtrR++;
                    *DestPtr++ = *PtrA++;
                }
            } else {
                for (i = 0; i < PixelCount; i++) {
                    *DestPtr++ = *PtrB++;
                    *DestPtr++ = *PtrG++;
                    *DestPtr++ = *PtrR++;
                    *DestPtr++ = 255;
                }
            }
            FreePool(RawDataPtr);
            
        } else {
            // copy data, interleaved
            PtrI = DataPtr;
            DestPtr = ImageData;
            if (MaskPtr) {
                PtrA = MaskPtr;
                for (i = 0; i < PixelCount; i++, PtrI += 3) {
                    *DestPtr++ = PtrI[2];
                    *DestPtr++ = PtrI[1];
                    *DestPtr++ = PtrI[0];
                    *DestPtr++ = *PtrA++;
                }
            } else {
                for (i = 0; i < PixelCount; i++, PtrI += 3) {
                    *DestPtr++ = PtrI[2];
                    *DestPtr++ = PtrI[1];
                    *DestPtr++ = PtrI[0];
                    *DestPtr++ = 255;
                }
            }
        }
        
        // TODO: scale down when we were looking for 32, but only got 128
        
        Image = AllocatePool(sizeof(REFIT_IMAGE));
        Image->PixelData = ImageData;
        Image->Width = FetchPixelSize;
        Image->Height = FetchPixelSize;
        
    } else
        Image = NULL;
    
    FreePool(Buffer);
    
    return Image;
}

REFIT_IMAGE * DummyImage(IN UINTN PixelSize)
{
    UINTN           x, y, LineOffset;
    CHAR8           *ImageData, *Ptr, *YPtr;
    REFIT_IMAGE     *Image;
    
    ImageData = AllocateZeroPool(PixelSize * PixelSize * 4);
    LineOffset = PixelSize * 4;
    
    YPtr = ImageData + ((PixelSize - 32) >> 1) * (LineOffset + 4);
    for (y = 0; y < 32; y++) {
        Ptr = YPtr;
        for (x = 0; x < 32; x++) {
            if (((x + y) % 12) < 6) {
                *Ptr++ = 0;
                *Ptr++ = 0;
                *Ptr++ = 0;
            } else {
                *Ptr++ = 0;
                *Ptr++ = 255;
                *Ptr++ = 255;
            }
            *Ptr++ = 144;
        }
        YPtr += LineOffset;
    }
    
    Image = AllocatePool(sizeof(REFIT_IMAGE));
    Image->PixelData = ImageData;
    Image->Width = PixelSize;
    Image->Height = PixelSize;
    
    return Image;
}

#else   /* !TEXTONLY */

REFIT_IMAGE * LoadIcns(IN EFI_FILE_HANDLE BaseDir, IN CHAR16 *FileName, IN UINTN PixelSize)
{
    return NULL;
}

REFIT_IMAGE * DummyImage(IN UINTN PixelSize)
{
    return NULL;
}

REFIT_IMAGE * BuiltinIcon(IN UINTN Id)
{
    return NULL;
}

#endif  /* !TEXTONLY */

REFIT_IMAGE * LoadIcnsFallback(IN EFI_FILE_HANDLE BaseDir, IN CHAR16 *FileName, IN UINTN PixelSize)
{
    REFIT_IMAGE *Image;
    
    Image = LoadIcns(BaseDir, FileName, PixelSize);
    if (Image == NULL)
        Image = DummyImage(PixelSize);
    return Image;
}
