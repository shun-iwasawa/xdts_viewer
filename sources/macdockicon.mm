#include "macdockicon.h"

#ifdef __MACOS__
#import <AppKit/AppKit.h>

void hideDockIcon() {
  [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}
#endif
