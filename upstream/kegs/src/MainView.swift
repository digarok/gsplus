// $KmKId: MainView.swift,v 1.42 2024-09-15 13:55:35+00 kentd Exp $

//	Copyright 2019-2024 by Kent Dickey
//	This code is covered by the GNU GPL v3
//	See the file COPYING.txt or https://www.gnu.org/licenses/
//

import Cocoa
import CoreGraphics
import AudioToolbox

class MainView: NSView {

	var bitmapContext : CGContext!
	var bitmapData : UnsafeMutableRawPointer!
	var rawData : UnsafeMutablePointer<UInt32>!
	var current_flags : UInt = 0
	var mouse_moved : Bool = false
	var mac_warp_pointer : Int32 = 0
	var mac_hide_pointer : Int32 = 0
	var kimage_ptr : UnsafeMutablePointer<Kimage>!
	var closed : Bool = false
	var pixels_per_line = 640
	var max_height = 600

	let is_cmd = UInt(NSEvent.ModifierFlags.command.rawValue)
	let is_control = UInt(NSEvent.ModifierFlags.control.rawValue)
	let is_shift = NSEvent.ModifierFlags.shift.rawValue
	let is_capslock = NSEvent.ModifierFlags.capsLock.rawValue
	let is_option = NSEvent.ModifierFlags.option.rawValue


	override init(frame frameRect: NSRect) {
		super.init(frame: frameRect)
	}

	required init?(coder: NSCoder) {
		super.init(coder: coder)
	}

	func windowDidResize(_ notification: Notification) {
		print("DID RESIZE")
	}
	override func performKeyEquivalent(with event: NSEvent) -> Bool {
		let keycode = event.keyCode
		let is_repeat = event.isARepeat
		let unicode_key = get_unicode_key_from_event(event)
		// print(".performKeyEquiv keycode: \(keycode), is_repeat: " +
		//					"\(is_repeat)")
		if(((current_flags & is_cmd) == 0) || is_repeat) {
			// If CMD isn't being held down, just ignore this
			return false
		}
		// Otherwise, manually do down, then up, for this key
		adb_physical_key_update(kimage_ptr, Int32(keycode),
			UInt32(unicode_key), 0);
		adb_physical_key_update(kimage_ptr, Int32(keycode),
			UInt32(unicode_key), 1);
		return true
	}

	override var acceptsFirstResponder: Bool {
		return true
	}

	func get_unicode_key_from_event(_ event: NSEvent) -> UInt32 {
		var unicode_key = UInt32(0);
		if let str = event.charactersIgnoringModifiers {
			//print(" keydown unmod str: \(str), " +
			//		"code:\(event.keyCode)")
			let arr_chars = Array(str.unicodeScalars)
			//print(" arr_chars: \(arr_chars)")
			if(arr_chars.count == 1) {
				unicode_key = UInt32(arr_chars[0]);
				//print("key1:\(unicode_key)")
			}
		}
		return unicode_key
	}
	override func keyDown(with event: NSEvent) {
		let keycode = event.keyCode
		let is_repeat = event.isARepeat;
		// print(".keyDown code: \(keycode), repeat: \(is_repeat)")
		//if let str = event.characters {
		//	print(" keydown str: \(str), code:\(keycode)")
		//}
		let unicode_key = get_unicode_key_from_event(event)
		if(is_repeat) {
			// If we do CMD-E, then we never get a down for the E,
			//  but we will get repeat events for that E.  Let's
			//  ignore those
			return
		}
		adb_physical_key_update(kimage_ptr, Int32(keycode),
			UInt32(unicode_key), 0);
	}

	override func keyUp(with event: NSEvent) {
		let keycode = event.keyCode
		// let is_repeat = event.isARepeat;
		// print(".keyUp code: \(keycode), repeat: \(is_repeat)")
		let unicode_key = get_unicode_key_from_event(event)
		adb_physical_key_update(kimage_ptr, Int32(keycode),
			UInt32(unicode_key), 1);
	}

	override func flagsChanged(with event: NSEvent) {
		let flags = event.modifierFlags.rawValue &
				(is_cmd | is_control | is_shift | is_capslock |
								is_option)
		var c025_val = UInt32(0);
		if((flags & is_shift) != 0) {
			c025_val |= 1;
		}
		if((flags & is_control) != 0) {
			c025_val |= 2;
		}
		if((flags & is_capslock) != 0) {
			c025_val |= 4;
		}
		if((flags & is_option) != 0) {
			c025_val |= 0x40;
		}
		if((flags & is_cmd) != 0) {
			c025_val |= 0x80;
		}
		adb_update_c025_mask(kimage_ptr, c025_val, UInt32(0xc7));
		current_flags = flags
		//print("flagsChanged: \(flags) and keycode: \(event.keyCode)")
	}
	override func acceptsFirstMouse(for event: NSEvent?) -> Bool {
		// This is to let the first click when changing to this window
		//  through to the app, I probably don't want this.
		return false
	}
	override func mouseMoved(with event: NSEvent) {
		//let type = event.eventNumber
		//print(" event type: \(type)")
		mac_update_mouse(event: event, buttons_state:0, buttons_valid:0)

	}
	override func mouseDragged(with event: NSEvent) {
		mac_update_mouse(event: event, buttons_state: 0,
							buttons_valid: 0)
	}
	override func otherMouseDown(with event: NSEvent) {
		mac_update_mouse(event: event, buttons_state:2, buttons_valid:2)
	}
	override func otherMouseUp(with event: NSEvent) {
		mac_update_mouse(event: event, buttons_state:0, buttons_valid:2)
	}

	override func mouseEntered(with event: NSEvent) {
		print("mouse entered")
	}
	override func rightMouseUp(with event: NSEvent) {
		mac_update_mouse(event: event, buttons_state:0, buttons_valid:4)
	}
	override func rightMouseDown(with event: NSEvent) {
		print("Right mouse down")
		mac_update_mouse(event: event, buttons_state:4, buttons_valid:4)
	}
	override func mouseUp(with event: NSEvent) {
		// print("Mouse up \(event.locationInWindow.x)," +
		//				"\(event.locationInWindow.y)")
		mac_update_mouse(event: event, buttons_state:0, buttons_valid:1)
	}
	override func mouseDown(with event: NSEvent) {
		//print("Mouse down \(event.locationInWindow.x)," +
		//				"\(event.locationInWindow.y)")
		mac_update_mouse(event: event, buttons_state:1, buttons_valid:1)
	}

	func mac_update_mouse(event: NSEvent, buttons_state: Int,
							buttons_valid: Int) {
		var warp = Int32(0)
		var x_width = 0
		var y_height = 0
		var x = Int32(0)
		var y = Int32(0)
		var do_delta = Int(0)
		let hide = adb_get_hide_warp_info(kimage_ptr, &warp)
		if(warp != mac_warp_pointer) {
			mouse_moved = true
		}
		mac_warp_pointer = warp
		if(mac_hide_pointer != hide) {
			mac_hide_pointer(hide: hide)
		}
		mac_hide_pointer = hide
		let location = event.locationInWindow
		if(!Context_draw) {
			// We're letting the Mac scale the window for us,
			//  so video_scale_mouse*() doesn't know the scale
			//  factor, so pass it in
			x_width = Int(bounds.size.width);
			y_height = Int(bounds.size.height);
		}
		let raw_x = location.x
		let raw_y = bounds.size.height - 1 - location.y
			// raw_y is 0 at the top of the window now
		x = video_scale_mouse_x(kimage_ptr, Int32(raw_x),
						Int32(x_width))
		y = video_scale_mouse_y(kimage_ptr, Int32(raw_y),
						Int32(y_height))
		do_delta = 0;
		if(mac_warp_pointer != 0) {
			do_delta |= 0x1000;	// x,y are deltas
			x = Int32(event.deltaX)
			y = Int32(event.deltaY)
		}
		let ret = adb_update_mouse(kimage_ptr, x, y,
				Int32(buttons_state),
				Int32(buttons_valid | do_delta))
		if(ret != 0) {
			mouse_moved = true
		}
		guard let win = window else {
			return			// No valid window
		}
		if(mouse_moved) {
			var rect1 = NSRect.zero

			// If warp, warp cursor to middle of window.  Moving
			//  the cursor requires absolute screen coordinates,
			//  where y=0 is the top of the screen.  We must convert
			//  window coords (where y=0 is the bottom of the win).
			mouse_moved = false
			let warp_x = CGFloat(video_unscale_mouse_x(kimage_ptr,
				Int32(A2_WINDOW_WIDTH/2), Int32(x_width)))
			let warp_y = CGFloat(video_unscale_mouse_y(kimage_ptr,
				Int32(A2_WINDOW_HEIGHT/2), Int32(y_height)))
			let scr_height = CGDisplayPixelsHigh(CGMainDisplayID());
			rect1.origin.x = CGFloat(warp_x)
			rect1.origin.y = bounds.size.height - 1 -
								CGFloat(warp_y)
			rect1.size.width = 1;
			rect1.size.height = 0;
			let screen_rect = win.convertToScreen(rect1)
			let screen_rect_y = CGFloat(scr_height) -
							screen_rect.origin.y
			let cg_loc = CGPoint(x: screen_rect.origin.x,
				y: CGFloat(screen_rect_y))
			//print("scr_rect:\(screen_rect)")
			if(warp != 0) {
				// Warp to middle of the window
				CGDisplayMoveCursorToPoint(CGMainDisplayID(),
								cg_loc);
			}
		}
	}

	func mac_hide_pointer(hide: Int32) {
		// print("Hide called: \(hide)")
		if(hide != 0) {
			CGDisplayHideCursor(CGMainDisplayID())
		} else {
			CGDisplayShowCursor(CGMainDisplayID())
		}
	}
	func initialize() {
		//let colorSpace = CGColorSpace(name: CGColorSpace.sRGB)
		print("Initialize view called")
		// Get width,height from video.c to handle toggling status lines
		let width = Int(video_get_a2_width(kimage_ptr))
		let height = Int(video_get_a2_height(kimage_ptr))
		//if let screen = NSScreen.main {
		//	let rect = screen.frame
		//	width = Int(rect.size.width)
		//	height = Int(rect.size.height)
		//}
		pixels_per_line = width
		max_height = height
		//print("pixels_per_line: \(pixels_per_line), " +
		//		"max_height: \(max_height)")

		let color_space = CGDisplayCopyColorSpace(CGMainDisplayID())
		//let colorSpace = CGColorSpaceCreateDeviceRGB()
		bitmapContext = CGContext(data: nil, width: width,
			height: height, bitsPerComponent: 8,
			bytesPerRow: width * 4,
			space: color_space,
			bitmapInfo: CGImageAlphaInfo.noneSkipLast.rawValue)

		//CGImageAlphaInfo.noneSkipLast.rawValue
		bitmapData = bitmapContext.data!
		// Set the intial value of the data to black (0)
		bitmapData.initializeMemory(as: UInt32.self, repeating: 0,
						count: height * width)
		rawData = bitmapData.bindMemory(to: UInt32.self,
						capacity: height * width)
		//print("Calling video_update_scale from MainViewinitialize()")
		let x_width = Int(video_get_x_width(kimage_ptr))
		let x_height = Int(video_get_x_height(kimage_ptr))
		video_update_scale(kimage_ptr, Int32(x_width), Int32(x_height),
								Int32(1))
		if(Context_draw) {
			video_set_alpha_mask(UInt32(0xff000000))
			// Set video.c alpha mask, since 0 means transparent
		}
	}

	override func draw(_ dirtyRect: NSRect) {
		var rect : Change_rect
		//super.draw(dirtyRect)
			// Documentation says super.draw not needed...
		// Draw the current image buffer to the screen
		let context = NSGraphicsContext.current!.cgContext
		if(!Context_draw) {
			// Safe, simple drawing
			let image = bitmapContext.makeImage()!
			context.draw(image, in: bounds)
				// The above call does the actual copy of
				//  data to the screen, and can take a while
			//print("Draw, bounds:\(bounds), dirtyr:\(dirtyRect)")
		} else {
			// Unsafe, more direct drawing by peeking into
			// NSGraphicsContext.current.data
			if let data = context.data {
				rect = Change_rect(x:0, y:0,
					width:Int32(context.width),
					height:Int32(context.height));
				video_update_scale(kimage_ptr,
					Int32(context.width),
					Int32(context.height), Int32(0))
				video_out_data_scaled(data, kimage_ptr,
					Int32(context.bytesPerRow/4), &rect);
				video_out_done(kimage_ptr);
				print("Did out_data_scaled, rect:\(rect)");
			}
		}
	}

	@objc func do_config(_ : AnyObject) {
		// Create a "virtual" F4 press
		//print("do_config")
		// Create a keydown for the F4 key (keycode:0x76)
		adb_physical_key_update(kimage_ptr, Int32(0x76), 0, 0);

		// and create a keyup for the F4 key (keycode:0x76)
		adb_physical_key_update(kimage_ptr, Int32(0x76), 0, 1);
	}

	@objc func do_copy_text(_ : AnyObject) {
		// print("do_copy");
		//let text_buf_cstr = UnsafeMutablePointer<Int8>.allocate(
		//				capacity: 2100);
		if let cstr = cfg_text_screen_str() {
			let str = String(cString: cstr);
			NSPasteboard.general.clearContents();
			NSPasteboard.general.setString(str,
				forType: NSPasteboard.PasteboardType.string);
		}
	}
	@objc func do_paste(_ : AnyObject) {
		// print("do_paste")
		let general = NSPasteboard.general;
		guard let str = general.string(forType: .string) else {
			print("Cannot paste, nothing in clipboard");
			return
		}
		//print("str: \(str)")
		for raw_c in str.utf8 {
			let c = UInt32(raw_c)
			let ret = adb_paste_add_buf(c)
			if(ret != 0) {
				print("Paste too large!")
				return;
			}
		}
	}

	func mac_update_display() {
		var valid : Int32
		var rect : Change_rect
		var dirty_rect = NSRect.zero

		if(Context_draw) {
			// We just need to know if there are any changes,
			//  don't actually do the copies now
			valid = video_out_query(kimage_ptr);
			if(valid != 0) {
				self.setNeedsDisplay(bounds)
				print("Needs display");
			}
			return;
		}
		// Otherwise, update rawData in our Bitmap now
		rect = Change_rect(x:0, y:0, width:0, height:0)
		for i in 0...MAX_CHANGE_RECTS {		// MAX_CHANGE_RECTS
			valid = video_out_data(rawData, kimage_ptr,
				Int32(pixels_per_line), &rect, Int32(i))
			if(valid == 0) {
				break
			}
			dirty_rect.origin.x = CGFloat(rect.x)
			dirty_rect.origin.y = bounds.size.height -
					CGFloat(rect.y) - CGFloat(rect.height)
			dirty_rect.size.width = CGFloat(rect.width)
			dirty_rect.size.height = CGFloat(rect.height)

			self.setNeedsDisplay(bounds)
				// It's necessary to redraw the whole screen,
				//  there's no mechanism to just redraw part
				// The coordinates would need transformation
				//  (since Mac 0,0 is the lower left corner)
			//print("bounds: w:\(bounds.size.width) " +
			//			"h:\(bounds.size.height)\n")
			//self.setNeedsDisplay(dirty_rect)
		}
	}
}
