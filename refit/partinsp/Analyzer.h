/*
 * partinsp/Analyzer.h
 * Partition Inspector UI controller class
 *
 * Copyright (c) 2006-2007 Christoph Pfisterer
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

@interface Analyzer : NSObject
{
    @private
    NSString *report;
    NSAttributedString *attrReport;
    NSMutableDictionary *attr;
    
    BOOL inPhase1;
    BOOL inPhase2;
    BOOL inPhase3;
}

// init

- (id)init;

// accessors

- (NSString *)report;
- (void)setReport:(NSString *)newReport;

- (NSAttributedString *)attrReport;
- (void)setAttrReport:(NSAttributedString *)newReport;

- (BOOL)isInPhase1;
- (void)setInPhase1:(BOOL)newFlag;
- (BOOL)isInPhase2;
- (void)setInPhase2:(BOOL)newFlag;
- (BOOL)isInPhase3;
- (void)setInPhase3:(BOOL)newFlag;

// actions

- (IBAction)startAnalysis:(id)sender;

- (IBAction)visitHomePage:(id)sender;
- (IBAction)visitHelpPage:(id)sender;

// analyzing

- (void)analyze;
- (void)reportCallback:(NSString *)report;

// delegates

- (BOOL)windowShouldClose:(id)sender;

@end
