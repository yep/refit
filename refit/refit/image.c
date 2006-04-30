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

//
// image data
//

#include "egemb_refit_banner.h"

#include "egemb_back_normal_big.h"
#include "egemb_back_normal_small.h"
#include "egemb_back_selected_big.h"
#include "egemb_back_selected_small.h"

//
// table of images
//

typedef struct {
    EG_EMBEDDED_IMAGE *EmbImage;
    EG_IMAGE *Image;
} BUILTIN_IMAGE;

BUILTIN_IMAGE BuiltinImageTable[] = {
    { NULL, NULL },
    { &egemb_refit_banner, NULL },
    { &egemb_back_normal_big, NULL },
    { &egemb_back_selected_big, NULL },
    { &egemb_back_normal_small, NULL },
    { &egemb_back_selected_small, NULL },
};
#define BUILTIN_IMAGE_COUNT (6)

//
// image retrieval
//

EG_IMAGE * BuiltinImage(IN UINTN Id)
{
    if (Id >= BUILTIN_IMAGE_COUNT)
        return NULL;
    
    if (BuiltinImageTable[Id].Image == NULL)
        BuiltinImageTable[Id].Image = egPrepareEmbeddedImage(BuiltinImageTable[Id].EmbImage, FALSE);
    
    return BuiltinImageTable[Id].Image;
}

#else   /* !TEXTONLY */

EG_IMAGE * BuiltinImage(IN UINTN Id)
{
    return NULL;
}

#endif  /* !TEXTONLY */
