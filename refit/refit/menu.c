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

#define ATTR_BASIC (EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK)
#define ATTR_BANNER (EFI_WHITE | EFI_BACKGROUND_BLUE)
#define ATTR_CHOICE_BASIC ATTR_BASIC
#define ATTR_CHOICE_CURRENT (EFI_WHITE | EFI_BACKGROUND_GREEN)
#define ATTR_SCROLLARROW (EFI_LIGHTGREEN | EFI_BACKGROUND_BLACK)

static UINTN screen_width;
static UINTN screen_height;
static CHAR16 *BlankLine;
static CHAR16 arrowUp[2] = { ARROW_UP, 0 };
static CHAR16 arrowDown[2] = { ARROW_DOWN, 0 };


EFI_GUID gEfiConsoleControlProtocolGuid = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;

static VOID ScreenInitCommon(VOID)
{
    EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl;
    EFI_CONSOLE_CONTROL_SCREEN_MODE currentMode;
    
    // switch console to text mode if necessary
    if (BS->LocateProtocol(&gEfiConsoleControlProtocolGuid, NULL, &ConsoleControl) == EFI_SUCCESS) {
        ConsoleControl->GetMode(ConsoleControl, &currentMode, NULL, NULL);
        if (currentMode == EfiConsoleControlScreenGraphics) {
            ConsoleControl->SetMode(ConsoleControl, EfiConsoleControlScreenText);
        }
    }
    
    // disable cursor
    ST->ConOut->EnableCursor(ST->ConOut, FALSE);
}

VOID ScreenInit(VOID)
{
    UINTN i;
    
    ScreenInitCommon();
    
    // get size of display
    if (ST->ConOut->QueryMode(ST->ConOut, ST->ConOut->Mode->Mode, &screen_width, &screen_height) != EFI_SUCCESS) {
        screen_width = 80;
        screen_height = 25;
    }
    
    // make a buffer for the whole line
    BlankLine = AllocatePool(screen_width + 1);
    for (i = 0; i < screen_width; i++)
        BlankLine[i] = ' ';
    BlankLine[i] = 0;    
}

VOID ScreenReinit(VOID)
{
    ScreenInitCommon();
}

VOID ScreenLeave(IN UINTN State)
{
    // states:
    //  0 text, as is
    //  1 text, blank screen
    //  2 graphics
    
    // clear text screen if requested
    if (State > 0) {
        ST->ConOut->SetAttribute(ST->ConOut, ATTR_BASIC);
        ST->ConOut->ClearScreen(ST->ConOut);
    }
    
    // enable cursor
    ST->ConOut->EnableCursor(ST->ConOut, TRUE);
    
    // switch to graphics if requested
    if (State == 2) {
        EFI_CONSOLE_CONTROL_PROTOCOL *ConsoleControl;
        if (BS->LocateProtocol(&gEfiConsoleControlProtocolGuid, NULL, &ConsoleControl) == EFI_SUCCESS) {
            ConsoleControl->SetMode(ConsoleControl, EfiConsoleControlScreenGraphics);
        }
    }
}

VOID ScreenHeader(IN CHAR16 *Title)
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

VOID ScreenWaitForKey(VOID)
{
    UINTN index;
    EFI_INPUT_KEY key;
    
    Print(L"\n* Hit any key to continue *");
    BS->WaitForEvent(1, &ST->ConIn->WaitForKey, &index);
    ST->ConIn->ReadKeyStroke(ST->ConIn, &key);
    Print(L"\n");
}


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

VOID MenuRun(IN REFIT_MENU_SCREEN *Screen, OUT REFIT_MENU_ENTRY **ChosenEntry)
{
    UINTN index;
    INTN i, chosenIndex, lastChosenIndex, maxIndex;
    INTN firstVisible, lastVisible, maxVisible, maxFirstVisible;
    UINTN menuWidth, itemWidth;
    EFI_STATUS status;
    EFI_INPUT_KEY key;
    BOOLEAN isScrolling, paintAll, running;
    CHAR16 **DisplayStrings;
    
    chosenIndex = 0;
    maxIndex = (INTN)Screen->EntryCount - 1;
    firstVisible = 0;
    maxVisible = (INTN)screen_height - 5;  // includes -1 offset for "last" counting method
    maxFirstVisible = maxIndex - maxVisible;
    if (maxFirstVisible < 0)
        maxFirstVisible = 0;   // non-scrolling case
    isScrolling = (maxIndex > maxVisible) ? TRUE : FALSE;
    running = TRUE;
    paintAll = TRUE;
    
    ScreenHeader(Screen->Title);
    
    // determine width of the menu
    menuWidth = 20;  // minimum
    for (i = 0; i <= maxIndex; i++) {
        itemWidth = StrLen(Screen->Entries[i].Title);
        if (menuWidth < itemWidth)
            menuWidth = itemWidth;
    }
    if (menuWidth > screen_width - 6)
        menuWidth = screen_width - 6;
    
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
        
        status = ST->ConIn->ReadKeyStroke(ST->ConIn, &key);
        if (status == EFI_NOT_READY) {
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
    
    FreePool(DisplayStrings);
    
    if (ChosenEntry) {
        if (chosenIndex < 0)
            *ChosenEntry = NULL;
        else
            *ChosenEntry = Screen->Entries + chosenIndex;
    }
    return;
}
