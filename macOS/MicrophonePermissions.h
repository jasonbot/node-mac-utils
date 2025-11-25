#import <Foundation/Foundation.h>

@interface MicrophonePermissions : NSObject

/**
 * Gets the current microphone authorization status.
 *
 * @return A string representing the authorization status:
 *         "NotDetermined" - User hasn't granted or denied yet
 *         "Restricted" - App isn't permitted to use microphone
 *         "Denied" - User explicitly denied permission
 *         "Authorized" - User explicitly granted permission
 */
+ (NSString *)getMicrophoneAuthorizationStatus;

/**
 * Requests microphone access from the user.
 * This will trigger a system permission dialog if the user hasn't been asked before.
 *
 * @param completionHandler Block called with YES if access was granted, NO otherwise
 */
+ (void)requestMicrophoneAccess:(void (^)(BOOL granted))completionHandler;

@end
