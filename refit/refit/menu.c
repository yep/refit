/*
 * refit/menu.c
 * Screen handling and menu functions
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

static UINTN ConWidth;
static UINTN ConHeight;
static CHAR16 *BlankLine;
static CHAR16 arrowUp[2] = { ARROW_UP, 0 };
static CHAR16 arrowDown[2] = { ARROW_DOWN, 0 };

#define ATTR_BASIC (EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK)
#define ATTR_ERROR (EFI_YELLOW | EFI_BACKGROUND_BLACK)
#define ATTR_BANNER (EFI_WHITE | EFI_BACKGROUND_BLUE)
#define ATTR_CHOICE_BASIC ATTR_BASIC
#define ATTR_CHOICE_CURRENT (EFI_WHITE | EFI_BACKGROUND_GREEN)
#define ATTR_SCROLLARROW (EFI_LIGHTGREEN | EFI_BACKGROUND_BLACK)

static VOID SwitchToText(IN BOOLEAN CursorEnabled);
static VOID SwitchToGraphics(VOID);
static VOID DrawScreenHeader(IN CHAR16 *Title);
static VOID PauseForKey(VOID);

// UGA defines and variables

static EFI_GUID gEfiUgaDrawProtocolGuid = EFI_UGA_DRAW_PROTOCOL_GUID;
static EFI_UGA_DRAW_PROTOCOL *UGA;
static UINTN UGAWidth;
static UINTN UGAHeight;
static BOOLEAN InGraphicsMode;
static BOOLEAN AllowGraphicsMode;
static BOOLEAN GraphicsScreenDirty;

#ifndef TEXTONLY

static EFI_UGA_PIXEL BackgroundPixel = { 0xbf, 0xbf, 0xbf, 0 };

#include "image_refit_banner.h"
#include "image_back_normal_big.h"
#include "image_back_normal_small.h"
#include "image_back_selected_big.h"
#include "image_back_selected_small.h"

#include "image_font.h"
#define FONT_CELL_WIDTH (7)
#define FONT_CELL_HEIGHT (12)

#define LAYOUT_TEXT_WIDTH (512)
#define LAYOUT_TOTAL_HEIGHT (368)

static VOID BltClearScreen(VOID);
static VOID BltImage(REFIT_IMAGE *Image, UINTN XPos, UINTN YPos);
static VOID BltImageComposite(REFIT_IMAGE *BaseImage, REFIT_IMAGE *TopImage, UINTN XPos, UINTN YPos);
static VOID RenderText(IN CHAR16 *Text, IN OUT REFIT_IMAGE *BackBuffer);

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
    
#ifndef TEXTONLY
    if (AllowGraphicsMode && InGraphicsMode) {
        // display banner during init phase
        BltClearScreen();
        BltImage(&image_refit_banner, (UGAWidth - image_refit_banner.Width) >> 1, (UGAHeight - LAYOUT_TOTAL_HEIGHT) >> 1);
    }
    GraphicsScreenDirty = FALSE;
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
    BlankLine = AllocatePool(ConWidth + 1);
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
        BltClearScreen();
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

static VOID BltClearScreen(VOID)
{
    UGA->Blt(UGA, &BackgroundPixel, EfiUgaVideoFill, 0, 0, 0, 0, UGAWidth, UGAHeight, 0);
}

static VOID BltImage(REFIT_IMAGE *Image, UINTN XPos, UINTN YPos)
{
    UGA->Blt(UGA, (EFI_UGA_PIXEL *)Image->PixelData, EfiUgaBltBufferToVideo,
             0, 0, XPos, YPos, Image->Width, Image->Height, 0);
}

static VOID BltImageComposite(REFIT_IMAGE *BaseImage, REFIT_IMAGE *TopImage, UINTN XPos, UINTN YPos)
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
}

static VOID RenderText(IN CHAR16 *Text, IN OUT REFIT_IMAGE *BackBuffer)
{
    UINT8 *Ptr;
    UINT8 *FontPtr;
    UINTN TextLength, TextWidth;
    UINTN i, c, y;
    UINTN LineOffset, FontLineOffset;
    
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
    
    // render it
    Ptr = BackBuffer->PixelData + ((BackBuffer->Width - TextWidth) >> 1) * 4;
    LineOffset = BackBuffer->Width * 4;
    FontLineOffset = image_font.Width * 4;
    for (i = 0; i < TextLength; i++) {
        c = Text[i];
        if (c < 32 || c >= 127)
            c = 95;
        else
            c -= 32;
        FontPtr = image_font_data + c * FONT_CELL_WIDTH * 4;
        for (y = 0; y < FONT_CELL_HEIGHT; y++)
            CopyMem(Ptr + y * LineOffset, FontPtr + y * FontLineOffset, FONT_CELL_WIDTH * 4);
        Ptr += FONT_CELL_WIDTH * 4;
    }
}

#endif  /* !TEXTONLY */

//
// Menu functions
//

VOID MenuAddEntry(IN REFIT_MENU_SCREEN *Screen, IN REFIT_MENU_ENTRY *Entry)
{
    REFIT_MENU_ENTRY *NewEntries;
    
    if (Screen->EntryCount >= Screen->AllocatedEntryCount) {
        Screen->AllocatedEntryCount += 8;
        NewEntries = AllocatePool(sizeof(REFIT_MENU_ENTRY) * Screen->AllocatedEntryCount);
        if (Screen->EntryCount > 0)
            CopyMem(NewEntries, Screen->Entries, sizeof(REFIT_MENU_ENTRY) * Screen->EntryCount);
        if (Screen->Entries)
            FreePool(Screen->Entries);
        Screen->Entries = NewEntries;
        // TODO: use ReallocatePool instead
    }
    
    CopyMem(Screen->Entries + Screen->EntryCount, Entry, sizeof(REFIT_MENU_ENTRY));
    Screen->EntryCount++;
}

VOID MenuFree(IN REFIT_MENU_SCREEN *Screen)
{
    if (Screen->Entries)
        FreePool(Screen->Entries);
}

static VOID MenuRunText(IN REFIT_MENU_SCREEN *Screen, OUT REFIT_MENU_ENTRY **ChosenEntry)
{
    UINTN index;
    INTN i, chosenIndex, lastChosenIndex, maxIndex;
    INTN firstVisible, lastVisible, maxVisible, maxFirstVisible;
    UINTN menuWidth, itemWidth;
    EFI_STATUS Status;
    EFI_INPUT_KEY key;
    BOOLEAN isScrolling, paintAll, running;
    CHAR16 **DisplayStrings;
    
    chosenIndex = 0;
    maxIndex = (INTN)Screen->EntryCount - 1;
    firstVisible = 0;
    maxVisible = (INTN)ConHeight - 5;  // includes -1 offset for "last" counting method
    maxFirstVisible = maxIndex - maxVisible;
    if (maxFirstVisible < 0)
        maxFirstVisible = 0;   // non-scrolling case
    isScrolling = (maxIndex > maxVisible) ? TRUE : FALSE;
    running = TRUE;
    paintAll = TRUE;
    
    // setup screen
    BeginTextScreen(Screen->Title);
    
    // determine width of the menu
    menuWidth = 20;  // minimum
    for (i = 0; i <= maxIndex; i++) {
        itemWidth = StrLen(Screen->Entries[i].Title);
        if (menuWidth < itemWidth)
            menuWidth = itemWidth;
    }
    if (menuWidth > ConWidth - 6)
        menuWidth = ConWidth - 6;
    
    // prepare strings for display
    DisplayStrings = AllocatePool(sizeof(CHAR16 *) * Screen->EntryCount);
    for (i = 0; i <= maxIndex; i++)
        DisplayStrings[i] = PoolPrint(L" %-.*s ", menuWidth, Screen->Entries[i].Title);
    // TODO: shorten strings that are too long (PoolPrint doesn't do that...)
    // TODO: use more elaborate techniques for shortening too long strings (ellipses in the middle)
    // TODO: account for double-width characters
    
    while (running) {
        // update the screen
        if (paintAll) {
            // paint the whole screen (initially and after scrolling)
            lastVisible = firstVisible + maxVisible;   // done here for code efficiency
            for (i = 0; i <= maxIndex; i++) {
                if (i >= firstVisible && i <= lastVisible) {
                    ST->ConOut->SetCursorPosition(ST->ConOut, 2, 4 + (i - firstVisible));
                    if (i == chosenIndex)
                        ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_CURRENT);
                    else
                        ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_BASIC);
                    ST->ConOut->OutputString(ST->ConOut, DisplayStrings[i]);
                }
            }
            // scrolling indicators
            ST->ConOut->SetAttribute(ST->ConOut, ATTR_SCROLLARROW);
            ST->ConOut->SetCursorPosition(ST->ConOut, 0, 4);
            if (firstVisible > 0)
                ST->ConOut->OutputString(ST->ConOut, arrowUp);
            else
                ST->ConOut->OutputString(ST->ConOut, L" ");
            ST->ConOut->SetCursorPosition(ST->ConOut, 0, 4 + maxVisible);
            if (lastVisible < maxIndex)
                ST->ConOut->OutputString(ST->ConOut, arrowDown);
            else
                ST->ConOut->OutputString(ST->ConOut, L" ");
            
        } else if (chosenIndex != lastChosenIndex) {
            // redraw selection cursor
            ST->ConOut->SetCursorPosition(ST->ConOut, 2, 4 + (lastChosenIndex - firstVisible));
            ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_BASIC);
            ST->ConOut->OutputString(ST->ConOut, DisplayStrings[lastChosenIndex]);
            ST->ConOut->SetCursorPosition(ST->ConOut, 2, 4 + (chosenIndex - firstVisible));
            ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_CURRENT);
            ST->ConOut->OutputString(ST->ConOut, DisplayStrings[chosenIndex]);
            
        }
        paintAll = FALSE;
        lastChosenIndex = chosenIndex;
        
        Status = ST->ConIn->ReadKeyStroke(ST->ConIn, &key);
        if (Status == EFI_NOT_READY) {
            BS->WaitForEvent(1, &ST->ConIn->WaitForKey, &index);
            continue;
        }
        
        switch (key.ScanCode) {
            case 0x01:  // Arrow Up
                if (chosenIndex > 0) {
                    chosenIndex --;
                    if (chosenIndex < firstVisible) {
                        paintAll = TRUE;
                        firstVisible = chosenIndex - (maxVisible >> 1);
                        if (firstVisible < 0)
                            firstVisible = 0;
                    }
                }
                break;
                
            case 0x02:  // Arrow Down
                if (chosenIndex < maxIndex) {
                    chosenIndex ++;
                    if (chosenIndex > lastVisible) {
                        paintAll = TRUE;
                        firstVisible = chosenIndex - (maxVisible >> 1);
                        if (firstVisible > maxFirstVisible)
                            firstVisible = maxFirstVisible;
                    }
                }
                break;
                
            case 0x09:  // PageUp
                if (chosenIndex > 0) {
                    if (chosenIndex == maxIndex) {
                        if (isScrolling)
                            chosenIndex -= maxVisible - 1;  // move to second line without scrolling
                        else
                            chosenIndex = 0;                // move to first entry
                    } else {
                        if (firstVisible > 0)
                            paintAll = TRUE;
                        chosenIndex -= maxVisible;          // move one page and scroll synchronously
                        firstVisible -= maxVisible;
                    }
                    if (chosenIndex < 0)
                        chosenIndex = 0;
                    if (firstVisible < 0)
                        firstVisible = 0;
                }
                break;
                
            case 0x0a:  // PageDown
                if (chosenIndex < maxIndex) {
                    if (chosenIndex == 0) {
                        if (isScrolling)
                            chosenIndex += maxVisible - 1;  // move to second-to-last line without scrolling
                        else
                            chosenIndex = maxIndex;         // move to last entry
                    } else {
                        if (firstVisible < maxFirstVisible)
                            paintAll = TRUE;
                        chosenIndex += maxVisible;          // move one page and scroll synchronously
                        firstVisible += maxVisible;
                    }
                    if (chosenIndex > maxIndex)
                        chosenIndex = maxIndex;
                    if (firstVisible > maxFirstVisible)
                        firstVisible = maxFirstVisible;
                }
                break;
                
            case 0x05:  // Home
                if (chosenIndex > 0) {
                    chosenIndex = 0;
                    if (firstVisible > 0) {
                        paintAll = TRUE;
                        firstVisible = 0;
                    }
                }
                break;
                
            case 0x06:  // End
                if (chosenIndex < maxIndex) {
                    chosenIndex = maxIndex;
                    if (firstVisible < maxFirstVisible) {
                        paintAll = TRUE;
                        firstVisible = maxFirstVisible;
                    }
                }
                break;
                
            case 0x17:  // Escape
                // exit menu screen without selection
                chosenIndex = -1;   // special value only valid on this exit path
                running = FALSE;
                break;
                
        }
        switch (key.UnicodeChar) {
            case 0x0a:
            case 0x0d:
            case 0x20:
                running = FALSE;
                break;
        }
    }
    
    for (i = 0; i <= maxIndex; i++)
        FreePool(DisplayStrings[i]);
    FreePool(DisplayStrings);
    
    if (ChosenEntry) {
        if (chosenIndex < 0)
            *ChosenEntry = NULL;
        else
            *ChosenEntry = Screen->Entries + chosenIndex;
    }
}

#ifndef TEXTONLY

static VOID DrawMenuEntryGraphics(REFIT_MENU_ENTRY *Entry, BOOLEAN selected, UINTN PosX, UINTN PosY)
{
    REFIT_IMAGE *BackgroundImage;
    
    if (Entry->Row == 0) {
        if (selected)
            BackgroundImage = &image_back_selected_big;
        else
            BackgroundImage = &image_back_normal_big;
    } else {
        if (selected)
            BackgroundImage = &image_back_selected_small;
        else
            BackgroundImage = &image_back_normal_small;
    }
    BltImageComposite(BackgroundImage, Entry->Image, PosX, PosY);
}

static VOID MenuRunGraphics(IN REFIT_MENU_SCREEN *Screen, OUT REFIT_MENU_ENTRY **ChosenEntry)
{
    UINTN row0Count, row0PosX, row0PosY, row0PosXRunning;
    UINTN row1Count, row1PosX, row1PosY, row1PosXRunning;
    UINTN *itemPosX;
    UINTN index;
    INTN i, chosenIndex, lastChosenIndex, maxIndex;
    REFIT_IMAGE TextBuffer;
    EFI_STATUS Status;
    EFI_INPUT_KEY key;
    BOOLEAN running;
    
    lastChosenIndex = chosenIndex = 0;
    maxIndex = (INTN)Screen->EntryCount - 1;
    running = TRUE;
    
    row0Count = 0;
    row1Count = 0;
    for (i = 0; i <= maxIndex; i++) {
        if (Screen->Entries[i].Row == 0)
            row0Count++;
        else
            row1Count++;
    }
    row0PosX = (UGAWidth + 8 - (144 + 8) * row0Count) >> 1;
    row0PosY = ((UGAHeight - LAYOUT_TOTAL_HEIGHT) >> 1) + 32 + 32;
    row1PosX = (UGAWidth + 8 - (80 + 8) * row1Count) >> 1;
    row1PosY = row0PosY + 144 + 16;
    
    itemPosX = AllocatePool(sizeof(UINTN) * Screen->EntryCount);
    row0PosXRunning = row0PosX;
    row1PosXRunning = row1PosX;
    for (i = 0; i <= maxIndex; i++) {
        if (Screen->Entries[i].Row == 0) {
            itemPosX[i] = row0PosXRunning;
            row0PosXRunning += 144 + 8;
        } else {
            itemPosX[i] = row1PosXRunning;
            row1PosXRunning += 80 + 8;
        }
    }
    
    TextBuffer.Width = LAYOUT_TEXT_WIDTH;
    TextBuffer.Height = FONT_CELL_HEIGHT;
    TextBuffer.PixelData = AllocatePool(TextBuffer.Width * TextBuffer.Height * 4);
    
    // initial painting
    if (!InGraphicsMode) {
        SwitchToGraphics();
        GraphicsScreenDirty = TRUE;
    }
    if (GraphicsScreenDirty)
        BltClearScreen();
    BltImage(&image_refit_banner, (UGAWidth - image_refit_banner.Width) >> 1, (UGAHeight - LAYOUT_TOTAL_HEIGHT) >> 1);
    for (i = 0; i <= maxIndex; i++) {
        DrawMenuEntryGraphics(&(Screen->Entries[i]), (i == chosenIndex) ? TRUE : FALSE,
                              itemPosX[i], (Screen->Entries[i].Row == 0) ? row0PosY : row1PosY);
    }
    RenderText(Screen->Entries[chosenIndex].Title, &TextBuffer);
    BltImage(&TextBuffer, (UGAWidth - TextBuffer.Width) >> 1, row1PosY + 80 + 16);
    
    while (running) {
        
        if (chosenIndex != lastChosenIndex) {
            DrawMenuEntryGraphics(&(Screen->Entries[lastChosenIndex]), FALSE,
                                  itemPosX[lastChosenIndex], (Screen->Entries[lastChosenIndex].Row == 0) ? row0PosY : row1PosY);
            DrawMenuEntryGraphics(&(Screen->Entries[chosenIndex]), TRUE,
                                  itemPosX[chosenIndex], (Screen->Entries[chosenIndex].Row == 0) ? row0PosY : row1PosY);
            RenderText(Screen->Entries[chosenIndex].Title, &TextBuffer);
            BltImage(&TextBuffer, (UGAWidth - TextBuffer.Width) >> 1, row1PosY + 80 + 16);
        }
        lastChosenIndex = chosenIndex;
        
        Status = ST->ConIn->ReadKeyStroke(ST->ConIn, &key);
        if (Status == EFI_NOT_READY) {
            BS->WaitForEvent(1, &ST->ConIn->WaitForKey, &index);
            continue;
        }
        
        switch (key.ScanCode) {
            case 0x01:  // Arrow Up
            case 0x04:  // Arrow Left
                if (chosenIndex > 0) {
                    chosenIndex --;
                }
                break;
                
            case 0x02:  // Arrow Down
            case 0x03:  // Arrow Right
                if (chosenIndex < maxIndex) {
                    chosenIndex ++;
                }
                break;
                
            case 0x09:  // PageUp
            case 0x05:  // Home
                if (chosenIndex > 0) {
                    chosenIndex = 0;
                }
                break;
                
            case 0x0a:  // PageDown
            case 0x06:  // End
                if (chosenIndex < maxIndex) {
                    chosenIndex = maxIndex;
                }
                break;
                
            case 0x17:  // Escape
                        // exit menu screen without selection
                chosenIndex = -1;   // special value only valid on this exit path
                running = FALSE;
                break;
                
        }
        switch (key.UnicodeChar) {
            case 0x0a:
            case 0x0d:
            case 0x20:
                running = FALSE;
                break;
        }
    }
    
    FreePool(TextBuffer.PixelData);
    FreePool(itemPosX);
    
    if (ChosenEntry) {
        if (chosenIndex < 0)
            *ChosenEntry = NULL;
        else
            *ChosenEntry = Screen->Entries + chosenIndex;
    }
    GraphicsScreenDirty = TRUE;
}

#endif  /* !TEXTONLY */

VOID MenuRun(IN BOOLEAN HasGraphics, IN REFIT_MENU_SCREEN *Screen, OUT REFIT_MENU_ENTRY **ChosenEntry)
{
#ifndef TEXTONLY
    if (HasGraphics && AllowGraphicsMode)
        MenuRunGraphics(Screen, ChosenEntry);
    else
#endif  /* !TEXTONLY */
        MenuRunText(Screen, ChosenEntry);
}
