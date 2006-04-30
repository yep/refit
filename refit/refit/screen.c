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

UINTN ConWidth;
UINTN ConHeight;
CHAR16 *BlankLine;

static VOID SwitchToText(IN BOOLEAN CursorEnabled);
static VOID SwitchToGraphics(VOID);
static VOID DrawScreenHeader(IN CHAR16 *Title);
static VOID PauseForKey(VOID);

// UGA defines and variables

UINTN UGAWidth;
UINTN UGAHeight;
BOOLEAN AllowGraphicsMode;

static BOOLEAN GraphicsScreenDirty;

#ifndef TEXTONLY

static EG_PIXEL BackgroundPixel = { 0xbf, 0xbf, 0xbf, 0 };

#endif  /* !TEXTONLY */

// general defines and variables

static BOOLEAN haveError = FALSE;

//
// Screen handling
//

VOID InitScreen(VOID)
{
    UINTN i;
    
    // initialize libeg
    egInitScreen();
    
#ifndef TEXTONLY
    if (egHasGraphicsMode()) {
        egGetScreenSize(&UGAWidth, &UGAHeight);
        AllowGraphicsMode = TRUE;
    } else {
#endif  /* !TEXTONLY */
        AllowGraphicsMode = FALSE;
        egSetGraphicsModeEnabled(FALSE);
#ifndef TEXTONLY
    }
#endif  /* !TEXTONLY */
    
    GraphicsScreenDirty = TRUE;
#ifndef TEXTONLY
    if (AllowGraphicsMode) {
        // display banner during init phase
        BltClearScreen(TRUE);
    }
#endif  /* !TEXTONLY */
    
    // disable cursor
    ST->ConOut->EnableCursor(ST->ConOut, FALSE);
    
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
    DrawScreenHeader(L"Initializing...");
}

VOID SetTextOnly(VOID)
{
    AllowGraphicsMode = FALSE;
    SwitchToText(TRUE);
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
    // clear text screen
    ST->ConOut->SetAttribute(ST->ConOut, ATTR_BASIC);
    ST->ConOut->ClearScreen(ST->ConOut);
    
    // enable cursor
    ST->ConOut->EnableCursor(ST->ConOut, TRUE);
}

static VOID SwitchToText(IN BOOLEAN CursorEnabled)
{
    egSetGraphicsModeEnabled(FALSE);
    ST->ConOut->EnableCursor(ST->ConOut, CursorEnabled);
}

static VOID SwitchToGraphics(VOID)
{
    if (AllowGraphicsMode && !egIsGraphicsModeEnabled()) {
        egSetGraphicsModeEnabled(TRUE);
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
        Print(BlankLine);
    }
    
    // print header text
    ST->ConOut->SetCursorPosition(ST->ConOut, 3, 1);
    Print(L"rEFIt - %s", Title);
    
    // reposition cursor
    ST->ConOut->SetAttribute(ST->ConOut, ATTR_BASIC);
    ST->ConOut->SetCursorPosition(ST->ConOut, 0, 4);
}

static BOOLEAN ReadAllKeyStrokes(VOID)
{
    BOOLEAN       GotKeyStrokes;
    EFI_STATUS    Status;
    EFI_INPUT_KEY key;
    
    GotKeyStrokes = FALSE;
    for (;;) {
        Status = ST->ConIn->ReadKeyStroke(ST->ConIn, &key);
        if (Status == EFI_SUCCESS) {
            GotKeyStrokes = TRUE;
            continue;
        }
        break;
    }
    return GotKeyStrokes;
}

static VOID PauseForKey(VOID)
{
    UINTN index;
    
    Print(L"\n* Hit any key to continue *");
    
    if (ReadAllKeyStrokes()) {  // remove buffered key strokes
        BS->Stall(5000000);     // 5 seconds delay
        ReadAllKeyStrokes();    // empty the buffer again
    }
    
    BS->WaitForEvent(1, &ST->ConIn->WaitForKey, &index);
    ReadAllKeyStrokes();        // empty the buffer to protect the menu
    
    Print(L"\n");
}

#if REFIT_DEBUG > 0
VOID DebugPause(VOID)
{
    // show console and wait for key
    SwitchToText(FALSE);
    PauseForKey();
    
    // reset error flag
    haveError = FALSE;
}
#endif

//
// Error handling
//

BOOLEAN CheckFatalError(IN EFI_STATUS Status, IN CHAR16 *where)
{
    CHAR16 ErrorName[64];
    
    if (!EFI_ERROR(Status))
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
    
    if (!EFI_ERROR(Status))
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
    egClearScreen(&BackgroundPixel);
    if (ShowBanner) {
        EG_IMAGE *banner = BuiltinImage(1);
        BltImage(banner, (UGAWidth - banner->Width) >> 1, (UGAHeight - LAYOUT_TOTAL_HEIGHT) >> 1);
    }
    GraphicsScreenDirty = FALSE;
}

VOID BltImage(IN EG_IMAGE *Image, IN UINTN XPos, IN UINTN YPos)
{
    egDrawImage(Image, XPos, YPos);
    GraphicsScreenDirty = TRUE;
}

VOID BltImageAlpha(IN EG_IMAGE *Image, IN UINTN XPos, IN UINTN YPos)
{
    EG_IMAGE *CompImage;
    
    // compose on standard background
    CompImage = egCreateFilledImage(Image->Width, Image->Height, FALSE, &BackgroundPixel);
    egComposeImage(CompImage, Image, 0, 0);
    
    // blit to screen and clean up
    egDrawImage(CompImage, XPos, YPos);
    egFreeImage(CompImage);
    GraphicsScreenDirty = TRUE;
}

VOID BltImageComposite(IN EG_IMAGE *BaseImage, IN EG_IMAGE *TopImage, IN UINTN XPos, IN UINTN YPos)
{
    UINTN TotalWidth, TotalHeight, CompWidth, CompHeight, OffsetX, OffsetY;
    EG_IMAGE *CompImage;
    
    // initialize buffer with base image
    CompImage = egCopyImage(BaseImage);
    TotalWidth  = BaseImage->Width;
    TotalHeight = BaseImage->Height;
    
    // place the top image
    CompWidth = TopImage->Width;
    if (CompWidth > TotalWidth)
        CompWidth = TotalWidth;
    OffsetX = (TotalWidth - CompWidth) >> 1;
    CompHeight = TopImage->Height;
    if (CompHeight > TotalHeight)
        CompHeight = TotalHeight;
    OffsetY = (TotalHeight - CompHeight) >> 1;
    egComposeImage(CompImage, TopImage, OffsetX, OffsetY);
    
    // blit to screen and clean up
    egDrawImage(CompImage, XPos, YPos);
    egFreeImage(CompImage);
    GraphicsScreenDirty = TRUE;
}

VOID BltImageCompositeBadge(IN EG_IMAGE *BaseImage, IN EG_IMAGE *TopImage, IN EG_IMAGE *BadgeImage, IN UINTN XPos, IN UINTN YPos)
{
    UINTN TotalWidth, TotalHeight, CompWidth, CompHeight, OffsetX, OffsetY;
    EG_IMAGE *CompImage;
    
    // initialize buffer with base image
    CompImage = egCopyImage(BaseImage);
    TotalWidth  = BaseImage->Width;
    TotalHeight = BaseImage->Height;
    
    // place the top image
    CompWidth = TopImage->Width;
    if (CompWidth > TotalWidth)
        CompWidth = TotalWidth;
    OffsetX = (TotalWidth - CompWidth) >> 1;
    CompHeight = TopImage->Height;
    if (CompHeight > TotalHeight)
        CompHeight = TotalHeight;
    OffsetY = (TotalHeight - CompHeight) >> 1;
    egComposeImage(CompImage, TopImage, OffsetX, OffsetY);
    
    // place the badge image
    if (BadgeImage != NULL && (BadgeImage->Width + 8) < CompWidth && (BadgeImage->Height + 8) < CompHeight) {
        OffsetX += CompWidth  - 8 - BadgeImage->Width;
        OffsetY += CompHeight - 8 - BadgeImage->Height;
        egComposeImage(CompImage, BadgeImage, OffsetX, OffsetY);
    }
    
    // blit to screen and clean up
    egDrawImage(CompImage, XPos, YPos);
    egFreeImage(CompImage);
    GraphicsScreenDirty = TRUE;
}

#endif  /* !TEXTONLY */
