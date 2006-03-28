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

// scrolling definitions

typedef struct {
    INTN CurrentSelection, LastSelection, MaxIndex;
    INTN FirstVisible, LastVisible, MaxVisible, MaxFirstVisible;
    BOOLEAN IsScrolling, PaintAll, PaintSelection;
} SCROLL_STATE;

#define SCROLL_LINE_UP    (0)
#define SCROLL_LINE_DOWN  (1)
#define SCROLL_PAGE_UP    (2)
#define SCROLL_PAGE_DOWN  (3)
#define SCROLL_FIRST      (4)
#define SCROLL_LAST       (5)

// other menu definitions

static CHAR16 ArrowUp[2] = { ARROW_UP, 0 };
static CHAR16 ArrowDown[2] = { ARROW_DOWN, 0 };

#ifndef TEXTONLY

#define TEXT_SPACING (FONT_CELL_HEIGHT + 4)

static REFIT_IMAGE TextBuffer = { NULL, LAYOUT_TEXT_WIDTH, TEXT_SPACING };

#endif  /* !TEXTONLY */

//
// Scrolling functions
//

static VOID InitScroll(OUT SCROLL_STATE *State, IN UINTN ItemCount, IN UINTN VisibleSpace)
{
    State->LastSelection = State->CurrentSelection = 0;
    State->MaxIndex = (INTN)ItemCount - 1;
    State->FirstVisible = 0;
    if (VisibleSpace == 0)
        State->MaxVisible = State->MaxIndex;
    else
        State->MaxVisible = (INTN)VisibleSpace - 1;
    State->MaxFirstVisible = State->MaxIndex - State->MaxVisible;
    if (State->MaxFirstVisible < 0)
        State->MaxFirstVisible = 0;   // non-scrolling case
    State->IsScrolling = (State->MaxFirstVisible > 0) ? TRUE : FALSE;
    State->PaintAll = TRUE;
    State->PaintSelection = FALSE;
    
    State->LastVisible = State->FirstVisible + State->MaxVisible;
    
    // TODO: use more sane values for the non-scrolling case
}

#define CONSTRAIN_MIN(Variable, MinValue) if (Variable < MinValue) Variable = MinValue
#define CONSTRAIN_MAX(Variable, MaxValue) if (Variable > MaxValue) Variable = MaxValue

static VOID UpdateScroll(IN OUT SCROLL_STATE *State, IN UINTN Movement)
{
    State->LastSelection = State->CurrentSelection;
    
    switch (Movement) {
        case SCROLL_LINE_UP:
            if (State->CurrentSelection > 0) {
                State->CurrentSelection --;
                if (State->CurrentSelection < State->FirstVisible) {
                    State->PaintAll = TRUE;
                    State->FirstVisible = State->CurrentSelection - (State->MaxVisible >> 1);
                    CONSTRAIN_MIN(State->FirstVisible, 0);
                }
            }
            break;
            
        case SCROLL_LINE_DOWN:
            if (State->CurrentSelection < State->MaxIndex) {
                State->CurrentSelection ++;
                if (State->CurrentSelection > State->LastVisible) {
                    State->PaintAll = TRUE;
                    State->FirstVisible = State->CurrentSelection - (State->MaxVisible >> 1);
                    CONSTRAIN_MAX(State->FirstVisible, State->MaxFirstVisible);
                }
            }
            break;
            
        case SCROLL_PAGE_UP:
            if (State->CurrentSelection > 0) {
                if (State->CurrentSelection == State->MaxIndex) {   // currently at last entry, special treatment
                    if (State->IsScrolling)
                        State->CurrentSelection -= State->MaxVisible - 1;  // move to second line without scrolling
                    else
                        State->CurrentSelection = 0;                // move to first entry
                } else {
                    if (State->FirstVisible > 0)
                        State->PaintAll = TRUE;
                    State->CurrentSelection -= State->MaxVisible;          // move one page and scroll synchronously
                    State->FirstVisible -= State->MaxVisible;
                }
                CONSTRAIN_MIN(State->CurrentSelection, 0);
                CONSTRAIN_MIN(State->FirstVisible, 0);
            }
            break;
            
        case SCROLL_PAGE_DOWN:
            if (State->CurrentSelection < State->MaxIndex) {
                if (State->CurrentSelection == 0) {   // currently at first entry, special treatment
                    if (State->IsScrolling)
                        State->CurrentSelection += State->MaxVisible - 1;  // move to second-to-last line without scrolling
                    else
                        State->CurrentSelection = State->MaxIndex;         // move to last entry
                } else {
                    if (State->FirstVisible < State->MaxFirstVisible)
                        State->PaintAll = TRUE;
                    State->CurrentSelection += State->MaxVisible;          // move one page and scroll synchronously
                    State->FirstVisible += State->MaxVisible;
                }
                CONSTRAIN_MAX(State->CurrentSelection, State->MaxIndex);
                CONSTRAIN_MAX(State->FirstVisible, State->MaxFirstVisible);
            }
            break;
            
        case SCROLL_FIRST:
            if (State->CurrentSelection > 0) {
                State->CurrentSelection = 0;
                if (State->FirstVisible > 0) {
                    State->PaintAll = TRUE;
                    State->FirstVisible = 0;
                }
            }
            break;
            
        case SCROLL_LAST:
            if (State->CurrentSelection < State->MaxIndex) {
                State->CurrentSelection = State->MaxIndex;
                if (State->FirstVisible < State->MaxFirstVisible) {
                    State->PaintAll = TRUE;
                    State->FirstVisible = State->MaxFirstVisible;
                }
            }
            break;
            
    }
    
    if (!State->PaintAll && State->CurrentSelection != State->LastSelection)
        State->PaintSelection = TRUE;
    State->LastVisible = State->FirstVisible + State->MaxVisible;
}

static VOID UpdateScrollScancode(IN OUT SCROLL_STATE *State, IN UINT16 Scancode)
{
    switch (Scancode) {
        case 0x01:  // Arrow Up
        case 0x04:  // Arrow Left
            UpdateScroll(State, SCROLL_LINE_UP);
            break;
        case 0x02:  // Arrow Down
        case 0x03:  // Arrow Right
            UpdateScroll(State, SCROLL_LINE_DOWN);
            break;
        case 0x05:  // Home
            UpdateScroll(State, SCROLL_FIRST);
            break;
        case 0x06:  // End
            UpdateScroll(State, SCROLL_LAST);
            break;
        case 0x09:  // PageUp
            UpdateScroll(State, SCROLL_PAGE_UP);
            break;
        case 0x0a:  // PageDown
            UpdateScroll(State, SCROLL_PAGE_DOWN);
            break;
    }
}

//
// Menu functions
//

VOID AddMenuEntry(IN REFIT_MENU_SCREEN *Screen, IN REFIT_MENU_ENTRY *Entry)
{
    AddListElement(&(Screen->Entries), &(Screen->EntryCount), Entry);
}

VOID FreeMenu(IN REFIT_MENU_SCREEN *Screen)
{
    if (Screen->Entries)
        FreePool(Screen->Entries);
}

//
// text mode menu (used for all situations)
//

static UINTN RunMenuText(IN REFIT_MENU_SCREEN *Screen, OUT REFIT_MENU_ENTRY **ChosenEntry)
{
    SCROLL_STATE State;
    INTN i;
    UINTN menuWidth, itemWidth;
    EFI_STATUS Status;
    EFI_INPUT_KEY key;
    UINTN index;
    BOOLEAN HaveTimeout;
    UINTN TimeoutCountdown;
    CHAR16 *TimeoutMessage;
    UINTN MenuExit;
    CHAR16 **DisplayStrings;
    
    if (Screen->TimeoutSeconds > 0) {
        HaveTimeout = TRUE;
        TimeoutCountdown = Screen->TimeoutSeconds * 10;
    } else
        HaveTimeout = FALSE;
    
    InitScroll(&State, Screen->EntryCount, ConHeight - 4 - (HaveTimeout ? 2 : 0));
    
    MenuExit = 0;
    
    // setup screen
    BeginTextScreen(Screen->Title);
    
    // determine width of the menu
    menuWidth = 20;  // minimum
    for (i = 0; i <= State.MaxIndex; i++) {
        itemWidth = StrLen(Screen->Entries[i]->Title);
        if (menuWidth < itemWidth)
            menuWidth = itemWidth;
    }
    if (menuWidth > ConWidth - 6)
        menuWidth = ConWidth - 6;
    
    // prepare strings for display
    DisplayStrings = AllocatePool(sizeof(CHAR16 *) * Screen->EntryCount);
    for (i = 0; i <= State.MaxIndex; i++)
        DisplayStrings[i] = PoolPrint(L" %-.*s ", menuWidth, Screen->Entries[i]->Title);
    // TODO: shorten strings that are too long (PoolPrint doesn't do that...)
    // TODO: use more elaborate techniques for shortening too long strings (ellipses in the middle)
    // TODO: account for double-width characters
    
    while (!MenuExit) {
        // update the screen
        if (State.PaintAll) {
            // paint the whole screen (initially and after scrolling)
            for (i = 0; i <= State.MaxIndex; i++) {
                if (i >= State.FirstVisible && i <= State.LastVisible) {
                    ST->ConOut->SetCursorPosition(ST->ConOut, 2, 4 + (i - State.FirstVisible));
                    if (i == State.CurrentSelection)
                        ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_CURRENT);
                    else
                        ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_BASIC);
                    ST->ConOut->OutputString(ST->ConOut, DisplayStrings[i]);
                }
            }
            // scrolling indicators
            ST->ConOut->SetAttribute(ST->ConOut, ATTR_SCROLLARROW);
            ST->ConOut->SetCursorPosition(ST->ConOut, 0, 4);
            if (State.FirstVisible > 0)
                ST->ConOut->OutputString(ST->ConOut, ArrowUp);
            else
                ST->ConOut->OutputString(ST->ConOut, L" ");
            ST->ConOut->SetCursorPosition(ST->ConOut, 0, 4 + State.MaxVisible);
            if (State.LastVisible < State.MaxIndex)
                ST->ConOut->OutputString(ST->ConOut, ArrowDown);
            else
                ST->ConOut->OutputString(ST->ConOut, L" ");
            State.PaintAll = FALSE;
            
        } else if (State.PaintSelection) {
            // redraw selection cursor
            ST->ConOut->SetCursorPosition(ST->ConOut, 2, 4 + (State.LastSelection - State.FirstVisible));
            ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_BASIC);
            ST->ConOut->OutputString(ST->ConOut, DisplayStrings[State.LastSelection]);
            ST->ConOut->SetCursorPosition(ST->ConOut, 2, 4 + (State.CurrentSelection - State.FirstVisible));
            ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_CURRENT);
            ST->ConOut->OutputString(ST->ConOut, DisplayStrings[State.CurrentSelection]);
            State.PaintSelection = FALSE;
            
        }
        
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
                MenuExit = MENU_EXIT_TIMEOUT;
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
        
        UpdateScrollScancode(&State, key.ScanCode);
        
        if (key.ScanCode == 0x17)   // Escape
            MenuExit = MENU_EXIT_ESCAPE;
        if (key.UnicodeChar == 0x0a || key.UnicodeChar == 0x0d || key.UnicodeChar == 0x20)   // Enter, Space
            MenuExit = MENU_EXIT_ENTER;
        // TODO: function key for "details" exit
    }
    
    for (i = 0; i <= State.MaxIndex; i++)
        FreePool(DisplayStrings[i]);
    FreePool(DisplayStrings);
    
    if (ChosenEntry)
        *ChosenEntry = Screen->Entries[State.CurrentSelection];
    return MenuExit;
}

#ifndef TEXTONLY

//
// graphics mode menu, generic style
//

static VOID DrawMenuText(IN CHAR16 *Text, IN UINTN SelectedWidth, IN UINTN XPos, IN UINTN YPos)
{
    UINT8 *Ptr;
    UINTN i, x, y;
    
    if (TextBuffer.PixelData == NULL)
        TextBuffer.PixelData = AllocatePool(TextBuffer.Width * TextBuffer.Height * 4);
    
    // clear the buffer
    Ptr = TextBuffer.PixelData;
    for (i = 0; i < TextBuffer.Width * TextBuffer.Height; i++) {
        *Ptr++ = 0xbf;
        *Ptr++ = 0xbf;
        *Ptr++ = 0xbf;
        *Ptr++ = 0;
    }
    
    // draw selection background
    if (SelectedWidth > 0) {
        for (y = 0; y < TextBuffer.Height; y++) {
            Ptr = TextBuffer.PixelData + y * TextBuffer.Width * 4;
            for (x = 0; x < SelectedWidth; x++) {
                *Ptr++ = 0xff;
                *Ptr++ = 0xff;
                *Ptr++ = 0xff;
                *Ptr++ = 0;
            }
        }
    }
    
    // render the text
    RenderText(Text, &TextBuffer, 8, (TEXT_SPACING - FONT_CELL_HEIGHT) >> 1);
    BltImage(&TextBuffer, XPos, YPos);
}

static UINTN RunMenuGraphics(IN REFIT_MENU_SCREEN *Screen, OUT REFIT_MENU_ENTRY **ChosenEntry)
{
    UINTN MenuWidth, ItemWidth;
    UINTN EntriesPosX, EntriesPosY, TimeoutPosY;
    SCROLL_STATE State;
    INTN i;
    EFI_STATUS Status;
    EFI_INPUT_KEY key;
    UINTN index;
    BOOLEAN HaveTimeout;
    UINTN TimeoutCountdown;
    CHAR16 *TimeoutMessage;
    UINTN MenuExit;
    
    if (Screen->TimeoutSeconds > 0) {
        HaveTimeout = TRUE;
        TimeoutCountdown = Screen->TimeoutSeconds * 10;
    } else
        HaveTimeout = FALSE;
    
    InitScroll(&State, Screen->EntryCount, 0);    // TODO: calculate available screen space
    
    MenuExit = 0;
    
    // determine width of the menu
    MenuWidth = 20;  // minimum
    for (i = 0; i <= State.MaxIndex; i++) {
        ItemWidth = StrLen(Screen->Entries[i]->Title);
        if (MenuWidth < ItemWidth)
            MenuWidth = ItemWidth;
    }
    MenuWidth = 2*8 + MenuWidth * FONT_CELL_WIDTH;
    if (MenuWidth > 512)
        MenuWidth = 512;
    
    if (Screen->TitleImage)
        EntriesPosX = (UGAWidth + (Screen->TitleImage->Width + 32) - MenuWidth) >> 1;
    else
        EntriesPosX = (UGAWidth - MenuWidth) >> 1;
    EntriesPosY = ((UGAHeight - LAYOUT_TOTAL_HEIGHT) >> 1) + 32 + 32;
    TimeoutPosY = EntriesPosY + (Screen->EntryCount + 1) * TEXT_SPACING;
    
    // initial painting
    SwitchToGraphicsAndClear();
    if (Screen->TitleImage)
        BltImageAlpha(Screen->TitleImage, EntriesPosX - (Screen->TitleImage->Width + 32), EntriesPosY);
    for (i = 0; i <= State.MaxIndex; i++) {
        DrawMenuText(Screen->Entries[i]->Title, (i == State.CurrentSelection) ? MenuWidth : 0,
                     EntriesPosX, EntriesPosY + i * TEXT_SPACING);
    }
    // TODO: account for scrolling
    State.PaintAll = FALSE;
    State.PaintSelection = FALSE;
    
    while (!MenuExit) {
        
        if (State.PaintSelection) {
            DrawMenuText(Screen->Entries[State.LastSelection]->Title, 0,
                         EntriesPosX, EntriesPosY + State.LastSelection * TEXT_SPACING);
            DrawMenuText(Screen->Entries[State.CurrentSelection]->Title, MenuWidth,
                         EntriesPosX, EntriesPosY + State.CurrentSelection * TEXT_SPACING);
            State.PaintSelection = FALSE;
        }
        // TODO: account for scrolling
        
        if (HaveTimeout) {
            TimeoutMessage = PoolPrint(L"%s in %d seconds", Screen->TimeoutText, (TimeoutCountdown + 5) / 10);
            DrawMenuText(TimeoutMessage, 0, EntriesPosX, TimeoutPosY);
            FreePool(TimeoutMessage);
        }
        
        Status = ST->ConIn->ReadKeyStroke(ST->ConIn, &key);
        if (Status == EFI_NOT_READY) {
            if (HaveTimeout && TimeoutCountdown == 0) {
                // timeout expired
                MenuExit = MENU_EXIT_TIMEOUT;
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
            DrawMenuText(L"", 0, EntriesPosX, TimeoutPosY);
            HaveTimeout = FALSE;
        }
        
        UpdateScrollScancode(&State, key.ScanCode);
        
        if (key.ScanCode == 0x17)   // Escape
            MenuExit = MENU_EXIT_ESCAPE;
        if (key.UnicodeChar == 0x0a || key.UnicodeChar == 0x0d || key.UnicodeChar == 0x20)   // Enter, Space
            MenuExit = MENU_EXIT_ENTER;
        // TODO: function key for "details" exit
    }
    
    if (ChosenEntry)
        *ChosenEntry = Screen->Entries[State.CurrentSelection];
    return MenuExit;
}

//
// graphics mode menu, main menu style
//

static VOID DrawMainMenuEntry(REFIT_MENU_ENTRY *Entry, BOOLEAN selected, UINTN XPos, UINTN YPos)
{
    REFIT_IMAGE *BackgroundImage;
    
    if (Entry->Row == 0) {
        if (selected)
            BackgroundImage = BuiltinImage(3);  // image_back_selected_big
        else
            BackgroundImage = BuiltinImage(2);  // image_back_normal_big
    } else {
        if (selected)
            BackgroundImage = BuiltinImage(5);  // image_back_selected_small
        else
            BackgroundImage = BuiltinImage(4);  // image_back_normal_small
    }
    BltImageCompositeBadge(BackgroundImage, Entry->Image, Entry->BadgeImage, XPos, YPos);
}

static VOID DrawMainMenuText(IN CHAR16 *Text, IN UINTN XPos, IN UINTN YPos)
{
    UINT8 *Ptr;
    UINTN TextWidth, i;
    
    if (TextBuffer.PixelData == NULL)
        TextBuffer.PixelData = AllocatePool(TextBuffer.Width * TextBuffer.Height * 4);
    
    // clear the buffer
    Ptr = TextBuffer.PixelData;
    for (i = 0; i < TextBuffer.Width * TextBuffer.Height; i++) {
        *Ptr++ = 0xbf;
        *Ptr++ = 0xbf;
        *Ptr++ = 0xbf;
        *Ptr++ = 0;
    }
    
    // render the text
    MeasureText(Text, &TextWidth, NULL);
    RenderText(Text, &TextBuffer, (TextBuffer.Width - TextWidth) >> 1, 0);
    BltImage(&TextBuffer, XPos, YPos);
}

static UINTN RunMainMenuGraphics(IN REFIT_MENU_SCREEN *Screen, OUT REFIT_MENU_ENTRY **ChosenEntry)
{
    UINTN row0Count, row0PosX, row0PosY, row0PosXRunning;
    UINTN row1Count, row1PosX, row1PosY, row1PosXRunning;
    UINTN *itemPosX;
    UINTN textPosY;
    SCROLL_STATE State;
    INTN i;
    EFI_STATUS Status;
    EFI_INPUT_KEY key;
    UINTN index;
    BOOLEAN HaveTimeout;
    UINTN TimeoutCountdown;
    CHAR16 *TimeoutMessage;
    UINTN MenuExit;
    
    if (Screen->TimeoutSeconds > 0) {
        HaveTimeout = TRUE;
        TimeoutCountdown = Screen->TimeoutSeconds * 10;
    } else
        HaveTimeout = FALSE;
    
    InitScroll(&State, Screen->EntryCount, 0);
    
    MenuExit = 0;
    
    row0Count = 0;
    row1Count = 0;
    for (i = 0; i <= State.MaxIndex; i++) {
        if (Screen->Entries[i]->Row == 0)
            row0Count++;
        else
            row1Count++;
    }
    row0PosX = (UGAWidth + 8 - (144 + 8) * row0Count) >> 1;
    row0PosY = ((UGAHeight - LAYOUT_TOTAL_HEIGHT) >> 1) + 32 + 32;
    row1PosX = (UGAWidth + 8 - (64 + 8) * row1Count) >> 1;
    row1PosY = row0PosY + 144 + 16;
    textPosY = row1PosY + 64 + 16;
    
    itemPosX = AllocatePool(sizeof(UINTN) * Screen->EntryCount);
    row0PosXRunning = row0PosX;
    row1PosXRunning = row1PosX;
    for (i = 0; i <= State.MaxIndex; i++) {
        if (Screen->Entries[i]->Row == 0) {
            itemPosX[i] = row0PosXRunning;
            row0PosXRunning += 144 + 8;
        } else {
            itemPosX[i] = row1PosXRunning;
            row1PosXRunning += 64 + 8;
        }
    }
    
    // initial painting
    SwitchToGraphicsAndClear();
    for (i = 0; i <= State.MaxIndex; i++) {
        DrawMainMenuEntry(Screen->Entries[i], (i == State.CurrentSelection) ? TRUE : FALSE,
                          itemPosX[i],
                          (Screen->Entries[i]->Row == 0) ? row0PosY : row1PosY);
    }
    DrawMainMenuText(Screen->Entries[State.CurrentSelection]->Title,
                     (UGAWidth - TextBuffer.Width) >> 1, textPosY);
    State.PaintAll = FALSE;
    State.PaintSelection = FALSE;
    
    while (!MenuExit) {
        
        if (State.PaintSelection) {
            DrawMainMenuEntry(Screen->Entries[State.LastSelection], FALSE,
                              itemPosX[State.LastSelection],
                              (Screen->Entries[State.LastSelection]->Row == 0) ? row0PosY : row1PosY);
            DrawMainMenuEntry(Screen->Entries[State.CurrentSelection], TRUE,
                              itemPosX[State.CurrentSelection],
                              (Screen->Entries[State.CurrentSelection]->Row == 0) ? row0PosY : row1PosY);
            DrawMainMenuText(Screen->Entries[State.CurrentSelection]->Title,
                             (UGAWidth - TextBuffer.Width) >> 1, textPosY);
            State.PaintSelection = FALSE;
        }
        
        if (HaveTimeout) {
            TimeoutMessage = PoolPrint(L"%s in %d seconds", Screen->TimeoutText, (TimeoutCountdown + 5) / 10);
            DrawMainMenuText(TimeoutMessage,
                             (UGAWidth - TextBuffer.Width) >> 1, textPosY + TEXT_SPACING);
            FreePool(TimeoutMessage);
        }
        
        Status = ST->ConIn->ReadKeyStroke(ST->ConIn, &key);
        if (Status == EFI_NOT_READY) {
            if (HaveTimeout && TimeoutCountdown == 0) {
                // timeout expired
                MenuExit = MENU_EXIT_TIMEOUT;
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
            DrawMainMenuText(L"",
                             (UGAWidth - TextBuffer.Width) >> 1, textPosY + TEXT_SPACING);
            HaveTimeout = FALSE;
        }
        
        UpdateScrollScancode(&State, key.ScanCode);
        
        if (key.ScanCode == 0x17)   // Escape
            MenuExit = MENU_EXIT_ESCAPE;
        if (key.UnicodeChar == 0x0a || key.UnicodeChar == 0x0d || key.UnicodeChar == 0x20)   // Enter, Space
            MenuExit = MENU_EXIT_ENTER;
        // TODO: function key for "details" exit
    }
    
    FreePool(itemPosX);
    
    if (ChosenEntry)
        *ChosenEntry = Screen->Entries[State.CurrentSelection];
    return MenuExit;
}

#endif  /* !TEXTONLY */

UINTN RunMenu(IN REFIT_MENU_SCREEN *Screen, OUT REFIT_MENU_ENTRY **ChosenEntry)
{
#ifndef TEXTONLY
    if (AllowGraphicsMode)
        return RunMenuGraphics(Screen, ChosenEntry);
    else
#endif  /* !TEXTONLY */
        return RunMenuText(Screen, ChosenEntry);
}

UINTN RunMainMenu(IN REFIT_MENU_SCREEN *Screen, OUT REFIT_MENU_ENTRY **ChosenEntry)
{
#ifndef TEXTONLY
    if (AllowGraphicsMode)
        return RunMainMenuGraphics(Screen, ChosenEntry);
    else
#endif  /* !TEXTONLY */
        return RunMenuText(Screen, ChosenEntry);
}
