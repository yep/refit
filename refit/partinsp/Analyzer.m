/*
 * partinsp/Analyzer.m
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

#import "Analyzer.h"
#import "ToolRunner.h"


@implementation Analyzer

// init

- (id)init
{
    self = [super init];
    if (self) {
        
        report = nil;
        attrReport = nil;
        attr = nil;
        
        inPhase1 = YES;
        inPhase2 = NO;
        inPhase3 = NO;
        
    }
    return self;
}

- (void)dealloc
{
    [report release];
    [attrReport release];
    [attr release];
    
    [super dealloc];
}

- (void)awakeFromNib
{
}

// accessors

- (NSString *)report
{
    return report;
}

- (void)setReport:(NSString *)newReport
{
    [report autorelease];
    report = [newReport retain];
    
    if (attr == nil) {
        attr = [[NSMutableDictionary dictionary] retain];
        NSFontManager *mgr = [NSFontManager sharedFontManager];
        NSFont *font = [mgr fontWithFamily:@"Monaco"
                                    traits:NSUnboldFontMask|NSUnitalicFontMask
                                    weight:5
                                    size:11];
        [attr setObject:font forKey:NSFontAttributeName];
    }
    
    NSAttributedString *newAttrReport = [[[NSAttributedString alloc] initWithString:report attributes:attr] autorelease];
    [self setAttrReport:newAttrReport];
}

- (NSAttributedString *)attrReport
{
    return attrReport;
}

- (void)setAttrReport:(NSAttributedString *)newAttrReport
{
    [attrReport autorelease];
    attrReport = [newAttrReport retain];
}

- (BOOL)isInPhase1
{
    return inPhase1;
}

- (void)setInPhase1:(BOOL)newFlag
{
    inPhase1 = newFlag;
}

- (BOOL)isInPhase2
{
    return inPhase2;
}

- (void)setInPhase2:(BOOL)newFlag
{
    inPhase2 = newFlag;
}

- (BOOL)isInPhase3
{
    return inPhase3;
}

- (void)setInPhase3:(BOOL)newFlag
{
    inPhase3 = newFlag;
}

// actions

- (IBAction)startAnalysis:(id)sender
{
    [self setInPhase1:NO];
    [self setInPhase2:YES];
    
    [self performSelectorOnMainThread:@selector(analyze) withObject:nil waitUntilDone:NO];
}

- (IBAction)visitHomePage:(id)sender
{
    NSString *pageURLString = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CYProductHomePage"];
    if (pageURLString != nil)
        [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:pageURLString]];
}

- (IBAction)visitHelpPage:(id)sender
{
    NSString *pageURLString = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CYProductHelpPage"];
    if (pageURLString != nil)
        [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:pageURLString]];
}

// analyzing

- (void)analyze
{
    ToolRunner *runner = [[ToolRunner alloc] initWithLaunchPath:[[NSBundle mainBundle] pathForResource:@"showpart" ofType:nil]
                                                      arguments:[NSArray arrayWithObject:@"/dev/rdisk0"]
                                               callbackSelector:@selector(reportCallback:)
                                                 callbackObject:self];
    [runner launchTool];
}

- (void)reportCallback:(NSString *)toolReport
{
    if (toolReport == nil) {
        [self setInPhase1:YES];
        [self setInPhase2:NO];
        return;
    }
    
    [self setReport:[NSString stringWithFormat:@"\n*** Report for internal hard disk ***\n%@", toolReport]];
    
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    [pb declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:nil];
    [pb setString:report forType:NSStringPboardType];
    
    [self setInPhase2:NO];
    [self setInPhase3:YES];
}

// delegates

- (BOOL)windowShouldClose:(id)sender
{
    [NSApp terminate:sender];
    return YES;
}

@end
