/*
 * refit/image.c
 * Handling of embedded images
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

#ifndef TEXTONLY

#include "eei.h"

//
// image data
//

#include "eei_font.h"
#include "eei_refit_banner.h"

#include "eei_back_normal_big.h"
#include "eei_back_normal_small.h"
#include "eei_back_selected_big.h"
#include "eei_back_selected_small.h"

//
// table of images
//

typedef struct {
    EEI_IMAGE *EEI;
    REFIT_IMAGE Image;
} BUILTIN_IMAGE;

#define EMPTY_IMAGE { NULL, 0, 0 }

BUILTIN_IMAGE BuiltinImageTable[] = {
    { &eei_font, EMPTY_IMAGE },
    { &eei_refit_banner, EMPTY_IMAGE },
    { &eei_back_normal_big, EMPTY_IMAGE },
    { &eei_back_selected_big, EMPTY_IMAGE },
    { &eei_back_normal_small, EMPTY_IMAGE },
    { &eei_back_selected_small, EMPTY_IMAGE },
};
#define BUILTIN_IMAGE_COUNT (6)

//
// image retrieval
//

REFIT_IMAGE * BuiltinImage(IN UINTN Id)
{
    if (Id >= BUILTIN_IMAGE_COUNT)
        return NULL;
    
    if (BuiltinImageTable[Id].Image.PixelData == NULL) {
        
        EEIPrepareImage(BuiltinImageTable[Id].EEI);
        
        BuiltinImageTable[Id].Image.PixelData = (UINT8 *)(BuiltinImageTable[Id].EEI->PixelData);
        BuiltinImageTable[Id].Image.Width = BuiltinImageTable[Id].EEI->Width;
        BuiltinImageTable[Id].Image.Height = BuiltinImageTable[Id].EEI->Height;
        
    }
    
    return &(BuiltinImageTable[Id].Image);
}

#else   /* !TEXTONLY */

REFIT_IMAGE * BuiltinImage(IN UINTN Id)
{
    return NULL;
}

#endif  /* !TEXTONLY */
