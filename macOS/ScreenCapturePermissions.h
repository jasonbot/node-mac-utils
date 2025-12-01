#import <Foundation/Foundation.h>

@interface ScreenCapturePermissions : NSObject

/**
 * Checks if the application has screen capture permission.
 * This performs a preflight check without triggering a permission dialog.
 *
 * @return YES if the application has permission, NO otherwise
 */
+ (BOOL)checkScreenCaptureAccess;

/**
 * Requests screen capture access from the user.
 * This will trigger a system permission dialog if the user hasn't been asked before.
 *
 * @return YES if access was granted, NO otherwise
 */
+ (BOOL)requestScreenCaptureAccess;

@end
