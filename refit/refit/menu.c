/*
 * refit/menu.c
 * Menu functions
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

static CHAR16 arrowUp[2] = { ARROW_UP, 0 };
static CHAR16 arrowDown[2] = { ARROW_DOWN, 0 };

#ifndef TEXTONLY

#include "image_back_normal_big.h"
#include "image_back_normal_small.h"
#include "image_back_selected_big.h"
#include "image_back_selected_small.h"

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
    BOOLEAN HaveTimeout;
    UINTN TimeoutCountdown;
    CHAR16 *TimeoutMessage;
    BOOLEAN isScrolling, paintAll, running;
    CHAR16 **DisplayStrings;
    
    chosenIndex = 0;
    maxIndex = (INTN)Screen->EntryCount - 1;
    
    if (Screen->TimeoutSeconds > 0) {
        HaveTimeout = TRUE;
        TimeoutCountdown = Screen->TimeoutSeconds * 10;
    } else
        HaveTimeout = FALSE;
    
    firstVisible = 0;
    maxVisible = (INTN)ConHeight - 5;  // includes -1 offset for "last" counting method
    if (HaveTimeout)
        maxVisible -= 2;
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
        
        if (HaveTimeout) {
            TimeoutMessage = PoolPrint(L"%s in %d seconds  ", Screen->TimeoutText, (TimeoutCountdown + 5) / 10);
            ST->ConOut->SetAttribute(ST->ConOut, ATTR_ERROR);
            ST->ConOut->SetCursorPosition(ST->ConOut, 3, ConHeight - 1);
            ST->ConOut->OutputString(ST->ConOut, TimeoutMessage);
            FreePool(TimeoutMessage);
        }
        
        Status = ST->ConIn->ReadKeyStroke(ST->ConIn, &key);
        if (Status == EFI_NOT_READY) {
            if (HaveTimeout && TimeoutCountdown == 0) {
                // timeout expired
                break;
            } else if (HaveTimeout) {
                BS->Stall(100000);
                TimeoutCountdown--;
            } else
                BS->WaitForEvent(1, &ST->ConIn->WaitForKey, &index);
            continue;
        }
        if (HaveTimeout) {
            // the user pressed a key, cancel the timeout
            ST->ConOut->SetAttribute(ST->ConOut, ATTR_BASIC);
            ST->ConOut->SetCursorPosition(ST->ConOut, 0, ConHeight - 1);
            ST->ConOut->OutputString(ST->ConOut, BlankLine + 1);
            HaveTimeout = FALSE;
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
    BOOLEAN HaveTimeout;
    UINTN TimeoutCountdown;
    CHAR16 *TimeoutMessage;
    BOOLEAN running;
    
    lastChosenIndex = chosenIndex = 0;
    maxIndex = (INTN)Screen->EntryCount - 1;
    running = TRUE;
    if (Screen->TimeoutSeconds > 0) {
        HaveTimeout = TRUE;
        TimeoutCountdown = Screen->TimeoutSeconds * 10;
    } else
        HaveTimeout = FALSE;
    
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
    SwitchToGraphicsAndClear();
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
        
        if (HaveTimeout) {
            TimeoutMessage = PoolPrint(L"%s in %d seconds", Screen->TimeoutText, (TimeoutCountdown + 5) / 10);
            RenderText(TimeoutMessage, &TextBuffer);
            BltImage(&TextBuffer, (UGAWidth - TextBuffer.Width) >> 1, row1PosY + 80 + 16 + 16);
            FreePool(TimeoutMessage);
        }
        
        Status = ST->ConIn->ReadKeyStroke(ST->ConIn, &key);
        if (Status == EFI_NOT_READY) {
            if (HaveTimeout && TimeoutCountdown == 0) {
                // timeout expired
                break;
            } else if (HaveTimeout) {
                BS->Stall(100000);
                TimeoutCountdown--;
            } else
                BS->WaitForEvent(1, &ST->ConIn->WaitForKey, &index);
            continue;
        }
        if (HaveTimeout) {
            // the user pressed a key, cancel the timeout
            RenderText(L"", &TextBuffer);
            BltImage(&TextBuffer, (UGAWidth - TextBuffer.Width) >> 1, row1PosY + 80 + 16 + 16);
            HaveTimeout = FALSE;
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
