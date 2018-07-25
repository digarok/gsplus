/*
  GSPLUS - Advanced Apple IIGS Emulator Environment
  Based on the KEGS emulator written by Kent Dickey
  See COPYRIGHT.txt for Copyright information
	See LICENSE.txt for license (GPL v2)
*/

/*
 * Function prototypes
 */
MRESULT EXPENTRY MyWindowProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );
VOID os2_abort(HWND hwndFrame, HWND hwndClient);

#define MSGBOXID    1001

#define ID_WINDOW   256

#define ID_OPTIONS  257
#define ID_OPTION1  258
#define ID_OPTION2  259
#define ID_OPTION3  260

#define ID_EXITPROG 261

#define IDS_HELLO   262
#define IDS_1       263
#define IDS_2       264
#define IDS_3       265

#define ID_BITMAP   266 /* For testing backgrounds - can be removed once video works */
