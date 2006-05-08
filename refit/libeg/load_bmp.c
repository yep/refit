/*
 * libeg/load_bmp.c
 * Loading function for BMP images
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

// BMP structures

#pragma pack(1)

typedef struct {
    UINT8   Blue;
    UINT8   Green;
    UINT8   Red;
    UINT8   Reserved;
} BMP_COLOR_MAP;

typedef struct {
    CHAR8         CharB;
    CHAR8         CharM;
    UINT32        Size;
    UINT16        Reserved[2];
    UINT32        ImageOffset;
    UINT32        HeaderSize;
    UINT32        PixelWidth;
    UINT32        PixelHeight;
    UINT16        Planes;       // Must be 1
    UINT16        BitPerPixel;  // 1, 4, 8, or 24
    UINT32        CompressionType;
    UINT32        ImageSize;    // Compressed image size in bytes
    UINT32        XPixelsPerMeter;
    UINT32        YPixelsPerMeter;
    UINT32        NumberOfColors;
    UINT32        ImportantColors;
} BMP_IMAGE_HEADER;

#pragma pack()

//
// Load BMP image
//

EG_IMAGE * egLoadBMPImage(IN UINT8 *FileData, IN UINTN FileDataLength, IN UINTN IconSize, IN BOOLEAN WantAlpha)
{
    EG_IMAGE            *NewImage;
    BMP_IMAGE_HEADER    *BmpHeader;
    BMP_COLOR_MAP       *BmpColorMap;
    UINTN               x, y;
    UINT8               *ImagePtr;
    UINT8               *ImagePtrBase;
    UINTN               ImageLineOffset;
    UINT8               ImageValue;
    EG_PIXEL            *PixelPtr;
    UINTN               Index, BitIndex;
    
    // read and check header
    if (FileDataLength < sizeof(BMP_IMAGE_HEADER))
        return NULL;
    BmpHeader = (BMP_IMAGE_HEADER *) FileData;
    if (BmpHeader->CharB != 'B' || BmpHeader->CharM != 'M')
        return NULL;
    if (BmpHeader->CompressionType != 0)
        return NULL;
    if (BmpHeader->BitPerPixel != 1 && BmpHeader->BitPerPixel != 4 &&
        BmpHeader->BitPerPixel != 8 && BmpHeader->BitPerPixel != 24)
        return NULL;
    
    // calculate parameters
    ImageLineOffset = BmpHeader->PixelWidth;
    if (BmpHeader->BitPerPixel == 24)
        ImageLineOffset *= 3;
    else if (BmpHeader->BitPerPixel == 1)
        ImageLineOffset = (ImageLineOffset + 7) >> 3;
    else if (BmpHeader->BitPerPixel == 4)
        ImageLineOffset = (ImageLineOffset + 1) >> 1;
    if ((ImageLineOffset % 4) != 0)
        ImageLineOffset = ImageLineOffset + (4 - (ImageLineOffset % 4));
    // check bounds
    if (BmpHeader->ImageOffset + ImageLineOffset * BmpHeader->PixelHeight > FileDataLength)
        return NULL;
    
    // allocate image structure and buffer
    NewImage = egCreateImage(BmpHeader->PixelWidth, BmpHeader->PixelHeight, FALSE);
    if (NewImage == NULL)
        return NULL;
    
    // convert image
    BmpColorMap = (BMP_COLOR_MAP *)(FileData + sizeof(BMP_IMAGE_HEADER));
    ImagePtrBase = FileData + BmpHeader->ImageOffset;
    for (y = 0; y < BmpHeader->PixelHeight; y++) {
        ImagePtr = ImagePtrBase;
        ImagePtrBase += ImageLineOffset;
        PixelPtr = NewImage->PixelData + (BmpHeader->PixelHeight - 1 - y) * BmpHeader->PixelWidth;
        
        switch (BmpHeader->BitPerPixel) {
            
            case 1:
                for (x = 0; x < BmpHeader->PixelWidth; x++) {
                    BitIndex = x & 0x07;
                    if (BitIndex == 0)
                        ImageValue = *ImagePtr++;
                    
                    Index = (ImageValue >> (7 - BitIndex)) & 0x01;
                    PixelPtr->b = BmpColorMap[Index].Blue;
                    PixelPtr->g = BmpColorMap[Index].Green;
                    PixelPtr->r = BmpColorMap[Index].Red;
                    PixelPtr->a = 0;
                    PixelPtr++;
                }
                break;
            
            case 4:
                for (x = 0; x <= BmpHeader->PixelWidth - 2; x += 2) {
                    ImageValue = *ImagePtr++;
                    
                    Index = ImageValue >> 4;
                    PixelPtr->b = BmpColorMap[Index].Blue;
                    PixelPtr->g = BmpColorMap[Index].Green;
                    PixelPtr->r = BmpColorMap[Index].Red;
                    PixelPtr->a = 0;
                    PixelPtr++;
                    
                    Index = ImageValue & 0x0f;
                    PixelPtr->b = BmpColorMap[Index].Blue;
                    PixelPtr->g = BmpColorMap[Index].Green;
                    PixelPtr->r = BmpColorMap[Index].Red;
                    PixelPtr->a = 0;
                    PixelPtr++;
                }
                if (x < BmpHeader->PixelWidth) {
                    ImageValue = *ImagePtr++;
                    
                    Index = ImageValue >> 4;
                    PixelPtr->b = BmpColorMap[Index].Blue;
                    PixelPtr->g = BmpColorMap[Index].Green;
                    PixelPtr->r = BmpColorMap[Index].Red;
                    PixelPtr->a = 0;
                    PixelPtr++;
                }
                break;
            
            case 8:
                for (x = 0; x < BmpHeader->PixelWidth; x++) {
                    Index = *ImagePtr++;
                    PixelPtr->b = BmpColorMap[Index].Blue;
                    PixelPtr->g = BmpColorMap[Index].Green;
                    PixelPtr->r = BmpColorMap[Index].Red;
                    PixelPtr->a = 0;
                    PixelPtr++;
                }
                break;
            
            case 24:
                for (x = 0; x < BmpHeader->PixelWidth; x++) {
                    PixelPtr->b = *ImagePtr++;
                    PixelPtr->g = *ImagePtr++;
                    PixelPtr->r = *ImagePtr++;
                    PixelPtr->a = 0;
                    PixelPtr++;
                }
                break;
            
        }
    }
    
    return NewImage;
}

//
// Save BMP image
//

VOID egSaveBMPImage(IN EG_IMAGE *Image, IN EFI_FILE_HANDLE BaseDir, IN CHAR16 *FileName)
{
    BMP_IMAGE_HEADER    *BmpHeader;
    UINT8               *FileData;
    UINTN               FileDataLength;
    UINT8               *ImagePtr;
    EG_PIXEL            *PixelPtr;
    UINTN               x, y;
    EFI_STATUS          Status;
    EFI_FILE_HANDLE     ImageFile;
    
    if (BaseDir == NULL) {
        Print(L"Error BaseDir is NULL\n");
        return;
    }
    if ((Image->Width % 4) != 0) {
        Print(L"Error Width % 4\n");
        return;
    }
    
    FileDataLength = sizeof(BMP_IMAGE_HEADER) + Image->Height * Image->Width * 3;
    FileData = AllocateZeroPool(FileDataLength);
    if (FileData == NULL) {
        Print(L"Error allocate %d bytes\n", FileDataLength);
        return;
    }
    
    BmpHeader = (BMP_IMAGE_HEADER *)FileData;
    ImagePtr = FileData + sizeof(BMP_IMAGE_HEADER);
    
    // fill header
    BmpHeader->CharB = 'B';
    BmpHeader->CharM = 'M';
    BmpHeader->Size = FileDataLength;
    BmpHeader->ImageOffset = sizeof(BMP_IMAGE_HEADER);
    BmpHeader->HeaderSize = 0x28;
    BmpHeader->PixelWidth = Image->Width;
    BmpHeader->PixelHeight = Image->Height;
    BmpHeader->Planes = 1;
    BmpHeader->BitPerPixel = 24;
    BmpHeader->CompressionType = 1;
    BmpHeader->XPixelsPerMeter = 0xb13;
    BmpHeader->YPixelsPerMeter = 0xb13;
    
    // fill pixel buffer
    for (y = 0; y < Image->Height; y++) {
        PixelPtr = Image->PixelData + (Image->Height - 1 - y) * Image->Width;
        for (x = 0; x < Image->Width; x++) {
            *ImagePtr++ = PixelPtr->b;
            *ImagePtr++ = PixelPtr->g;
            *ImagePtr++ = PixelPtr->r;
            PixelPtr++;
        }
    }
    
    // write to file
    Status = BaseDir->Open(BaseDir, &ImageFile, FileName,
                           EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
    if (EFI_ERROR(Status)) {
        Print(L"Error File Open: %x\n", Status);
        FreePool(FileData);
        return;
    }
    Status = ImageFile->Write(ImageFile, &FileDataLength, FileData);
    if (EFI_ERROR(Status)) {
        Print(L"Error File Write: %x\n", Status);
    }
    FreePool(FileData);
    ImageFile->Close(ImageFile);
}

/* EOF */
