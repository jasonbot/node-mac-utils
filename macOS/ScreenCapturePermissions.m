#import "ScreenCapturePermissions.h"
#import <CoreGraphics/CoreGraphics.h>

@implementation ScreenCapturePermissions

+ (BOOL)checkScreenCaptureAccess {
  return CGPreflightScreenCaptureAccess();
}

+ (BOOL)requestScreenCaptureAccess {
  return CGRequestScreenCaptureAccess();
}

@end
