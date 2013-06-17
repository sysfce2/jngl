#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>
#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>

namespace jngl {
	class WindowImpl;
}

@interface JNGLView : UIView <UIKeyInput> {
	EAGLContext* context;
	float angle;
	int width;
	int height;
	CFTimeInterval startTime;
	jngl::WindowImpl* impl;
	bool pause;
	bool needToResetFrameLimiter;
}

- (void) drawView: (CADisplayLink*) displayLink;
- (void) didRotate: (NSNotification*) notification;
- (void) touchesEnded: (NSSet*) touches withEvent: (UIEvent*) event;
- (void) touchesMoved: (NSSet*) touches withEvent: (UIEvent*) event;
- (void) didRotate:(NSNotification*) notification;
- (void) setPause: (bool) p;

@end