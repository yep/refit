/*
 * partinsp/ToolRunner.m
 * Helper class to run embedded tool with root privileges
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

#import "ToolRunner.h"

#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>


@implementation ToolRunner

// init

- (id)initWithLaunchPath:(NSString *)_launchPath
               arguments:(NSArray *)_arguments
        callbackSelector:(SEL)_callbackSelector
          callbackObject:(id)_callbackObject
{
    self = [super init];
    if (self) {
        
        launchPath = [_launchPath retain];
        arguments = [_arguments retain];
        callbackSelector = _callbackSelector;
        callbackObject = _callbackObject;
        
        task = nil;
        stdoutHandle = nil;
        stdoutData = nil;
        
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    
    [launchPath release];
    [arguments release];
    
    [stdoutData release];
    
    [super dealloc];
}

- (void)launchTool
{
    stdoutData = [[NSMutableData data] retain];
    
#ifdef USE_SUDO
    task = [[NSTask alloc] init];
    [task setLaunchPath:@"/usr/bin/sudo"];
    NSMutableArray *fullArguments = [NSMutableArray array];
    [fullArguments addObject:launchPath];
    [fullArguments addObjectsFromArray:arguments];
    [task setArguments:fullArguments];
    
    NSPipe *stdoutPipe = [NSPipe pipe];
    stdoutHandle = [[stdoutPipe fileHandleForReading] retain];
    [task setStandardOutput:stdoutPipe]; 
    [task launch];
    
#else
    OSStatus myStatus;
    int i;
    FILE *pipefile;
    
    // create authorization session
    AuthorizationFlags myFlags = kAuthorizationFlagDefaults;
    AuthorizationRef myAuthorizationRef;
    myStatus = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment,
                                   myFlags, &myAuthorizationRef);
    if (myStatus == errAuthorizationSuccess) {
        
        // add the execute right
        AuthorizationItem myItems = { kAuthorizationRightExecute, 0, NULL, 0 };
        AuthorizationRights myRights = { 1, &myItems };
        myFlags = kAuthorizationFlagDefaults |
            kAuthorizationFlagInteractionAllowed |
            kAuthorizationFlagPreAuthorize |
            kAuthorizationFlagExtendRights;
        myStatus = AuthorizationCopyRights(myAuthorizationRef,
                                           &myRights, NULL, myFlags, NULL);
        if (myStatus == errAuthorizationSuccess) {
            const char *myToolPath = [launchPath UTF8String];
            const char **myArguments = malloc(sizeof(const char *) * ([arguments count] + 1));
            if (myArguments == nil)
                return;
            for (i = 0; i < [arguments count]; i++)
                myArguments[i] = [[arguments objectAtIndex:i] UTF8String];
            myArguments[i] = nil;
            
            myFlags = kAuthorizationFlagDefaults;
            myStatus = AuthorizationExecuteWithPrivileges(myAuthorizationRef,
                                                          myToolPath,
                                                          myFlags,
                                                          (char **)myArguments,
                                                          &pipefile);
            // NOTE: The cast of myArguments avoids a compiler warning only.
            //  The function actually expects a "const * char *", but the Obj-C
            //  compiler somehow doesn't know about that...
            free(myArguments);
            
            if (myStatus == errAuthorizationSuccess) {
                stdoutHandle = [[NSFileHandle alloc] initWithFileDescriptor:fileno(pipefile)];
            } else {
                // TODO: error handling
            }
        }
        //AuthorizationFree(myAuthorizationRef, kAuthorizationFlagDefaults);
    }
    // TODO: error handling
    
    if (stdoutHandle == nil) {
        [callbackObject performSelectorOnMainThread:callbackSelector withObject:nil waitUntilDone:NO];
        return;
    }
    
#endif
    
    // arrange for data to be read asynchronously
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(didReadNextChunk:)
                                                 name:NSFileHandleReadCompletionNotification
                                               object:stdoutHandle];
    [stdoutHandle readInBackgroundAndNotify];
}

- (void)readCompleted
{
    // close our end of the pipe
    [stdoutHandle closeFile];
    [stdoutHandle release];
    stdoutHandle = nil;
    
    // release the task
    [task release];
    task = nil;
    
    // send all data to the client object
    NSString *stdoutString = [[[NSString alloc] initWithData:stdoutData encoding:NSUTF8StringEncoding] autorelease];
    [stdoutData release];
    stdoutData = nil;
    [callbackObject performSelectorOnMainThread:callbackSelector withObject:stdoutString waitUntilDone:NO];
}

- (void)didReadNextChunk:(NSNotification *)notification
{
    if ([notification object] != stdoutHandle)
        return;
    
    NSData *chunk = [[notification userInfo] objectForKey:NSFileHandleNotificationDataItem];
    if ([chunk length] == 0) {
        // reached EOF
        [self readCompleted];
    } else {
        // got some data
        [stdoutData appendData:chunk];
        [stdoutHandle readInBackgroundAndNotify];
    }
}

@end
