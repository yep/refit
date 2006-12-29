//
//  ToolRunner.h
//  partshow
//
//  Created by Christoph Pfisterer on 26.11.06.
//  Copyright 2006 __MyCompanyName__. All rights reserved.
//

@interface ToolRunner : NSObject
{
    @private
    NSString *launchPath;
    NSArray *arguments;
    SEL callbackSelector;
    id callbackObject;
    
    NSTask *task;
    NSFileHandle *stdoutHandle;
    NSMutableData *stdoutData;
}

// init

- (id)initWithLaunchPath:(NSString *)_launchPath
               arguments:(NSArray *)_arguments
        callbackSelector:(SEL)_callbackSelector
          callbackObject:(id)_callbackObject;
- (void)launchTool;

- (void)readCompleted;
- (void)didReadNextChunk:(NSNotification *)notification;

@end
