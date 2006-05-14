/*
 * libeg/image.c
 * Image handling functions
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

#include "libegint.h"

#define MAX_FILE_SIZE (1024*1024*1024)

//
// Basic image handling
//

EG_IMAGE * egCreateImage(IN UINTN Width, IN UINTN Height, IN BOOLEAN HasAlpha)
{
    EG_IMAGE        *NewImage;
    
    NewImage = (EG_IMAGE *) AllocatePool(sizeof(EG_IMAGE));
    if (NewImage == NULL)
        return NULL;
    NewImage->PixelData = (EG_PIXEL *) AllocatePool(Width * Height * sizeof(EG_PIXEL));
    if (NewImage->PixelData == NULL) {
        FreePool(NewImage);
        return NULL;
    }
    
    NewImage->Width = Width;
    NewImage->Height = Height;
    NewImage->HasAlpha = HasAlpha;
    return NewImage;
}

EG_IMAGE * egCreateFilledImage(IN UINTN Width, IN UINTN Height, IN BOOLEAN HasAlpha, IN EG_PIXEL *Color)
{
    EG_IMAGE        *NewImage;
    
    NewImage = egCreateImage(Width, Height, HasAlpha);
    if (NewImage == NULL)
        return NULL;
    
    egFillImage(NewImage, Color);
    return NewImage;
}

EG_IMAGE * egCopyImage(IN EG_IMAGE *Image)
{
    EG_IMAGE        *NewImage;
    
    NewImage = egCreateImage(Image->Width, Image->Height, Image->HasAlpha);
    if (NewImage == NULL)
        return NULL;
    
    CopyMem(NewImage->PixelData, Image->PixelData, Image->Width * Image->Height * sizeof(EG_PIXEL));
    return NewImage;
}

VOID egFreeImage(IN EG_IMAGE *Image)
{
    if (Image != NULL) {
        if (Image->PixelData != NULL)
            FreePool(Image->PixelData);
        FreePool(Image);
    }
}

//
// Basic file operations
//

EFI_STATUS egLoadFile(IN EFI_FILE_HANDLE BaseDir, IN CHAR16 *FileName,
                      OUT UINT8 **FileData, OUT UINTN *FileDataLength)
{
    EFI_STATUS          Status;
    EFI_FILE_HANDLE     FileHandle;
    EFI_FILE_INFO       *FileInfo;
    UINT64              ReadSize;
    UINTN               BufferSize;
    UINT8               *Buffer;
    
    Status = BaseDir->Open(BaseDir, &FileHandle, FileName, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status))
        return Status;
    
    FileInfo = LibFileInfo(FileHandle);
    if (FileInfo == NULL) {
        FileHandle->Close(FileHandle);
        return EFI_NOT_FOUND;
    }
    ReadSize = FileInfo->FileSize;
    if (ReadSize > MAX_FILE_SIZE)
        ReadSize = MAX_FILE_SIZE;
    FreePool(FileInfo);
    
    BufferSize = (UINTN)ReadSize;   // was limited to 1 GB above, so this is safe
    Buffer = (UINT8 *) AllocatePool(BufferSize);
    if (Buffer == NULL) {
        FileHandle->Close(FileHandle);
        return EFI_OUT_OF_RESOURCES;
    }
    
    Status = FileHandle->Read(FileHandle, &BufferSize, Buffer);
    FileHandle->Close(FileHandle);
    if (EFI_ERROR(Status)) {
        FreePool(Buffer);
        return Status;
    }
    
    *FileData = Buffer;
    *FileDataLength = BufferSize;
    return EFI_SUCCESS;
}

static EFI_GUID ESPGuid = { 0xc12a7328, 0xf81f, 0x11d2, { 0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b } };

static EFI_STATUS egFindESP(OUT EFI_FILE_HANDLE *RootDir)
{
    EFI_STATUS          Status;
    UINTN               HandleCount = 0;
    EFI_HANDLE          *Handles;
    
    Status = LibLocateHandle(ByProtocol, &ESPGuid, NULL, &HandleCount, &Handles);
    if (!EFI_ERROR(Status) && HandleCount > 0) {
        *RootDir = LibOpenRoot(Handles[0]);
        if (*RootDir == NULL)
            Status = EFI_NOT_FOUND;
        FreePool(Handles);
    }
    return Status;
}

EFI_STATUS egSaveFile(IN EFI_FILE_HANDLE BaseDir OPTIONAL, IN CHAR16 *FileName,
                      IN UINT8 *FileData, IN UINTN FileDataLength)
{
    EFI_STATUS          Status;
    EFI_FILE_HANDLE     FileHandle;
    UINTN               BufferSize;
    
    if (BaseDir == NULL) {
        Status = egFindESP(&BaseDir);
        if (EFI_ERROR(Status))
            return Status;
    }
    
    Status = BaseDir->Open(BaseDir, &FileHandle, FileName,
                           EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
    if (EFI_ERROR(Status))
        return Status;
    
    BufferSize = FileDataLength;
    Status = FileHandle->Write(FileHandle, &BufferSize, FileData);
    FileHandle->Close(FileHandle);
    
    return Status;
}

//
// Loading images from files and embedded data
//

static CHAR16 * egFindExtension(IN CHAR16 *FileName)
{
    UINTN i;
    
    for (i = StrLen(FileName); i >= 0; i--) {
        if (FileName[i] == '.')
            return FileName + i + 1;
        if (FileName[i] == '/' || FileName[i] == '\\')
            break;
    }
    return FileName + StrLen(FileName);
}

static EG_IMAGE * egDecodeAny(IN UINT8 *FileData, IN UINTN FileDataLength,
                              IN CHAR16 *Format, IN UINTN IconSize, IN BOOLEAN WantAlpha)
{
    EG_DECODE_FUNC  DecodeFunc;
    EG_IMAGE        *NewImage;
    
    // dispatch by extension
    DecodeFunc = NULL;
    if (StriCmp(Format, L"BMP") == 0)
        DecodeFunc = egDecodeBMP;
    else if (StriCmp(Format, L"ICNS") == 0)
        DecodeFunc = egDecodeICNS;
    
    if (DecodeFunc == NULL)
        return NULL;
    
    // decode it
    NewImage = DecodeFunc(FileData, FileDataLength, IconSize, WantAlpha);
    
    return NewImage;
}

EG_IMAGE * egLoadImage(IN EFI_FILE_HANDLE BaseDir, IN CHAR16 *FileName, IN BOOLEAN WantAlpha)
{
    EFI_STATUS      Status;
    UINT8           *FileData;
    UINTN           FileDataLength;
    EG_IMAGE        *NewImage;
    
    if (BaseDir == NULL || FileName == NULL)
        return NULL;
    
    // load file
    Status = egLoadFile(BaseDir, FileName, &FileData, &FileDataLength);
    if (EFI_ERROR(Status))
        return NULL;
    
    // decode it
    NewImage = egDecodeAny(FileData, FileDataLength, egFindExtension(FileName), 128, WantAlpha);
    FreePool(FileData);
    
    return NewImage;
}

EG_IMAGE * egLoadIcon(IN EFI_FILE_HANDLE BaseDir, IN CHAR16 *FileName, IN UINTN IconSize)
{
    EFI_STATUS      Status;
    UINT8           *FileData;
    UINTN           FileDataLength;
    EG_IMAGE        *NewImage;
    
    if (BaseDir == NULL || FileName == NULL)
        return NULL;
    
    // load file
    Status = egLoadFile(BaseDir, FileName, &FileData, &FileDataLength);
    if (EFI_ERROR(Status))
        return NULL;
    
    // decode it
    NewImage = egDecodeAny(FileData, FileDataLength, egFindExtension(FileName), IconSize, TRUE);
    FreePool(FileData);
    
    return NewImage;
}

EG_IMAGE * egDecodeImage(IN UINT8 *FileData, IN UINTN FileDataLength, IN CHAR16 *Format, IN BOOLEAN WantAlpha)
{
    return egDecodeAny(FileData, FileDataLength, Format, 128, WantAlpha);
}

EG_IMAGE * egPrepareEmbeddedImage(IN EG_EMBEDDED_IMAGE *EmbeddedImage, IN BOOLEAN WantAlpha)
{
    EG_IMAGE            *NewImage;
    UINT8               *CompData;
    UINTN               CompLen;
    UINTN               PixelCount;
    
    // sanity check
    if (EmbeddedImage->PixelMode > EG_MAX_EIPIXELMODE ||
        (EmbeddedImage->CompressMode != EG_EICOMPMODE_NONE && EmbeddedImage->CompressMode != EG_EICOMPMODE_RLE))
        return NULL;
    
    // allocate image structure and pixel buffer
    NewImage = egCreateImage(EmbeddedImage->Width, EmbeddedImage->Height, WantAlpha);
    if (NewImage == NULL)
        return NULL;
    
    CompData = (UINT8 *)EmbeddedImage->Data;   // drop const
    CompLen  = EmbeddedImage->DataLength;
    PixelCount = EmbeddedImage->Width * EmbeddedImage->Height;
    
    // FUTURE: for EG_EICOMPMODE_EFICOMPRESS, decompress whole data block here
    
    if (EmbeddedImage->PixelMode == EG_EIPIXELMODE_GRAY ||
        EmbeddedImage->PixelMode == EG_EIPIXELMODE_GRAY_ALPHA) {
        
        // copy grayscale plane and expand
        if (EmbeddedImage->CompressMode == EG_EICOMPMODE_RLE) {
            egDecompressIcnsRLE(&CompData, &CompLen, PLPTR(NewImage, r), PixelCount);
        } else {
            egInsertPlane(CompData, PLPTR(NewImage, r), PixelCount);
            CompData += PixelCount;
        }
        egCopyPlane(PLPTR(NewImage, r), PLPTR(NewImage, g), PixelCount);
        egCopyPlane(PLPTR(NewImage, r), PLPTR(NewImage, b), PixelCount);
        
    } else if (EmbeddedImage->PixelMode == EG_EIPIXELMODE_COLOR ||
               EmbeddedImage->PixelMode == EG_EIPIXELMODE_COLOR_ALPHA) {
        
        // copy color planes
        if (EmbeddedImage->CompressMode == EG_EICOMPMODE_RLE) {
            egDecompressIcnsRLE(&CompData, &CompLen, PLPTR(NewImage, r), PixelCount);
            egDecompressIcnsRLE(&CompData, &CompLen, PLPTR(NewImage, g), PixelCount);
            egDecompressIcnsRLE(&CompData, &CompLen, PLPTR(NewImage, b), PixelCount);
        } else {
            egInsertPlane(CompData, PLPTR(NewImage, r), PixelCount);
            CompData += PixelCount;
            egInsertPlane(CompData, PLPTR(NewImage, g), PixelCount);
            CompData += PixelCount;
            egInsertPlane(CompData, PLPTR(NewImage, b), PixelCount);
            CompData += PixelCount;
        }
        
    } else {
        
        // set color planes to black
        egSetPlane(PLPTR(NewImage, r), 0, PixelCount);
        egSetPlane(PLPTR(NewImage, g), 0, PixelCount);
        egSetPlane(PLPTR(NewImage, b), 0, PixelCount);
        
    }
    
    if (WantAlpha && (EmbeddedImage->PixelMode == EG_EIPIXELMODE_GRAY_ALPHA ||
                      EmbeddedImage->PixelMode == EG_EIPIXELMODE_COLOR_ALPHA ||
                      EmbeddedImage->PixelMode == EG_EIPIXELMODE_ALPHA)) {
        
        // copy alpha plane
        if (EmbeddedImage->CompressMode == EG_EICOMPMODE_RLE) {
            egDecompressIcnsRLE(&CompData, &CompLen, PLPTR(NewImage, a), PixelCount);
        } else {
            egInsertPlane(CompData, PLPTR(NewImage, a), PixelCount);
            CompData += PixelCount;
        }
        
    } else {
        egSetPlane(PLPTR(NewImage, a), WantAlpha ? 255 : 0, PixelCount);
    }
    
    return NewImage;
}

//
// Compositing
//

static VOID egCalculateOverlap(IN UINTN SrcWidth, IN UINTN SrcHeight,
                               IN UINTN DestWidth, IN UINTN DestHeight,
                               IN UINTN PosXInDest, IN UINTN PosYInDest,
                               OUT UINTN *CompWidthOut, OUT UINTN *CompHeightOut)
{
    UINTN CompWidth, CompHeight;
    
    if (PosXInDest >= DestWidth || PosYInDest >= DestHeight) {
        // out of bounds, operation has no effect
        CompWidth = 0;
        CompHeight = 0;
    } else {
        // calculate affected area
        CompWidth  = DestWidth  - PosXInDest;
        CompHeight = DestHeight - PosYInDest;
        if (CompWidth  > SrcWidth)
            CompWidth  = SrcWidth;
        if (CompHeight > SrcHeight)
            CompHeight = SrcHeight;
    }
    *CompWidthOut  = CompWidth;
    *CompHeightOut = CompHeight;
    
    // FUTURE: use signed ints, do clipping in all directions
}

VOID egFillImage(IN OUT EG_IMAGE *CompImage, IN EG_PIXEL *Color)
{
    UINTN       i;
    EG_PIXEL    *PixelPtr;
    
    PixelPtr = CompImage->PixelData;
    for (i = 0; i < CompImage->Width * CompImage->Height; i++, PixelPtr++)
        *PixelPtr = *Color;
}

VOID egFillImageArea(IN OUT EG_IMAGE *CompImage,
                     IN UINTN PosX, IN UINTN PosY, IN UINTN Width, IN UINTN Height,
                     IN EG_PIXEL *Color)
{
    UINTN       x, y;
    UINTN       CompWidth, CompHeight;
    EG_PIXEL    *PixelPtr;
    EG_PIXEL    *PixelBasePtr;
    
    egCalculateOverlap(Width, Height,
                       CompImage->Width, CompImage->Height,
                       PosX, PosY,
                       &CompWidth, &CompHeight);
    
    if (CompWidth > 0) {
        PixelBasePtr = CompImage->PixelData + PosY * CompImage->Width + PosX;
        for (y = 0; y < CompHeight; y++) {
            PixelPtr = PixelBasePtr;
            for (x = 0; x < CompWidth; x++, PixelPtr++)
                *PixelPtr = *Color;
            PixelBasePtr += CompImage->Width;
        }
    }
}

VOID egRawCompose(IN OUT EG_PIXEL *CompBasePtr, IN EG_PIXEL *TopBasePtr,
                  IN UINTN Width, IN UINTN Height,
                  IN UINTN CompLineOffset, IN UINTN TopLineOffset)
{
    UINTN       x, y;
    EG_PIXEL    *TopPtr, *CompPtr;
    UINTN       Alpha;
    UINTN       RevAlpha;
    
    for (y = 0; y < Height; y++) {
        TopPtr = TopBasePtr;
        CompPtr = CompBasePtr;
        for (x = 0; x < Width; x++) {
            Alpha = TopPtr->a;
            RevAlpha = 255 - Alpha;
            CompPtr->b = ((UINTN)CompPtr->b * RevAlpha + (UINTN)TopPtr->b * Alpha) / 255;
            CompPtr->g = ((UINTN)CompPtr->g * RevAlpha + (UINTN)TopPtr->g * Alpha) / 255;
            CompPtr->r = ((UINTN)CompPtr->r * RevAlpha + (UINTN)TopPtr->r * Alpha) / 255;
            TopPtr++, CompPtr++;
        }
        TopBasePtr += TopLineOffset;
        CompBasePtr += CompLineOffset;
    }
}

VOID egComposeImage(IN OUT EG_IMAGE *CompImage, IN EG_IMAGE *TopImage, IN UINTN PosX, IN UINTN PosY)
{
    UINTN       CompWidth, CompHeight;
    
    egCalculateOverlap(TopImage->Width,  TopImage->Height,
                       CompImage->Width, CompImage->Height,
                       PosX, PosY,
                       &CompWidth, &CompHeight);
    
    // compose
    if (CompWidth > 0)
        egRawCompose(CompImage->PixelData + PosY * CompImage->Width + PosX, TopImage->PixelData,
                     CompWidth, CompHeight, CompImage->Width, TopImage->Width);
}

//
// misc internal functions
//

VOID egInsertPlane(IN UINT8 *SrcDataPtr, IN UINT8 *DestPlanePtr, IN UINTN PixelCount)
{
    UINTN i;
    
    for (i = 0; i < PixelCount; i++) {
        *DestPlanePtr = *SrcDataPtr++;
        DestPlanePtr += 4;
    }
}

VOID egSetPlane(IN UINT8 *DestPlanePtr, IN UINT8 Value, IN UINTN PixelCount)
{
    UINTN i;
    
    for (i = 0; i < PixelCount; i++) {
        *DestPlanePtr = Value;
        DestPlanePtr += 4;
    }
}

VOID egCopyPlane(IN UINT8 *SrcPlanePtr, IN UINT8 *DestPlanePtr, IN UINTN PixelCount)
{
    UINTN i;
    
    for (i = 0; i < PixelCount; i++) {
        *DestPlanePtr = *SrcPlanePtr;
        DestPlanePtr += 4, SrcPlanePtr += 4;
    }
}

/* EOF */
