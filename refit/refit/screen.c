/*
 * refit/screen.c
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

#include "lib.h"

// Console defines and variables

static EFI_GUID gEfiConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;
static EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl;

UINTN ConWidth;
UINTN ConHeight;
CHAR16 *BlankLine;

static VOID SwitchToText(IN BOOLEAN CursorEnabled);
static VOID SwitchToGraphics(VOID);
static VOID DrawScreenHeader(IN CHAR16 *Title);
static VOID PauseForKey(VOID);

// UGA defines and variables

static EFI_GUID gEfiUgaDrawProtocolGuid = EFI_UGA_DRAW_PROTOCOL_GUID;
static EFI_UGA_DRAW_PROTOCOL *UGA;

UINTN UGAWidth;
UINTN UGAHeight;
BOOLEAN AllowGraphicsMode;

static BOOLEAN InGraphicsMode;
static BOOLEAN GraphicsScreenDirty;

#ifndef TEXTONLY
static EFI_UGA_PIXEL BackgroundPixel = { 0xbf, 0xbf, 0xbf, 0 };
#endif  /* !TEXTONLY */

// general defines and variables

static BOOLEAN haveError = FALSE;

//
// Screen handling
//

VOID InitScreen(VOID)
{
    EFI_STATUS Status;
    EFI_CONSOLE_CONTROL_SCREEN_MODE CurrentMode;
#ifndef TEXTONLY
    UINT32 UGADepth;
    UINT32 UGARefreshRate;
#endif  /* !TEXTONLY */
    UINTN i;
    
    // get protocols
    Status = BS->LocateProtocol(&gEfiConsoleControlProtocolGuid, NULL, &ConsoleControl);
    if (Status != EFI_SUCCESS)
        ConsoleControl = NULL;
#ifndef TEXTONLY
    Status = BS->LocateProtocol(&gEfiUgaDrawProtocolGuid, NULL, &UGA);
    if (Status != EFI_SUCCESS)
        UGA = NULL;
#endif  /* !TEXTONLY */
    
    // now, look at what we have and at the current mode
    if (ConsoleControl == NULL) {
        // no ConSplitter, assume text-only
        InGraphicsMode = FALSE;
        AllowGraphicsMode = FALSE;
        
    } else {
        // we have a ConSplitter, check current mode
        ConsoleControl->GetMode(ConsoleControl, &CurrentMode, NULL, NULL);
#ifndef TEXTONLY
        if (UGA == NULL) {
#endif  /* !TEXTONLY */
            // ...but no graphics. Strange, but still run in text-only mode
            if (CurrentMode == EfiConsoleControlScreenGraphics)
                ConsoleControl->SetMode(ConsoleControl, EfiConsoleControlScreenText);
            InGraphicsMode = FALSE;
            AllowGraphicsMode = FALSE;
            
#ifndef TEXTONLY
        } else {
            // we have everything, horray!
            // get screen size
            Status = UGA->GetMode(UGA, &UGAWidth, &UGAHeight, &UGADepth, &UGARefreshRate);
            if (EFI_ERROR(Status)) {
                // TODO: error message
                // fall back to text mode
                if (CurrentMode == EfiConsoleControlScreenGraphics)
                    ConsoleControl->SetMode(ConsoleControl, EfiConsoleControlScreenText);
                InGraphicsMode = FALSE;
                AllowGraphicsMode = FALSE;
                
            } else {
                InGraphicsMode = (CurrentMode == EfiConsoleControlScreenGraphics) ? TRUE : FALSE;
                AllowGraphicsMode = TRUE;
            }
        }
#endif  /* !TEXTONLY */
    }
    
    GraphicsScreenDirty = TRUE;
#ifndef TEXTONLY
    if (AllowGraphicsMode && InGraphicsMode) {
        // display banner during init phase
        BltClearScreen(TRUE);
    }
#endif  /* !TEXTONLY */
    
    if (!InGraphicsMode) {
        // disable cursor
        ST->ConOut->EnableCursor(ST->ConOut, FALSE);
    }
    
    // get size of text console
    if (ST->ConOut->QueryMode(ST->ConOut, ST->ConOut->Mode->Mode, &ConWidth, &ConHeight) != EFI_SUCCESS) {
        // use default values on error
        ConWidth = 80;
        ConHeight = 25;
    }
    
    // make a buffer for the whole line
    BlankLine = AllocatePool((ConWidth + 1) * sizeof(CHAR16));
    for (i = 0; i < ConWidth; i++)
        BlankLine[i] = ' ';
    BlankLine[i] = 0;
    
    // show the banner (even when in graphics mode)
    DrawScreenHeader(L"rEFIt - Initializing...");
}

VOID BeginTextScreen(IN CHAR16 *Title)
{
    DrawScreenHeader(Title);
    SwitchToText(FALSE);
    
    // reset error flag
    haveError = FALSE;
}

VOID FinishTextScreen(IN BOOLEAN WaitAlways)
{
    if (haveError || WaitAlways) {
        SwitchToText(FALSE);
        PauseForKey();
    }
    
    // reset error flag
    haveError = FALSE;
}

VOID BeginExternalScreen(IN UINTN Mode, IN CHAR16 *Title)
{
    // modes:
    //  0 text screen with header
    //  1 graphics, blank grey
    
    if (!AllowGraphicsMode)
        Mode = 0;
    
#ifndef TEXTONLY
    if (Mode == 1) {
        SwitchToGraphics();
        BltClearScreen(FALSE);
    }
#endif  /* !TEXTONLY */
    
    // NOTE: The following happens always, because we might switch back to text mode later
    //       to show errors
    // show the header
    DrawScreenHeader(Title);
    
    if (Mode == 0)
        SwitchToText(TRUE);
    
    // reset error flag
    haveError = FALSE;
}

VOID FinishExternalScreen(VOID)
{
    // sync our internal state
    if (ConsoleControl != NULL) {
        EFI_CONSOLE_CONTROL_SCREEN_MODE CurrentMode;
        ConsoleControl->GetMode(ConsoleControl, &CurrentMode, NULL, NULL);
        InGraphicsMode = (CurrentMode == EfiConsoleControlScreenGraphics) ? TRUE : FALSE;
    }
    GraphicsScreenDirty = TRUE;
    
    if (haveError) {
        SwitchToText(FALSE);
        PauseForKey();
    }
    
    // reset error flag
    haveError = FALSE;
}

VOID TerminateScreen(VOID)
{
    if (!InGraphicsMode) {
        // clear text screen
        ST->ConOut->SetAttribute(ST->ConOut, ATTR_BASIC);
        ST->ConOut->ClearScreen(ST->ConOut);
    }
    
    // enable cursor
    ST->ConOut->EnableCursor(ST->ConOut, TRUE);
}

static VOID SwitchToText(IN BOOLEAN CursorEnabled)
{
    if (InGraphicsMode && ConsoleControl != NULL) {
        ConsoleControl->SetMode(ConsoleControl, EfiConsoleControlScreenText);
        InGraphicsMode = FALSE;
    }
    ST->ConOut->EnableCursor(ST->ConOut, CursorEnabled);
}

static VOID SwitchToGraphics(VOID)
{
    if (!InGraphicsMode && AllowGraphicsMode) {
        ConsoleControl->SetMode(ConsoleControl, EfiConsoleControlScreenGraphics);
        InGraphicsMode = TRUE;
        GraphicsScreenDirty = TRUE;
    }
}

static VOID DrawScreenHeader(IN CHAR16 *Title)
{
    UINTN y;
    
    // clear to black background
    ST->ConOut->SetAttribute(ST->ConOut, ATTR_BASIC);
    ST->ConOut->ClearScreen(ST->ConOut);
    
    // paint header background
    ST->ConOut->SetAttribute(ST->ConOut, ATTR_BANNER);
    for (y = 0; y < 3; y++) {
        ST->ConOut->SetCursorPosition(ST->ConOut, 0, y);
        ST->ConOut->OutputString(ST->ConOut, BlankLine);
    }
    
    // print header text
    ST->ConOut->SetCursorPosition(ST->ConOut, 3, 1);
    ST->ConOut->OutputString(ST->ConOut, Title);
    
    // reposition cursor
    ST->ConOut->SetAttribute(ST->ConOut, ATTR_BASIC);
    ST->ConOut->SetCursorPosition(ST->ConOut, 0, 4);
}

static VOID PauseForKey(VOID)
{
    UINTN index;
    EFI_INPUT_KEY key;
    
    Print(L"\n* Hit any key to continue *");
    BS->WaitForEvent(1, &ST->ConIn->WaitForKey, &index);
    ST->ConIn->ReadKeyStroke(ST->ConIn, &key);
    Print(L"\n");
}

//
// Error handling
//

BOOLEAN CheckFatalError(IN EFI_STATUS Status, IN CHAR16 *where)
{
    CHAR16 ErrorName[64];
    
    if (!(EFI_ERROR(Status)))
        return FALSE;
    
    StatusToString(ErrorName, Status);
    ST->ConOut->SetAttribute(ST->ConOut, ATTR_ERROR);
    Print(L"Fatal Error: %s %s\n", ErrorName, where);
    ST->ConOut->SetAttribute(ST->ConOut, ATTR_BASIC);
    haveError = TRUE;
    
    //BS->Exit(ImageHandle, ExitStatus, ExitDataSize, ExitData);
    
    return TRUE;
}

BOOLEAN CheckError(IN EFI_STATUS Status, IN CHAR16 *where)
{
    CHAR16 ErrorName[64];
    
    if (!(EFI_ERROR(Status)))
        return FALSE;
    
    StatusToString(ErrorName, Status);
    ST->ConOut->SetAttribute(ST->ConOut, ATTR_ERROR);
    Print(L"Error: %s %s\n", ErrorName, where);
    ST->ConOut->SetAttribute(ST->ConOut, ATTR_BASIC);
    haveError = TRUE;
    
    return TRUE;
}

//
// Graphics functions
//

#ifndef TEXTONLY

VOID SwitchToGraphicsAndClear(VOID)
{
    SwitchToGraphics();
    if (GraphicsScreenDirty)
        BltClearScreen(TRUE);
}

VOID BltClearScreen(IN BOOLEAN ShowBanner)
{
    UGA->Blt(UGA, &BackgroundPixel, EfiUgaVideoFill, 0, 0, 0, 0, UGAWidth, UGAHeight, 0);
    if (ShowBanner) {
        REFIT_IMAGE *banner = BuiltinImage(1);
        BltImage(banner, (UGAWidth - banner->Width) >> 1, (UGAHeight - LAYOUT_TOTAL_HEIGHT) >> 1);
    }
    GraphicsScreenDirty = FALSE;
}

VOID BltImage(IN REFIT_IMAGE *Image, IN UINTN XPos, IN UINTN YPos)
{
    UGA->Blt(UGA, (EFI_UGA_PIXEL *)Image->PixelData, EfiUgaBltBufferToVideo,
             0, 0, XPos, YPos, Image->Width, Image->Height, 0);
    GraphicsScreenDirty = TRUE;
}

VOID BltImageComposite(IN REFIT_IMAGE *BaseImage, IN REFIT_IMAGE *TopImage, IN UINTN XPos, IN UINTN YPos)
{
    EFI_UGA_PIXEL *CompositeData;
    UINTN x, y, TopOffsetX, TopOffsetY;
    
    CompositeData = AllocatePool(BaseImage->Width * BaseImage->Height * 4);
    CopyMem(CompositeData, (VOID *)BaseImage->PixelData, BaseImage->Width * BaseImage->Height * 4);
    TopOffsetX = (BaseImage->Width - TopImage->Width) >> 1;
    TopOffsetY = (BaseImage->Height - TopImage->Height) >> 1;
    
    for (y = 0; y < TopImage->Height; y++) {
        const UINT8 *BasePtr = BaseImage->PixelData + (y + TopOffsetY) * BaseImage->Width * 4 + TopOffsetX * 4;
        const UINT8 *TopPtr = TopImage->PixelData + y * TopImage->Width * 4;
        EFI_UGA_PIXEL *CompPtr = CompositeData + (y + TopOffsetY) * BaseImage->Width + TopOffsetX;
        for (x = 0; x < TopImage->Width; x++) {
            UINTN Alpha = TopPtr[3];
            UINTN RevAlpha = 255 - Alpha;
            CompPtr->Blue = ((UINTN)(*BasePtr++) * RevAlpha + (UINTN)(*TopPtr++) * Alpha) / 255;
            CompPtr->Green = ((UINTN)(*BasePtr++) * RevAlpha + (UINTN)(*TopPtr++) * Alpha) / 255;
            CompPtr->Red = ((UINTN)(*BasePtr++) * RevAlpha + (UINTN)(*TopPtr++) * Alpha) / 255;
            BasePtr++, TopPtr++, CompPtr++;
        }
    }
    
    UGA->Blt(UGA, CompositeData, EfiUgaBltBufferToVideo,
             0, 0, XPos, YPos, BaseImage->Width, BaseImage->Height, 0);
    FreePool(CompositeData);
    GraphicsScreenDirty = TRUE;
}

VOID RenderText(IN CHAR16 *Text, IN OUT REFIT_IMAGE *BackBuffer)
{
    UINT8 *Ptr;
    UINT8 *FontPtr;
    UINTN TextLength, TextWidth;
    UINTN i, c, y;
    UINTN LineOffset, FontLineOffset;
    REFIT_IMAGE *FontImage;
    
    // clear the buffer
    Ptr = BackBuffer->PixelData;
    for (i = 0; i < BackBuffer->Width * BackBuffer->Height; i++) {
        *Ptr++ = 0xbf;
        *Ptr++ = 0xbf;
        *Ptr++ = 0xbf;
        *Ptr++ = 0;
    }
    
    // fit the text
    TextLength = StrLen(Text);
    TextWidth = TextLength * FONT_CELL_WIDTH;
    if (BackBuffer->Width < TextWidth) {
        TextLength = BackBuffer->Width / FONT_CELL_WIDTH;
        TextWidth = TextLength * FONT_CELL_WIDTH;
    }
    
    // load the font
    FontImage = BuiltinImage(0);
    
    // render it
    Ptr = BackBuffer->PixelData + ((BackBuffer->Width - TextWidth) >> 1) * 4;
    LineOffset = BackBuffer->Width * 4;
    FontLineOffset = FontImage->Width * 4;
    for (i = 0; i < TextLength; i++) {
        c = Text[i];
        if (c < 32 || c >= 127)
            c = 95;
        else
            c -= 32;
        FontPtr = FontImage->PixelData + c * FONT_CELL_WIDTH * 4;
        for (y = 0; y < FONT_CELL_HEIGHT; y++)
            CopyMem(Ptr + y * LineOffset, FontPtr + y * FontLineOffset, FONT_CELL_WIDTH * 4);
        Ptr += FONT_CELL_WIDTH * 4;
    }
}

#endif  /* !TEXTONLY */
