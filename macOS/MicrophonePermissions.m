#import "MicrophonePermissions.h"
#import <AVFoundation/AVFoundation.h>

@implementation MicrophonePermissions

+ (NSString *)getMicrophoneAuthorizationStatus {
  AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];

  switch (status) {
    case AVAuthorizationStatusNotDetermined:
      return @"NotDetermined";
    case AVAuthorizationStatusRestricted:
      return @"Restricted";
    case AVAuthorizationStatusDenied:
      return @"Denied";
    case AVAuthorizationStatusAuthorized:
      return @"Authorized";
    default:
      return @"NotDetermined";
  }
}

@end
