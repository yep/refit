//
//  Analyzer.h
//  partshow
//
//  Created by Christoph Pfisterer on 26.11.06.
//  Copyright 2006 __MyCompanyName__. All rights reserved.
//

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

// analyzing

- (void)analyze;
- (void)reportCallback:(NSString *)report;

// delegates

- (BOOL)windowShouldClose:(id)sender;

@end
