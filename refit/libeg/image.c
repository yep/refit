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

static EFI_STATUS egLoadFile(IN EFI_FILE_HANDLE BaseDir, IN CHAR16 *FileName,
                             OUT UINT8 **FileData, OUT UINTN *FileDataLength)
{
    EFI_STATUS      Status;
    EFI_FILE_HANDLE IconFile;
    EFI_FILE_INFO   *IconFileInfo;
    UINT64          ReadSize;
    UINTN           BufferSize;
    UINT8           *Buffer;
    
    Status = BaseDir->Open(BaseDir, &IconFile, FileName, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status))
        return Status;
    
    IconFileInfo = LibFileInfo(IconFile);
    if (IconFileInfo == NULL) {
        IconFile->Close(IconFile);
        return EFI_NOT_FOUND;
    }
    ReadSize = IconFileInfo->FileSize;
    if (ReadSize > MAX_FILE_SIZE)
        ReadSize = MAX_FILE_SIZE;
    FreePool(IconFileInfo);
    
    BufferSize = (UINTN)ReadSize;   // was limited to 1 GB above, so this is safe
    Buffer = (UINT8 *) AllocatePool(BufferSize);
    if (Buffer == NULL) {
        IconFile->Close(IconFile);
        return EFI_OUT_OF_RESOURCES;
    }
    
    Status = IconFile->Read(IconFile, &BufferSize, Buffer);
    IconFile->Close(IconFile);
    if (EFI_ERROR(Status)) {
        FreePool(Buffer);
        return Status;
    }
    
    *FileData = Buffer;
    *FileDataLength = BufferSize;
    return EFI_SUCCESS;
}

EG_IMAGE * egLoadImage(IN EFI_FILE_HANDLE BaseDir, IN CHAR16 *FileName, IN BOOLEAN WantAlpha)
{
    EFI_STATUS      Status;
    CHAR16          *Extension;
    EG_LOADER_FUNC  Loader;
    UINT8           *FileData;
    UINTN           FileDataLength;
    EG_IMAGE        *NewImage;
    
    if (BaseDir == NULL || FileName == NULL)
        return NULL;
    
    // dispatch by extension
    Extension = egFindExtension(FileName);
    Loader = NULL;
    if (StriCmp(Extension, L"BMP") == 0)
        Loader = egLoadBMPImage;
    else if (StriCmp(Extension, L"ICNS") == 0)
        Loader = egLoadICNSIcon;
    
    if (Loader == NULL)
        return NULL;
    
    // load file
    Status = egLoadFile(BaseDir, FileName, &FileData, &FileDataLength);
    if (EFI_ERROR(Status))
        return NULL;
    
    // decode it
    NewImage = Loader(FileData, FileDataLength, 128, WantAlpha);
    
    FreePool(FileData);
    return NewImage;
}

EG_IMAGE * egLoadIcon(IN EFI_FILE_HANDLE BaseDir, IN CHAR16 *FileName, IN UINTN IconSize)
{
    EFI_STATUS  Status;
    CHAR16      *Extension;
    EG_LOADER_FUNC Loader;
    UINT8       *FileData;
    UINTN       FileDataLength;
    EG_IMAGE    *NewImage;
    
    if (BaseDir == NULL || FileName == NULL)
        return NULL;
    
    // dispatch by extension
    Extension = egFindExtension(FileName);
    Loader = NULL;
    if (StriCmp(Extension, L"ICNS") == 0)
        Loader = egLoadICNSIcon;
    
    if (Loader == NULL)
        return NULL;
    
    // load file
    Status = egLoadFile(BaseDir, FileName, &FileData, &FileDataLength);
    if (EFI_ERROR(Status))
        return NULL;
    
    // decode it
    NewImage = Loader(FileData, FileDataLength, IconSize, TRUE);
    
    FreePool(FileData);
    return NewImage;
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

VOID egFillImage(IN OUT EG_IMAGE *CompImage, IN EG_PIXEL *Color)
{
    EG_PIXEL    *PixelPtr;
    UINTN       i;
    
    PixelPtr = CompImage->PixelData;
    for (i = 0; i < CompImage->Width * CompImage->Height; i++, PixelPtr++)
        *PixelPtr = *Color;
}

static VOID egRawCompose(IN OUT EG_PIXEL *CompBasePtr, IN EG_PIXEL *TopBasePtr,
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
    UINTN CompWidth, CompHeight;
    
    if (PosX >= CompImage->Width || PosY >= CompImage->Height)
        return;   // operation has no effect
    
    // calculate affected area
    CompWidth  = CompImage->Width  - PosX;
    CompHeight = CompImage->Height - PosY;
    if (CompWidth  > TopImage->Width)
        CompWidth  = TopImage->Width;
    if (CompHeight > TopImage->Height)
        CompHeight = TopImage->Height;
    
    // compose
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
