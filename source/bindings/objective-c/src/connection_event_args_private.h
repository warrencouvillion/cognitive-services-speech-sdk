//
// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.md file in the project root for full license information.
//

#import "SPXConnectionEventArgs.h"
#import "common_private.h"

@interface SPXConnectionEventArgs (Private)

- (instancetype)init :(const SpeechImpl::ConnectionEventArgs&) eventArgsHandle;

@end