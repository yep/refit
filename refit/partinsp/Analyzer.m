//
//  Analyzer.m
//  partshow
//
//  Created by Christoph Pfisterer on 26.11.06.
//  Copyright 2006 __MyCompanyName__. All rights reserved.
//

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
