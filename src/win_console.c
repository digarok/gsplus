/*
   GSPLUS - Advanced Apple IIGS Emulator Environment
   Based on the KEGS emulator written by Kent Dickey
   See COPYRIGHT.txt for Copyright information
   See LICENSE.txt for license (GPL v2)
 */

#define WIN32_LEAN_AND_MEAN     /* Tell windows we want less header gunk */
#define STRICT                  /* Tell Windows we want compile type checks */

#include <windows.h>
#include <Shlwapi.h>

#include "winresource.h"
#include "defc.h"
#include "protos_windriver.h"

#include <io.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

extern void gsportinit(HWND _hwnd);
extern void gsportshut();
extern HWND g_hwnd_main;

extern char *g_status_ptrs[MAX_STATUS_LINES];
extern int g_win_status_debug;
extern int g_win_status_debug_request;
extern int g_win_fullscreen_state;

int win_nonblock_read_stdin(int fd, char *bufptr, int len)     {
  DWORD charsRead = 0;
  ReadConsole(GetStdHandle(STD_INPUT_HANDLE), bufptr, len, &charsRead, NULL);

  if (charsRead == 0)
  {
    errno = EAGAIN;
    return -1;
  }
  else
  {
    DWORD charsWritten = 0;
    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), bufptr, charsRead, &charsWritten, NULL);
    return charsRead;
  }
}

void get_cwd(LPTSTR buffer, int size) {
  HMODULE hSelf;
  hSelf = GetModuleHandle(NULL);
  GetModuleFileName(hSelf,buffer,size);
  PathRemoveFileSpec(buffer);
  //printf("Local directory: [%s]\n",buffer);
}

void x_dialog_create_gsport_conf(const char *str)      {
  // Just write the config file already...
  config_write_config_gsplus_file();
}

int x_show_alert(int is_fatal, const char *str) {

  if (str && *str) {
    adb_all_keys_up();
    MessageBox(NULL, str, "GS+", is_fatal ? MB_ICONERROR : MB_ICONWARNING);
  }
  return 0;
}

void get_default_window_size(LPSIZE size) {
  // Calculate the window client dimensions.
  RECT rect;
  rect.left = 0;
  rect.top = 0;
  rect.bottom = X_A2_WINDOW_HEIGHT;
  if (g_win_status_debug)
    rect.bottom += (MAX_STATUS_LINES * 16);
  rect.right = X_A2_WINDOW_WIDTH;

  // Calculate the window rectangle, which is the client area plus non-client area (e.g. frame and caption).
  AdjustWindowRect(&rect, WS_TILED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

  // Return the window size.
  size->cx = rect.right - rect.left;
  size->cy = rect.bottom - rect.top;
}

void x_toggle_status_lines() {
  SIZE size;

  if (!g_win_fullscreen_state)
  {
    g_win_status_debug = !g_win_status_debug;
    g_win_status_debug_request = g_win_status_debug;

    get_default_window_size(&size);
    SetWindowPos(g_hwnd_main, NULL, 0, 0, size.cx, size.cy, SWP_NOMOVE | SWP_NOZORDER);
    x_redraw_status_lines();
  }
}

void x_show_console(int show) {
  HWND hWnd = GetConsoleWindow();
  if (hWnd)
    ShowWindow(hWnd, show ? SW_SHOW : SW_HIDE);

  if (g_hwnd_main)
    SetFocus(g_hwnd_main);
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
  return main(0,0);
}

/* cygwin may have old headers ... */
#ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
#define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
#endif

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

#ifndef DISABLE_NEWLINE_AUTO_RETURN
#define DISABLE_NEWLINE_AUTO_RETURN 0x0008
#endif


#ifdef __CYGWIN__

static ssize_t handle_read(void *r, void *cookie, char *buffer, size_t size) {

  BOOL ok;
  DWORD n;
  ok = ReadConsole((HANDLE)cookie, buffer, size, &n, NULL);
  if (!ok) { errno = EIO; return -1; }
  return n;
}

static ssize_t handle_write(void *r, void *cookie, const char *buffer, size_t size) {

  static char *crbuffer = NULL;
  static int crbuffer_size = 0;

  BOOL ok;
  DWORD n;
  int crcount = 0;
  int i, j;

  for (i = 0; i < size; ++i) {
    if (buffer[i] == '\n') ++crcount;
  }

  if (crcount) {
    if (crbuffer_size < crcount + size) {
      crbuffer = realloc(crbuffer, size + crcount);
      if (!crbuffer) return -1;
      crbuffer_size = crcount + size;
    }

    for (i = 0, j = 0; i < size; ++i) {
      char c = buffer[i];
      if (c == '\n') crbuffer[j++] = '\r';
      crbuffer[j++] = c;
    }
  }


  ok = WriteConsole((HANDLE)cookie, crcount ? crbuffer : buffer, size + crcount, &n, NULL);
  if (!ok) { errno = EIO; return -1; }
  return size;
}

#endif

static void set_file_handle(FILE *fp, HANDLE h) {

  #ifdef __CYGWIN__
  fp->_file = -1;
  fp->_cookie = (void *)h;
  fp->_read = handle_read;
  fp->_write = handle_write;
  fp->_seek = NULL;
  fp->_close = NULL;
  #else
  int fd = _open_osfhandle((intptr_t)h, _O_TEXT);
  fp->_file = fd;
  #endif
}

int g_win32_cygwin;
void win_init_console(void) {
  /*
    powershell/cmd
    fd 0/1/2 closed
    GetStdHandle return 0
    stdin = -2, stdout = -2, stderr = -2

    msys/cygwin
    fd 0/1/2 open
    GetStdHandle return value (type = 3/pipe)
    stdin = 0, stdout = 1, stderr = 2
  */

  //struct stat st;
  //int ok;
  int fd;
  HANDLE h;
  DWORD mode;

#if 0
  FILE *dbg = fopen("debug.txt", "a+");
  h = GetStdHandle(STD_INPUT_HANDLE);
  fprintf(dbg, "STD_INPUT_HANDLE: %p\n", h);
  fprintf(dbg, "GetFileType: %08x\n", GetFileType(h));
  fprintf(dbg, "%d %d %d\n", stdin->_file, stdout->_file, stderr->_file);
  fclose(dbg);
#endif

  g_win32_cygwin = 0;

  setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
  setvbuf(stderr, NULL, _IOLBF, BUFSIZ);

#if 0
  if (fstat(0, &st) == 0) {
    g_win32_cygwin = 1;
    return;
  }
#endif

  SetStdHandle(STD_INPUT_HANDLE, 0);
  SetStdHandle(STD_OUTPUT_HANDLE, 0);
  SetStdHandle(STD_ERROR_HANDLE, 0);
  #if 0
  stdin->_file = 0;
  stdout->_file = 1;
  stderr->_file = 2;
  #endif


  AllocConsole();
  SetConsoleTitle("GS+");


  h = GetStdHandle(STD_INPUT_HANDLE);
  if (h != INVALID_HANDLE_VALUE) {

    mode = 0;
    GetConsoleMode(h, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    SetConsoleMode(h, mode);

    set_file_handle(stdin, h);
  }

  h = GetStdHandle(STD_OUTPUT_HANDLE);
  if (h != INVALID_HANDLE_VALUE) {

    mode = 0;
    GetConsoleMode(h, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
    SetConsoleMode(h, mode);

    //SetConsoleTextAttribute(h, BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    set_file_handle(stdout, h);
  }


  h = GetStdHandle(STD_ERROR_HANDLE);
  if (h != INVALID_HANDLE_VALUE) {

    mode = 0;
    GetConsoleMode(h, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
    SetConsoleMode(h, mode);

    set_file_handle(stderr, h);
  }


#if 0
  dbg = fopen("debug.txt", "a+");
  h = GetStdHandle(STD_INPUT_HANDLE);
  fprintf(dbg, "STD_INPUT_HANDLE: %p\n", h);
  fprintf(dbg, "GetFileType: %08x\n", GetFileType(h));
  fprintf(dbg, "%d %d %d\n", stdin->_file, stdout->_file, stderr->_file);
  fclose(dbg);
#endif
}


static void exit_sleep(void) {
  /* todo -- "press return to continue" */
  if (!g_win32_cygwin)
    sleep(10);
}

int main(int argc, char **argv) {
  // Hide the console initially to reduce window flashing.  We'll show the console later if needed.

  //atexit(exit_sleep);
  win_init_console();

  //x_show_console(0);


  // Register the window class.
  WNDCLASS wndclass;
  SIZE size;
  RECT rect;
  HHOOK hook;

  wndclass.style = 0;
  wndclass.lpfnWndProc = (WNDPROC)win_event_handler;
  wndclass.cbClsExtra = 0;
  wndclass.cbWndExtra = 0;
  wndclass.hInstance = GetModuleHandle(NULL);
  wndclass.hIcon = LoadIcon(wndclass.hInstance, MAKEINTRESOURCE(IDC_GSPORT32));
  wndclass.hCursor = LoadCursor((HINSTANCE) NULL, IDC_ARROW);
  wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  wndclass.lpszMenuName = NULL;
  wndclass.lpszClassName = "gsport";

  if(!RegisterClass(&wndclass)) {
    printf("Registering window failed\n");
    exit(1);
  }

  // Create the window.
  get_default_window_size(&size);

  HWND hwnd = CreateWindowEx(WS_EX_ACCEPTFILES, "gsport", "GSplus - Apple //gs Emulator",
                             WS_TILED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             size.cx, size.cy,
                             NULL, NULL, GetModuleHandle(NULL), NULL);


#if 0
  // Enable non-blocking, character-at-a-time console I/O.
  // win_nonblock_read_stdin() expects this behavior.
  DWORD mode;
  GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode);
  mode &= ~ENABLE_LINE_INPUT;
  SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), mode);
#endif

  hook = SetWindowsHookEx(WH_KEYBOARD_LL, win_ll_keyboard, NULL, 0);

  gsportinit(hwnd);
  int ret =  gsplusmain(argc, argv);

  UnhookWindowsHookEx(hook);
  UnregisterClass(wndclass.lpszClassName,GetModuleHandle(NULL));


  gsportshut();
  return ret;
}

void x_check_input_events() {
  MSG msg;

  while(PeekMessage(&msg, g_hwnd_main, 0, 0, PM_NOREMOVE)) {
    if(GetMessage(&msg, g_hwnd_main, 0, 0) > 0) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      printf("GetMessage returned <= 0\n");
      my_exit(2);
    }
  }
}

void x_redraw_status_lines()      {
  COLORREF oldtextcolor, oldbkcolor;
  char    *buf;
  int line;
  int len;
  int height;
  int margin;

  height = 16;
  margin = 0;
  if (g_win_status_debug)
  {
    HDC localdc = GetDC(g_hwnd_main);
    oldtextcolor = SetTextColor(localdc, RGB(255, 255, 255));
    oldbkcolor = SetBkColor(localdc, RGB(0, 0, 0));
    for(line = 0; line < MAX_STATUS_LINES; line++) {
      buf = g_status_ptrs[line];
      if(buf != 0) {
        len = strlen(buf);
        TextOut(localdc, 10, X_A2_WINDOW_HEIGHT +
                height*line + margin, buf, len);
      }
    }
    SetTextColor(localdc, oldtextcolor);
    SetBkColor(localdc, oldbkcolor);
    ReleaseDC(g_hwnd_main,localdc);
  }
}

int x_calc_ratio(float ratiox,float ratioy)     {
  return 0;       // not stretched
}
