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

#define MENU_FUNCTION_INIT            (0)
#define MENU_FUNCTION_CLEANUP         (1)
#define MENU_FUNCTION_PAINT_ALL       (2)
#define MENU_FUNCTION_PAINT_SELECTION (3)
#define MENU_FUNCTION_PAINT_TIMEOUT   (4)

typedef VOID (*MENU_STYLE_FUNC)(IN REFIT_MENU_SCREEN *Screen, IN SCROLL_STATE *State, IN UINTN Function, IN CHAR16 *ParamText);

static CHAR16 ArrowUp[2] = { ARROW_UP, 0 };
static CHAR16 ArrowDown[2] = { ARROW_DOWN, 0 };

#ifndef TEXTONLY

#define TEXT_YMARGIN (2)
#define TEXT_XMARGIN (8)
#define TEXT_LINE_HEIGHT (FONT_CELL_HEIGHT + TEXT_YMARGIN * 2)
#define TITLEICON_SPACING (16)

#define ROW0_TILESIZE (144)
#define ROW1_TILESIZE (64)
#define TILE_XSPACING (8)
#define TILE_YSPACING (16)

static REFIT_IMAGE TextBuffer = { NULL, LAYOUT_TEXT_WIDTH, TEXT_LINE_HEIGHT };

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

//
// menu helper functions
//

VOID AddMenuInfoLine(IN REFIT_MENU_SCREEN *Screen, IN CHAR16 *InfoLine)
{
    AddListElement(&(Screen->InfoLines), &(Screen->InfoLineCount), InfoLine);
}

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
// generic menu function
//

static UINTN RunGenericMenu(IN REFIT_MENU_SCREEN *Screen, IN MENU_STYLE_FUNC StyleFunc, OUT REFIT_MENU_ENTRY **ChosenEntry)
{
    SCROLL_STATE State;
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
    MenuExit = 0;
    
    StyleFunc(Screen, &State, MENU_FUNCTION_INIT, NULL);
    
    while (!MenuExit) {
        // update the screen
        if (State.PaintAll) {
            StyleFunc(Screen, &State, MENU_FUNCTION_PAINT_ALL, NULL);
            State.PaintAll = FALSE;
        } else if (State.PaintSelection) {
            StyleFunc(Screen, &State, MENU_FUNCTION_PAINT_SELECTION, NULL);
            State.PaintSelection = FALSE;
        }
        
        if (HaveTimeout) {
            TimeoutMessage = PoolPrint(L"%s in %d seconds", Screen->TimeoutText, (TimeoutCountdown + 5) / 10);
            StyleFunc(Screen, &State, MENU_FUNCTION_PAINT_TIMEOUT, TimeoutMessage);
            FreePool(TimeoutMessage);
        }
        
        // read key press (and wait for it if applicable)
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
            StyleFunc(Screen, &State, MENU_FUNCTION_PAINT_TIMEOUT, L"");
            HaveTimeout = FALSE;
        }
        
        // react to key press
        switch (key.ScanCode) {
            case SCAN_UP:
            case SCAN_LEFT:
                UpdateScroll(&State, SCROLL_LINE_UP);
                break;
            case SCAN_DOWN:
            case SCAN_RIGHT:
                UpdateScroll(&State, SCROLL_LINE_DOWN);
                break;
            case SCAN_HOME:
                UpdateScroll(&State, SCROLL_FIRST);
                break;
            case SCAN_END:
                UpdateScroll(&State, SCROLL_LAST);
                break;
            case SCAN_PAGE_UP:
                UpdateScroll(&State, SCROLL_PAGE_UP);
                break;
            case SCAN_PAGE_DOWN:
                UpdateScroll(&State, SCROLL_PAGE_DOWN);
                break;
            case SCAN_ESC:
                MenuExit = MENU_EXIT_ESCAPE;
                break;
            case SCAN_INSERT:
            case SCAN_F2:
                MenuExit = MENU_EXIT_DETAILS;
                break;
        }
        switch (key.UnicodeChar) {
            case CHAR_LINEFEED:
            case CHAR_CARRIAGE_RETURN:
            case ' ':
                MenuExit = MENU_EXIT_ENTER;
                break;
            case '+':
                MenuExit = MENU_EXIT_DETAILS;
                break;
        }
    }
    
    StyleFunc(Screen, &State, MENU_FUNCTION_CLEANUP, NULL);
    
    if (ChosenEntry)
        *ChosenEntry = Screen->Entries[State.CurrentSelection];
    return MenuExit;
}

//
// text-mode generic style
//

static VOID TextMenuStyle(IN REFIT_MENU_SCREEN *Screen, IN SCROLL_STATE *State, IN UINTN Function, IN CHAR16 *ParamText)
{
    INTN i;
    UINTN MenuWidth, ItemWidth, MenuHeight;
    static MenuPosY;
    static CHAR16 **DisplayStrings;
    CHAR16 *TimeoutMessage;
    
    switch (Function) {
        
        case MENU_FUNCTION_INIT:
            // vertical layout
            MenuPosY = 4;
            if (Screen->InfoLineCount > 0)
                MenuPosY += Screen->InfoLineCount + 1;
            MenuHeight = ConHeight - MenuPosY;
            if (Screen->TimeoutSeconds > 0)
                MenuHeight -= 2;
            InitScroll(State, Screen->EntryCount, MenuHeight);
            
            // determine width of the menu
            MenuWidth = 20;  // minimum
            for (i = 0; i <= State->MaxIndex; i++) {
                ItemWidth = StrLen(Screen->Entries[i]->Title);
                if (MenuWidth < ItemWidth)
                    MenuWidth = ItemWidth;
            }
            if (MenuWidth > ConWidth - 6)
                MenuWidth = ConWidth - 6;
            
            // prepare strings for display
            DisplayStrings = AllocatePool(sizeof(CHAR16 *) * Screen->EntryCount);
            for (i = 0; i <= State->MaxIndex; i++)
                DisplayStrings[i] = PoolPrint(L" %-.*s ", MenuWidth, Screen->Entries[i]->Title);
            // TODO: shorten strings that are too long (PoolPrint doesn't do that...)
            // TODO: use more elaborate techniques for shortening too long strings (ellipses in the middle)
            // TODO: account for double-width characters
                
            // initial painting
            BeginTextScreen(Screen->Title);
            if (Screen->InfoLineCount > 0) {
                ST->ConOut->SetAttribute(ST->ConOut, ATTR_BASIC);
                for (i = 0; i < (INTN)Screen->InfoLineCount; i++) {
                    ST->ConOut->SetCursorPosition(ST->ConOut, 3, 4 + i);
                    ST->ConOut->OutputString(ST->ConOut, Screen->InfoLines[i]);
                }
            }
            
            break;
            
        case MENU_FUNCTION_CLEANUP:
            // release temporary memory
            for (i = 0; i <= State->MaxIndex; i++)
                FreePool(DisplayStrings[i]);
            FreePool(DisplayStrings);
            break;
            
        case MENU_FUNCTION_PAINT_ALL:
            // paint the whole screen (initially and after scrolling)
            for (i = 0; i <= State->MaxIndex; i++) {
                if (i >= State->FirstVisible && i <= State->LastVisible) {
                    ST->ConOut->SetCursorPosition(ST->ConOut, 2, MenuPosY + (i - State->FirstVisible));
                    if (i == State->CurrentSelection)
                        ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_CURRENT);
                    else
                        ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_BASIC);
                    ST->ConOut->OutputString(ST->ConOut, DisplayStrings[i]);
                }
            }
            // scrolling indicators
            ST->ConOut->SetAttribute(ST->ConOut, ATTR_SCROLLARROW);
            ST->ConOut->SetCursorPosition(ST->ConOut, 0, MenuPosY);
            if (State->FirstVisible > 0)
                ST->ConOut->OutputString(ST->ConOut, ArrowUp);
            else
                ST->ConOut->OutputString(ST->ConOut, L" ");
            ST->ConOut->SetCursorPosition(ST->ConOut, 0, MenuPosY + State->MaxVisible);
            if (State->LastVisible < State->MaxIndex)
                ST->ConOut->OutputString(ST->ConOut, ArrowDown);
            else
                ST->ConOut->OutputString(ST->ConOut, L" ");
            break;
            
        case MENU_FUNCTION_PAINT_SELECTION:
            // redraw selection cursor
            ST->ConOut->SetCursorPosition(ST->ConOut, 2, MenuPosY + (State->LastSelection - State->FirstVisible));
            ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_BASIC);
            ST->ConOut->OutputString(ST->ConOut, DisplayStrings[State->LastSelection]);
            ST->ConOut->SetCursorPosition(ST->ConOut, 2, MenuPosY + (State->CurrentSelection - State->FirstVisible));
            ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_CURRENT);
            ST->ConOut->OutputString(ST->ConOut, DisplayStrings[State->CurrentSelection]);
            break;
            
        case MENU_FUNCTION_PAINT_TIMEOUT:
            if (ParamText[0] == 0) {
                // clear message
                ST->ConOut->SetAttribute(ST->ConOut, ATTR_BASIC);
                ST->ConOut->SetCursorPosition(ST->ConOut, 0, ConHeight - 1);
                ST->ConOut->OutputString(ST->ConOut, BlankLine + 1);
            } else {
                // paint or update message
                ST->ConOut->SetAttribute(ST->ConOut, ATTR_ERROR);
                ST->ConOut->SetCursorPosition(ST->ConOut, 3, ConHeight - 1);
                TimeoutMessage = PoolPrint(L"%s  ", ParamText);
                ST->ConOut->OutputString(ST->ConOut, TimeoutMessage);
                FreePool(TimeoutMessage);
            }
            break;
            
    }
}

#ifndef TEXTONLY

//
// graphical generic style
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
    RenderText(Text, &TextBuffer, TEXT_XMARGIN, TEXT_YMARGIN);
    BltImage(&TextBuffer, XPos, YPos);
}

static VOID GraphicsMenuStyle(IN REFIT_MENU_SCREEN *Screen, IN SCROLL_STATE *State, IN UINTN Function, IN CHAR16 *ParamText)
{
    INTN i;
    UINTN ItemWidth;
    static UINTN MenuWidth, EntriesPosX, EntriesPosY, TimeoutPosY;
    
    switch (Function) {
        
        case MENU_FUNCTION_INIT:
            InitScroll(State, Screen->EntryCount, 0);    // TODO: calculate available screen space
            
            // determine width of the menu
            MenuWidth = 20;  // minimum
            for (i = 0; i < (INTN)Screen->InfoLineCount; i++) {
                ItemWidth = StrLen(Screen->InfoLines[i]);
                if (MenuWidth < ItemWidth)
                    MenuWidth = ItemWidth;
            }
            for (i = 0; i <= State->MaxIndex; i++) {
                ItemWidth = StrLen(Screen->Entries[i]->Title);
                if (MenuWidth < ItemWidth)
                    MenuWidth = ItemWidth;
            }
            MenuWidth = TEXT_XMARGIN * 2 + MenuWidth * FONT_CELL_WIDTH;
            if (MenuWidth > LAYOUT_TEXT_WIDTH)
                MenuWidth = LAYOUT_TEXT_WIDTH;
            
            if (Screen->TitleImage)
                EntriesPosX = (UGAWidth + (Screen->TitleImage->Width + TITLEICON_SPACING) - MenuWidth) >> 1;
            else
                EntriesPosX = (UGAWidth - MenuWidth) >> 1;
            EntriesPosY = ((UGAHeight - LAYOUT_TOTAL_HEIGHT) >> 1) + LAYOUT_BANNER_YOFFSET + TEXT_LINE_HEIGHT * 2;
            TimeoutPosY = EntriesPosY + (Screen->EntryCount + 1) * TEXT_LINE_HEIGHT;
            
            // initial painting
            SwitchToGraphicsAndClear();
            MeasureText(Screen->Title, &ItemWidth, NULL);
            DrawMenuText(Screen->Title, 0, ((UGAWidth - ItemWidth) >> 1) - TEXT_XMARGIN, EntriesPosY - TEXT_LINE_HEIGHT * 2);
            if (Screen->TitleImage)
                BltImageAlpha(Screen->TitleImage, EntriesPosX - (Screen->TitleImage->Width + TITLEICON_SPACING), EntriesPosY);
            if (Screen->InfoLineCount > 0) {
                for (i = 0; i < (INTN)Screen->InfoLineCount; i++) {
                    DrawMenuText(Screen->InfoLines[i], 0, EntriesPosX, EntriesPosY);
                    EntriesPosY += TEXT_LINE_HEIGHT;
                }
                EntriesPosY += TEXT_LINE_HEIGHT;  // also add a blank line
            }
            
            break;
            
        case MENU_FUNCTION_CLEANUP:
            // nothing to do
            break;
            
        case MENU_FUNCTION_PAINT_ALL:
            for (i = 0; i <= State->MaxIndex; i++) {
                DrawMenuText(Screen->Entries[i]->Title, (i == State->CurrentSelection) ? MenuWidth : 0,
                             EntriesPosX, EntriesPosY + i * TEXT_LINE_HEIGHT);
            }
            // TODO: account for scrolling
            /*
            for (i = 0; i <= State->MaxIndex; i++) {
                if (i >= State->FirstVisible && i <= State->LastVisible) {
                    ST->ConOut->SetCursorPosition(ST->ConOut, 2, 4 + (i - State->FirstVisible));
                    if (i == State->CurrentSelection)
                        ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_CURRENT);
                    else
                        ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_BASIC);
                    ST->ConOut->OutputString(ST->ConOut, DisplayStrings[i]);
                }
            }
            // scrolling indicators
            ST->ConOut->SetAttribute(ST->ConOut, ATTR_SCROLLARROW);
            ST->ConOut->SetCursorPosition(ST->ConOut, 0, 4);
            if (State->FirstVisible > 0)
                ST->ConOut->OutputString(ST->ConOut, ArrowUp);
            else
                ST->ConOut->OutputString(ST->ConOut, L" ");
            ST->ConOut->SetCursorPosition(ST->ConOut, 0, 4 + State->MaxVisible);
            if (State->LastVisible < State->MaxIndex)
                ST->ConOut->OutputString(ST->ConOut, ArrowDown);
            else
                ST->ConOut->OutputString(ST->ConOut, L" ");
             */
            break;
            
        case MENU_FUNCTION_PAINT_SELECTION:
            // redraw selection cursor
            DrawMenuText(Screen->Entries[State->LastSelection]->Title, 0,
                         EntriesPosX, EntriesPosY + State->LastSelection * TEXT_LINE_HEIGHT);
            DrawMenuText(Screen->Entries[State->CurrentSelection]->Title, MenuWidth,
                         EntriesPosX, EntriesPosY + State->CurrentSelection * TEXT_LINE_HEIGHT);
            // TODO: account for scrolling
            /*
            ST->ConOut->SetCursorPosition(ST->ConOut, 2, 4 + (State->LastSelection - State->FirstVisible));
            ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_BASIC);
            ST->ConOut->OutputString(ST->ConOut, DisplayStrings[State->LastSelection]);
            ST->ConOut->SetCursorPosition(ST->ConOut, 2, 4 + (State->CurrentSelection - State->FirstVisible));
            ST->ConOut->SetAttribute(ST->ConOut, ATTR_CHOICE_CURRENT);
            ST->ConOut->OutputString(ST->ConOut, DisplayStrings[State->CurrentSelection]);
             */
            break;
            
        case MENU_FUNCTION_PAINT_TIMEOUT:
            DrawMenuText(ParamText, 0, EntriesPosX, TimeoutPosY);
            break;
            
    }
}

//
// graphical main menu style
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

static VOID MainMenuStyle(IN REFIT_MENU_SCREEN *Screen, IN SCROLL_STATE *State, IN UINTN Function, IN CHAR16 *ParamText)
{
    INTN i;
    UINTN row0Count, row0PosX, row0PosXRunning;
    UINTN row1Count, row1PosX, row1PosXRunning;
    static UINTN *itemPosX;
    static UINTN row0PosY, row1PosY, textPosY;
    
    switch (Function) {
        
        case MENU_FUNCTION_INIT:
            InitScroll(State, Screen->EntryCount, 0);
            
            // layout
            row0Count = 0;
            row1Count = 0;
            for (i = 0; i <= State->MaxIndex; i++) {
                if (Screen->Entries[i]->Row == 0)
                    row0Count++;
                else
                    row1Count++;
            }
            row0PosX = (UGAWidth + TILE_XSPACING - (ROW0_TILESIZE + TILE_XSPACING) * row0Count) >> 1;
            row0PosY = ((UGAHeight - LAYOUT_TOTAL_HEIGHT) >> 1) + LAYOUT_BANNER_YOFFSET;
            row1PosX = (UGAWidth + TILE_XSPACING - (ROW1_TILESIZE + TILE_XSPACING) * row1Count) >> 1;
            row1PosY = row0PosY + ROW0_TILESIZE + TILE_YSPACING;
            textPosY = row1PosY + ROW1_TILESIZE + TILE_YSPACING;
            
            itemPosX = AllocatePool(sizeof(UINTN) * Screen->EntryCount);
            row0PosXRunning = row0PosX;
            row1PosXRunning = row1PosX;
            for (i = 0; i <= State->MaxIndex; i++) {
                if (Screen->Entries[i]->Row == 0) {
                    itemPosX[i] = row0PosXRunning;
                    row0PosXRunning += ROW0_TILESIZE + TILE_XSPACING;
                } else {
                    itemPosX[i] = row1PosXRunning;
                    row1PosXRunning += ROW1_TILESIZE + TILE_XSPACING;
                }
            }
            
            // initial painting
            SwitchToGraphicsAndClear();
            break;
            
        case MENU_FUNCTION_CLEANUP:
            FreePool(itemPosX);
            break;
            
        case MENU_FUNCTION_PAINT_ALL:
            for (i = 0; i <= State->MaxIndex; i++) {
                DrawMainMenuEntry(Screen->Entries[i], (i == State->CurrentSelection) ? TRUE : FALSE,
                                  itemPosX[i],
                                  (Screen->Entries[i]->Row == 0) ? row0PosY : row1PosY);
            }
            DrawMainMenuText(Screen->Entries[State->CurrentSelection]->Title,
                             (UGAWidth - TextBuffer.Width) >> 1, textPosY);
            break;
            
        case MENU_FUNCTION_PAINT_SELECTION:
            DrawMainMenuEntry(Screen->Entries[State->LastSelection], FALSE,
                              itemPosX[State->LastSelection],
                              (Screen->Entries[State->LastSelection]->Row == 0) ? row0PosY : row1PosY);
            DrawMainMenuEntry(Screen->Entries[State->CurrentSelection], TRUE,
                              itemPosX[State->CurrentSelection],
                              (Screen->Entries[State->CurrentSelection]->Row == 0) ? row0PosY : row1PosY);
            DrawMainMenuText(Screen->Entries[State->CurrentSelection]->Title,
                             (UGAWidth - TextBuffer.Width) >> 1, textPosY);
            break;
            
        case MENU_FUNCTION_PAINT_TIMEOUT:
            DrawMainMenuText(ParamText, (UGAWidth - TextBuffer.Width) >> 1, textPosY + TEXT_LINE_HEIGHT);
            break;
            
    }
}

#endif  /* !TEXTONLY */

//
// user-callable dispatcher functions
//

UINTN RunMenu(IN REFIT_MENU_SCREEN *Screen, OUT REFIT_MENU_ENTRY **ChosenEntry)
{
    MENU_STYLE_FUNC Style = TextMenuStyle;
    
#ifndef TEXTONLY
    if (AllowGraphicsMode)
        Style = GraphicsMenuStyle;
#endif  /* !TEXTONLY */
    
    return RunGenericMenu(Screen, Style, ChosenEntry);
}

UINTN RunMainMenu(IN REFIT_MENU_SCREEN *Screen, OUT REFIT_MENU_ENTRY **ChosenEntry)
{
    MENU_STYLE_FUNC Style = TextMenuStyle;
    MENU_STYLE_FUNC MainStyle = TextMenuStyle;
    REFIT_MENU_ENTRY *TempChosenEntry;
    UINTN MenuExit = 0;
    
#ifndef TEXTONLY
    if (AllowGraphicsMode) {
        Style = GraphicsMenuStyle;
        MainStyle = MainMenuStyle;
    }
#endif  /* !TEXTONLY */
    
    while (!MenuExit) {
        MenuExit = RunGenericMenu(Screen, MainStyle, &TempChosenEntry);
        Screen->TimeoutSeconds = 0;
        
        if (MenuExit == MENU_EXIT_DETAILS && TempChosenEntry->SubScreen != NULL) {
            MenuExit = RunGenericMenu(TempChosenEntry->SubScreen, Style, &TempChosenEntry);
            if (MenuExit == MENU_EXIT_ESCAPE || TempChosenEntry->Tag == TAG_RETURN)
                MenuExit = 0;
        }
    }
    
    if (ChosenEntry)
        *ChosenEntry = TempChosenEntry;
    return MenuExit;
}
