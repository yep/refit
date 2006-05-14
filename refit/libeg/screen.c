/*
 * libeg/screen.c
 * Screen handling functions
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

#include <efiUgaDraw.h>
#include <efiConsoleControl.h>

// Console defines and variables

static EFI_GUID ConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;
static EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl = NULL;

static EFI_GUID UgaDrawProtocolGuid = EFI_UGA_DRAW_PROTOCOL_GUID;
static EFI_UGA_DRAW_PROTOCOL *UgaDraw = NULL;

static UINTN egScreenWidth  = 800;
static UINTN egScreenHeight = 600;

//
// Screen handling
//

VOID egInitScreen(VOID)
{
    EFI_STATUS Status;
    UINT32 UGAWidth, UGAHeight, UGADepth, UGARefreshRate;
    
    // get protocols
    Status = LibLocateProtocol(&ConsoleControlProtocolGuid, (VOID **) &ConsoleControl);
    if (EFI_ERROR(Status))
        ConsoleControl = NULL;
    
    Status = LibLocateProtocol(&UgaDrawProtocolGuid, (VOID **) &UgaDraw);
    if (EFI_ERROR(Status))
        UgaDraw = NULL;
    
    // get screen size
    if (UgaDraw != NULL) {
        Status = UgaDraw->GetMode(UgaDraw, &UGAWidth, &UGAHeight, &UGADepth, &UGARefreshRate);
        if (EFI_ERROR(Status)) {
            UgaDraw = NULL;   // graphics not available
        } else {
            egScreenWidth  = UGAWidth;
            egScreenHeight = UGAHeight;
        }
    }
}

VOID egGetScreenSize(OUT UINTN *ScreenWidth, OUT UINTN *ScreenHeight)
{
    if (ScreenWidth != NULL)
        *ScreenWidth = egScreenWidth;
    if (ScreenHeight != NULL)
        *ScreenHeight = egScreenHeight;
}

BOOLEAN egHasGraphicsMode(VOID)
{
    return (UgaDraw != NULL) ? TRUE : FALSE;
}

BOOLEAN egIsGraphicsModeEnabled(VOID)
{
    EFI_CONSOLE_CONTROL_SCREEN_MODE CurrentMode;
    
    if (ConsoleControl != NULL) {
        ConsoleControl->GetMode(ConsoleControl, &CurrentMode, NULL, NULL);
        return (CurrentMode == EfiConsoleControlScreenGraphics) ? TRUE : FALSE;
    }
    
    return FALSE;
}

VOID egSetGraphicsModeEnabled(IN BOOLEAN Enable)
{
    EFI_CONSOLE_CONTROL_SCREEN_MODE CurrentMode;
    EFI_CONSOLE_CONTROL_SCREEN_MODE NewMode;
    
    if (ConsoleControl != NULL) {
        ConsoleControl->GetMode(ConsoleControl, &CurrentMode, NULL, NULL);
        
        NewMode = Enable ? EfiConsoleControlScreenGraphics
                         : EfiConsoleControlScreenText;
        if (CurrentMode != NewMode)
            ConsoleControl->SetMode(ConsoleControl, NewMode);
    }
}

//
// Drawing to the screen
//

VOID egClearScreen(IN EG_PIXEL *Color)
{
    EFI_UGA_PIXEL FillColor;
    
    if (UgaDraw != NULL) {
        
        FillColor.Red   = Color->r;
        FillColor.Green = Color->g;
        FillColor.Blue  = Color->b;
        FillColor.Reserved = 0;
        
        UgaDraw->Blt(UgaDraw, &FillColor, EfiUgaVideoFill,
                     0, 0, 0, 0, egScreenWidth, egScreenHeight, 0);
        
    }
}

VOID egDrawImage(IN EG_IMAGE *Image, IN UINTN ScreenPosX, IN UINTN ScreenPosY)
{
    if (UgaDraw != NULL) {
        
        if (Image->HasAlpha) {
            Image->HasAlpha = FALSE;
            egSetPlane(PLPTR(Image, a), 0, Image->Width * Image->Height);
        }
        
        UgaDraw->Blt(UgaDraw, (EFI_UGA_PIXEL *)Image->PixelData, EfiUgaBltBufferToVideo,
                     0, 0, ScreenPosX, ScreenPosY, Image->Width, Image->Height, 0);
        
    }
}

VOID egDrawImageArea(IN EG_IMAGE *Image,
                     IN UINTN AreaPosX, IN UINTN AreaPosY,
                     IN UINTN AreaWidth, IN UINTN AreaHeight,
                     IN UINTN ScreenPosX, IN UINTN ScreenPosY)
{
    egRestrictImageArea(Image, AreaPosX, AreaPosY, &AreaWidth, &AreaHeight);
    if (AreaWidth == 0)
        return;
    
    if (UgaDraw != NULL) {
        
        if (Image->HasAlpha) {
            Image->HasAlpha = FALSE;
            egSetPlane(PLPTR(Image, a), 0, Image->Width * Image->Height);
        }
        
        UgaDraw->Blt(UgaDraw, (EFI_UGA_PIXEL *)Image->PixelData, EfiUgaBltBufferToVideo,
                     AreaPosX, AreaPosY, ScreenPosX, ScreenPosY, AreaWidth, AreaHeight, Image->Width * 4);
        
    }
}

//
// Make a screenshot
//

VOID egScreenShot(VOID)
{
    EFI_STATUS      Status;
    EG_IMAGE        *Image;
    UINT8           *FileData;
    UINTN           FileDataLength;
    UINTN           Index;
    
    if (UgaDraw == NULL)
        return;
    
    // allocate a buffer for the whole screen
    Image = egCreateImage(egScreenWidth, egScreenHeight, FALSE);
    if (Image == NULL) {
        Print(L"Error egCreateImage returned NULL\n");
        goto bailout_wait;
    }
    
    // get full screen image
    UgaDraw->Blt(UgaDraw, (EFI_UGA_PIXEL *)Image->PixelData, EfiUgaVideoToBltBuffer,
                 0, 0, 0, 0, Image->Width, Image->Height, 0);
    
    // encode as BMP
    egEncodeBMP(Image, &FileData, &FileDataLength);
    egFreeImage(Image);
    if (FileData == NULL) {
        Print(L"Error egEncodeBMP returned NULL\n");
        goto bailout_wait;
    }
    
    // save to file on the ESP
    Status = egSaveFile(NULL, L"screenshot.bmp", FileData, FileDataLength);
    FreePool(FileData);
    if (EFI_ERROR(Status)) {
        Print(L"Error egSaveFile: %x\n", Status);
        goto bailout_wait;
    }
    
    return;
    
    // DEBUG: switch to text mode
bailout_wait:
    egSetGraphicsModeEnabled(FALSE);
    BS->WaitForEvent(1, &ST->ConIn->WaitForKey, &Index);
}

/* EOF */
