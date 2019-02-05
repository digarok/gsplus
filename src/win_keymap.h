/* this table is used to search for the Windows VK_* in col 1 or 2 */
/* flags bit 8 is or'ed into the VK, so we can distinguish keypad keys */
/* regardless of numlock */

int g_a2_key_to_wsym[][3] = {
	{ kVK_Escape,				VK_ESCAPE,	0 },
	{ kVK_F1,					VK_F1,	0 },
	{ kVK_F2,					VK_F2,	0 },
	{ kVK_F3,					VK_F3,	0 },
	{ kVK_F4,					VK_F4,	0 },
	{ kVK_F5,					VK_F5,	0 },
	{ kVK_F6,					VK_F6,	0 },
	{ kVK_F7,					VK_F7,	0 },
	{ kVK_F8,					VK_F8,	0 },
	{ kVK_F9,					VK_F9,	0 },
	{ kVK_F10,					VK_F10,	0 },
	{ kVK_F11,					VK_F11,	0 },
	{ kVK_F12,					VK_F12,	0 },
	{ kVK_F13,					VK_F13,	0 },
	{ kVK_F14,					VK_F14,	0 },
	{ kVK_F15,					VK_F15,	0 },
	{ kVK_Reset, 				VK_PAUSE, VK_CANCEL+0x100 },

	{ kVK_ANSI_Grave,			VK_OEM_3, 0 },		/* '`' */
	{ kVK_ANSI_1,				'1', 0 },
	{ kVK_ANSI_2,				'2', 0 },
	{ kVK_ANSI_3,				'3', 0 },
	{ kVK_ANSI_4,				'4', 0 },
	{ kVK_ANSI_5,				'5', 0 },
	{ kVK_ANSI_6,				'6', 0 },
	{ kVK_ANSI_7,				'7', 0 },
	{ kVK_ANSI_8,				'8', 0 },
	{ kVK_ANSI_9,				'9', 0 },
	{ kVK_ANSI_0,				'0', 0 },
	{ kVK_ANSI_Minus,			VK_OEM_MINUS, 0 },		/* '-' */
	{ kVK_ANSI_Equal,			VK_OEM_PLUS, 0 },		/* '=' */
	{ kVK_Delete,				VK_BACK, 0 },		/* backspace */
	{ kVK_Insert,				VK_INSERT+0x100, 0 },	/* Insert key */
	{ kVK_PageUp,				VK_PRIOR+0x100, 0 },	/* pageup */
	{ kVK_Home,					VK_HOME+0x100, 0 },		/* KP_equal is HOME key */

	{ kVK_Tab,					VK_TAB, 0 },
	{ kVK_ANSI_Q,				'Q', 0 },
	{ kVK_ANSI_W,				'W', 0 },
	{ kVK_ANSI_E,				'E', 0 },
	{ kVK_ANSI_R,				'R', 0 },
	{ kVK_ANSI_T,				'T', 0 },
	{ kVK_ANSI_Y,				'Y', 0 },
	{ kVK_ANSI_U,				'U', 0 },
	{ kVK_ANSI_I,				'I', 0 },
	{ kVK_ANSI_O,				'O', 0 },
	{ kVK_ANSI_P,				'P', 0 },
	{ kVK_ANSI_LeftBracket,		VK_OEM_4, 0 },	/* [ */
	{ kVK_ANSI_RightBracket,	VK_OEM_6, 0 },	/* ] */
	{ kVK_ANSI_Backslash,		VK_OEM_5, 0 },	/* backslash, bar */
	{ kVK_ForwardDelete,		VK_DELETE+0x100, 0 },
	{ kVK_End,					VK_END+0x100, 0 },
	{ kVK_PageDown,				VK_NEXT+0x100, 0 },



	// { kVK_CapsLock,			VK_CAPITAL, 0 },  // Handled specially!
	{ kVK_ANSI_A,				'A', 0 },
	{ kVK_ANSI_S,				'S', 0 },
	{ kVK_ANSI_D,				'D', 0 },
	{ kVK_ANSI_F,				'F', 0 },
	{ kVK_ANSI_G,				'G', 0 },
	{ kVK_ANSI_H,				'H', 0 },
	{ kVK_ANSI_J,				'J', 0 },
	{ kVK_ANSI_K,				'K', 0 },
	{ kVK_ANSI_L,				'L', 0 },
	{ kVK_ANSI_Semicolon,		VK_OEM_1, 0 },	/* ; */
	{ kVK_ANSI_Quote,			VK_OEM_7, 0 },	/* single quote */
	{ kVK_Return,				VK_RETURN, 0 },


	{ kVK_Shift,				VK_SHIFT, 0 },
	{ kVK_ANSI_Z,				'Z', 0 },
	{ kVK_ANSI_X,				'X', 0 },
	{ kVK_ANSI_C,				'C', 0 },
	{ kVK_ANSI_V,				'V', 0 },
	{ kVK_ANSI_B,				'B', 0 },
	{ kVK_ANSI_N,				'N', 0 },
	{ kVK_ANSI_M,				'M', 0 },
	{ kVK_ANSI_Comma,			VK_OEM_COMMA, 0 },	/* , */
	{ kVK_ANSI_Period,			VK_OEM_PERIOD, 0 },	/* . */
	{ kVK_ANSI_Slash,			VK_OEM_2, 0 },	/* / */


	{ kVK_Control,				VK_CONTROL, VK_CONTROL+0x100 },
	{ kVK_Option,				VK_SNAPSHOT+0x100, VK_MENU+0x100 },/* Opt=prntscrn or alt-r */

// OG ActiveGS map OA-CA to Win & AltKey
#ifndef ACTIVEGS
	{ kVK_Command,				VK_SCROLL, VK_MENU },		/* Command=scr_lock or alt-l */
#else
	{ kVK_Reset, 				VK_CANCEL, 0 },
	{ kVK_Option, 				VK_LWIN+0x100, VK_LWIN },
	{ kVK_Command,				VK_MENU, 0 },		/* Command=alt-l */
	{ kVK_Command,				VK_LMENU, 0 },		/* Command=alt-l */
	{ kVK_Reset,				VK_SCROLL,0 },		/* RESET */
	{ kVK_Control,				VK_LCONTROL, 0 },	// CTRL
#endif

	{ kVK_Space,				' ', 0 },
	{ kVK_LeftArrow,			VK_LEFT+0x100, 0 },
	{ kVK_DownArrow,			VK_DOWN+0x100, 0 },
	{ kVK_RightArrow,			VK_RIGHT+0x100, 0 },
	{ kVK_UpArrow,				VK_UP+0x100, 0 },


	{ kVK_ANSI_Keypad1,			VK_NUMPAD1, VK_END },
	{ kVK_ANSI_Keypad2,			VK_NUMPAD2, VK_DOWN },
	{ kVK_ANSI_Keypad3,			VK_NUMPAD3, VK_NEXT },
	{ kVK_ANSI_Keypad4,			VK_NUMPAD4, VK_LEFT },
	{ kVK_ANSI_Keypad5,			VK_NUMPAD5, VK_CLEAR },
	{ kVK_ANSI_Keypad6,			VK_NUMPAD6, VK_RIGHT },
	{ kVK_ANSI_Keypad7,			VK_NUMPAD7, VK_HOME },
	{ kVK_ANSI_Keypad8,			VK_NUMPAD8, VK_UP },
	{ kVK_ANSI_Keypad9,			VK_NUMPAD9, VK_PRIOR },
	{ kVK_ANSI_Keypad0,			VK_NUMPAD0, VK_INSERT },
	{ kVK_ANSI_KeypadDecimal,	VK_DECIMAL, VK_DECIMAL },
	{ kVK_ANSI_KeypadEnter,		VK_RETURN+0x100, 0 },
	{ kVK_ANSI_KeypadClear,		VK_NUMLOCK, VK_NUMLOCK+0x100 },	/* clear */
	{ kVK_ANSI_KeypadMinus,		VK_SUBTRACT, VK_SUBTRACT+0x100 },
	{ kVK_ANSI_KeypadPlus,		VK_ADD, VK_ADD+0x100 },
	{ kVK_ANSI_KeypadDivide,	VK_DIVIDE, VK_DIVIDE+0x100 },
	{ kVK_ANSI_KeypadMultiply,	VK_MULTIPLY, VK_MULTIPLY+0x100 },

	{ -1, -1, -1 }
};
