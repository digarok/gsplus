
#import <Cocoa/Cocoa.h>


void fix_mac_menu(void) {

	/*
	 * add an option-key modifier to all menu shortcuts
	 * eg, command-Q -> option+command-Q
	 */

	@autoreleasepool {
		if (NSApp) {
			NSMenu *menu = [NSApp mainMenu];

			for (NSMenuItem *a in [menu itemArray]) {
				for (NSMenuItem *b in [[a submenu] itemArray]) {
					unsigned m = [b keyEquivalentModifierMask];
					if (m & NSEventModifierFlagCommand)
						[b setKeyEquivalentModifierMask: m | NSEventModifierFlagOption];
				}
			}
		}
	}

}
