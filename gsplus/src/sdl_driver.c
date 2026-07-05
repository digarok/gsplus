/**********************************************************************/
/*                    GSplus - Apple //gs Emulator                    */
/*                    Based on KEGS by Kent Dickey                    */
/*      This code is covered by the GNU GPL v3                        */
/**********************************************************************/

/* SDL3 display/input driver and program entry point.
 *
 * KEGS has no platform-independent main(): each display driver owns the entry
 * point and the run loop, calling into the core. This file is the SDL3
 * equivalent of xdriver.c (X11) / windriver.c (Win32). The loop is:
 *
 *     parse_argv() -> kegs_init() -> sdl_video_init()
 *     while(running) { run_16ms(); poll input; push framebuffer }
 *
 * The core hands us framebuffers as Kimage objects. Each frame we ask the core
 * to copy its changed rectangles into our pixel buffer (video_out_data) and we
 * upload those to an SDL streaming texture.
 *
 * MILESTONE STATUS (Phase 2a): video + window + quit work. Keyboard/mouse input
 * (2b) and real audio (2c) are TODO; see the marked spots below.
 */

#include <SDL3/SDL.h>
#include <time.h>

#include "defc.h"
#include "protos_sdl.h"
#ifndef __APPLE__
# include "gsplus_icon.h"	/* embedded RGBA window icon (see below) */
#endif

#ifdef _WIN32
# include <sys/stat.h>
/* mingw/Windows has no lstat(). The core declares it (defc.h) and the native
 * Windows driver (windriver.c, which the SDL build excludes) provides this
 * shim -- so we supply it here. Windows has no POSIX symlinks, so stat() does. */
int
lstat(const char *path, struct stat *bufptr)
{
	return stat(path, bufptr);
}
#endif

/* Each KEGS driver defines its own private "window info" wrapper around a core
 * Kimage. Ours holds the SDL window/renderer/texture plus the scratch pixel
 * buffer the core fills. */
typedef struct {
	Kimage		*kimage_ptr;
	SDL_Window	*window;
	SDL_Renderer	*renderer;
	SDL_Texture	*texture;
	SDL_Texture	*overlay;		/* 2-row scanline tile, tiled over the
						 * final output in host pixels */
	int		overlay_for;		/* intensity the overlay was filled for */
	/* CRT effect resources (built lazily by sdl_create_texture when on). */
	SDL_Texture	*crt_target;		/* composited screen (fb+mask) */
	SDL_Texture	*glow_target;		/* downscaled crt_target, used for bloom */
	SDL_Texture	*mask;			/* 1-row R/G/B phosphor-mask tile (MUL),
						 * tiled over the output in host pixels */
	int		mask_for;		/* g_crt_mask the mask was filled for */
	SDL_Vertex	*crt_verts;		/* warped curvature mesh vertices */
	int		*crt_idx;		/* mesh triangle indices */
	int		crt_nverts;
	int		crt_nidx;
	int		crt_curve_for;		/* g_crt_curve the mesh was built for */
	int		crt_vig_for;		/* g_crt_vignette the mesh was built for */
	word32		*data;			/* dest buffer for video_out_data() */
	int		active;
	int		width_req;		/* current logical width  (pixels) */
	int		main_height;		/* current logical height (pixels) */
	int		pixels_per_line;	/* stride of data[]       (pixels) */
} Window_info;

static Window_info g_mainwin_info;

/* Window/display options, defined in config.c and settable via the command line
 * (e.g. "-fullscreen 1") or config.kegs. */
extern int g_fullscreen, g_borderless, g_noaspect, g_highdpi;
extern int g_nohwaccel;
extern int g_scanline_simulator;	/* CRT scanline overlay intensity, 0-100 */
extern int g_crt;			/* curved CRT effect on/off */
extern int g_crt_curve;			/* CRT screen curvature, 0-100 */
extern int g_crt_mask;			/* CRT phosphor-mask strength, 0-100 */
extern int g_crt_vignette;		/* CRT corner darkening, 0-100 */
extern int g_hblur;			/* horizontal linear blur, 0-100 */
extern int g_vblur;			/* vertical linear blur, 0-100 */
extern int g_hide_mouse;		/* hide host cursor over window / in fullscreen */
extern int g_mainwin_xpos, g_mainwin_ypos;	/* window position (KEGS config vars) */
extern char *g_cfg_ssdir;		/* screenshot output dir ("" = current dir) */
extern int g_halt_sim;			/* nonzero while the debugger has the CPU halted */

static int g_is_fullscreen = 0;		/* current fullscreen state (F11 toggles) */
static int g_mouse_over_window = 0;	/* cursor is inside our window (ENTER/LEAVE) */
static int g_cursor_hidden = 0;		/* current SDL cursor state we last set */
static int g_scanline_saved = 50;	/* intensity to restore when toggled back on */
static int g_screenshot_requested = 0;	/* set by Shift+F12, serviced at frame end */

/* CRT effect tuning. The curvature amount is a config var (g_crt_curve, 0-100);
 * these bake the rest of the look so the user gets one "CRT Effect" toggle. */
#define CRT_MESH_COLS	32		/* curvature mesh resolution (cells) */
#define CRT_MESH_ROWS	24
#define CRT_MASK_MIN	60		/* off-channel level at full mask strength
					 * (0-255); g_crt_mask scales 255 (off)
					 * down toward this floor */
#define CRT_MASK_STRIPE_PX 2		/* target phosphor stripe width in device
					 * (physical screen) pixels */
#define CRT_GLOW_DIV	3		/* glow downsample factor (blur radius) */
#define CRT_GLOW_LEVEL	90		/* additive bloom strength, 0-255 */

/* Widths of the 2-row scanline tile and the 1-row phosphor-mask tile. Any
 * width works (they're tiled across the output; the mask just needs a multiple
 * of its 3-column R/G/B period); wider means fewer tiles for backends without
 * WRAP support. */
#define SCAN_TILE_W	64
#define MASK_TILE_W	63

/* Version string (set by the build from the git tag; see CMakeLists.txt). */
#ifndef GSPLUS_VERSION_STR
# define GSPLUS_VERSION_STR	"dev"
#endif

/* Up-front buffer size: large enough for every IIgs video mode (incl. borders
 * and scaling headroom). The actual window is sized to the current mode. */
#define SDL_MAX_WIDTH	1280
#define SDL_MAX_HEIGHT	1024

static int g_quit_requested = 0;

static void sdl_create_crt(Window_info *win, int w, int h);
static void sdl_build_crt_mesh(Window_info *win, int w, int h);
static void sdl_render_crt(Window_info *win);
static void sdl_render_framebuffer(Window_info *win, int w, int h);

/* (Re)create the streaming texture at the given size. ARGB8888 matches what
 * video_out_data() writes when mdepth == 32. */
static void
sdl_create_texture(Window_info *win, int w, int h)
{
	if(win->texture) {
		SDL_DestroyTexture(win->texture);
		win->texture = NULL;
	}
	/* STATIC (not STREAMING) access: we update changed rectangles with
	 * SDL_UpdateTexture and keep the rest of the frame intact. Static textures
	 * are the documented target for SDL_UpdateTexture and preserve their
	 * contents between frames; partial SDL_UpdateTexture on a streaming texture
	 * is unreliable on the D3D11 backend (Windows showed a black window). */
	win->texture = SDL_CreateTexture(win->renderer, SDL_PIXELFORMAT_ARGB8888,
					SDL_TEXTUREACCESS_STATIC, w, h);
	if(!win->texture) {
		printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
		return;
	}
	/* Opaque copy (ignore the texture's alpha) and crisp nearest-neighbour. */
	SDL_SetTextureBlendMode(win->texture, SDL_BLENDMODE_NONE);
	SDL_SetTextureScaleMode(win->texture, SDL_SCALEMODE_NEAREST);

	/* Create the scanline tile once: 2 rows (clear, dark) tiled across the
	 * *output* in host pixels at render time, so the dark lines are always
	 * exactly one screen pixel no matter how far the image is upscaled. It
	 * blends over the scaled picture, so it uses alpha (BLEND). Its size
	 * doesn't depend on the framebuffer, so no rebuild on mode changes. */
	if(!win->overlay) {
		win->overlay = SDL_CreateTexture(win->renderer,
					SDL_PIXELFORMAT_ARGB8888,
					SDL_TEXTUREACCESS_STATIC, SCAN_TILE_W, 2);
		if(win->overlay) {
			SDL_SetTextureBlendMode(win->overlay,
						SDL_BLENDMODE_BLEND);
			SDL_SetTextureScaleMode(win->overlay,
						SDL_SCALEMODE_NEAREST);
		}
		win->overlay_for = -1;		/* force a fill */
	}

	/* Same idea for the phosphor mask: one row of R/G/B stripe columns,
	 * tiled across the output in host pixels so the grille pitch stays a
	 * fixed couple of device pixels at any window size. MUL blend so each
	 * column lets one channel through and dims the other two. */
	if(!win->mask) {
		win->mask = SDL_CreateTexture(win->renderer,
					SDL_PIXELFORMAT_ARGB8888,
					SDL_TEXTUREACCESS_STATIC, MASK_TILE_W, 1);
		if(win->mask) {
			SDL_SetTextureBlendMode(win->mask, SDL_BLENDMODE_MUL);
			SDL_SetTextureScaleMode(win->mask,
						SDL_SCALEMODE_NEAREST);
		}
		win->mask_for = -1;		/* force a fill */
	}

	/* (Re)build the CRT effect resources at the new size. Cheap to keep around
	 * even when the effect is off; the render path just skips them. */
	sdl_create_crt(win, w, h);
}

/* --------------------------------------------------------------------------
 * Curved-CRT effect (no shaders, no extra libraries).
 *
 * Built entirely on the 2D renderer:
 *   - screen curvature via SDL_RenderGeometry: a warped vertex mesh that the GPU
 *     samples the framebuffer onto, with vignette baked into the vertex colours
 *   - bloom/glow via a downscaled (= blurred) copy redrawn additively
 *   - an RGB aperture-grille "phosphor" mask (MULTIPLY blend) and the scanline
 *     overlay, both drawn last in host-pixel space (see sdl_render_overlays)
 *
 * Each frame, sdl_render_crt() copies the framebuffer into an offscreen
 * target, downscales it for glow, then warps both onto the curve.
 * -------------------------------------------------------------------------- */

/* Fill the phosphor-mask tile with vertical R/G/B stripe columns, the classic
 * Trinitron aperture-grille look. Alpha 255 so the multiply is exact.
 *
 * The off-channel "dim" level is driven by g_crt_mask (0-100): 0 leaves them at
 * 255 (mask invisible), 100 pulls them down to CRT_MASK_MIN (strongest). The
 * default is deliberately subtle. */
static void
sdl_fill_mask(Window_info *win)
{
	word32	buf[MASK_TILE_W];
	int	x, col, r, g, b, dim, strength;

	if(!win->mask) {
		return;
	}
	strength = g_crt_mask;
	if(strength < 0)   { strength = 0; }
	if(strength > 100) { strength = 100; }
	dim = 255 - (strength * (255 - CRT_MASK_MIN) / 100);
	for(x = 0; x < MASK_TILE_W; x++) {
		col = x % 3;
		r = g = b = dim;
		if(col == 0)      { r = 255; }
		else if(col == 1) { g = 255; }
		else              { b = 255; }
		buf[x] = 0xff000000u | ((word32)r << 16) |
				((word32)g << 8) | (word32)b;
	}
	SDL_UpdateTexture(win->mask, NULL, buf,
				MASK_TILE_W * (int)sizeof(word32));
	win->mask_for = g_crt_mask;
}

/* Build the curvature mesh: a CRT_MESH_COLS x CRT_MESH_ROWS grid whose vertices
 * are barrel-distorted (edges bow outward, corners pull in) and tinted darker
 * toward the corners for a vignette. UVs stay uniform so the GPU maps the flat
 * framebuffer onto the curved surface. Positions are in logical pixels; the
 * renderer's logical presentation then letterboxes them into the window. */
static void
sdl_build_crt_mesh(Window_info *win, int w, int h)
{
	int	i, j, nx, ny, n, idx, vig_strength;
	float	curve, vignette, u, v, px, py, wx, wy, r2, vig;

	nx = CRT_MESH_COLS + 1;
	ny = CRT_MESH_ROWS + 1;
	free(win->crt_verts);
	free(win->crt_idx);
	win->crt_verts = malloc((size_t)nx * ny * sizeof(SDL_Vertex));
	win->crt_idx = malloc((size_t)CRT_MESH_COLS * CRT_MESH_ROWS * 6 *
				sizeof(int));
	if(!win->crt_verts || !win->crt_idx) {
		free(win->crt_verts); win->crt_verts = NULL;
		free(win->crt_idx);   win->crt_idx = NULL;
		return;
	}

	/* 0-100 curvature knob -> a gentle 0..~0.30 distortion coefficient. */
	curve = (float)g_crt_curve * 0.003f;

	/* 0-100 vignette knob -> 0..1 corner-darkening amount (0 = flat). */
	vig_strength = g_crt_vignette;
	if(vig_strength < 0)   { vig_strength = 0; }
	if(vig_strength > 100) { vig_strength = 100; }
	vignette = (float)vig_strength * 0.01f;

	n = 0;
	for(j = 0; j < ny; j++) {
		for(i = 0; i < nx; i++) {
			u = (float)i / (float)CRT_MESH_COLS;
			v = (float)j / (float)CRT_MESH_ROWS;
			px = u * 2.0f - 1.0f;		/* [-1,1] */
			py = v * 2.0f - 1.0f;
			/* Barrel: bow each axis out in its middle, pull corners
			 * in by the square of the other axis' distance. */
			wx = px * (1.0f - curve * py * py);
			wy = py * (1.0f - curve * px * px);
			win->crt_verts[n].position.x = (wx * 0.5f + 0.5f) * w;
			win->crt_verts[n].position.y = (wy * 0.5f + 0.5f) * h;
			win->crt_verts[n].tex_coord.x = u;
			win->crt_verts[n].tex_coord.y = v;
			r2 = px * px + py * py;		/* 0 centre .. 2 corner */
			vig = 1.0f - vignette * (r2 * 0.5f);
			if(vig < 0.0f) { vig = 0.0f; }
			win->crt_verts[n].color.r = vig;
			win->crt_verts[n].color.g = vig;
			win->crt_verts[n].color.b = vig;
			win->crt_verts[n].color.a = 1.0f;
			n++;
		}
	}
	win->crt_nverts = n;

	idx = 0;
	for(j = 0; j < CRT_MESH_ROWS; j++) {
		for(i = 0; i < CRT_MESH_COLS; i++) {
			int n00 = j * nx + i;
			int n10 = n00 + 1;
			int n01 = n00 + nx;
			int n11 = n01 + 1;
			win->crt_idx[idx++] = n00;
			win->crt_idx[idx++] = n10;
			win->crt_idx[idx++] = n11;
			win->crt_idx[idx++] = n00;
			win->crt_idx[idx++] = n11;
			win->crt_idx[idx++] = n01;
		}
	}
	win->crt_nidx = idx;
	win->crt_curve_for = g_crt_curve;
	win->crt_vig_for = g_crt_vignette;
}

/* (Re)create the offscreen targets + mesh at the given logical size. */
static void
sdl_create_crt(Window_info *win, int w, int h)
{
	int	gw, gh;

	if(win->crt_target)  { SDL_DestroyTexture(win->crt_target);  }
	if(win->glow_target) { SDL_DestroyTexture(win->glow_target); }
	win->crt_target = win->glow_target = NULL;

	/* Composite target: the (optionally blurred) framebuffer lands here, then
	 * the curve mesh samples it. LINEAR so the warp/upscale to the window
	 * softens like a real tube instead of showing hard pixel edges. */
	win->crt_target = SDL_CreateTexture(win->renderer,
				SDL_PIXELFORMAT_ARGB8888,
				SDL_TEXTUREACCESS_TARGET, w, h);
	if(win->crt_target) {
		SDL_SetTextureScaleMode(win->crt_target, SDL_SCALEMODE_LINEAR);
	}

	/* Glow target: a small copy of crt_target. Downsampling with LINEAR is a
	 * cheap box blur; drawn back additively it becomes the bloom halo. */
	gw = w / CRT_GLOW_DIV; if(gw < 1) { gw = 1; }
	gh = h / CRT_GLOW_DIV; if(gh < 1) { gh = 1; }
	win->glow_target = SDL_CreateTexture(win->renderer,
				SDL_PIXELFORMAT_ARGB8888,
				SDL_TEXTUREACCESS_TARGET, gw, gh);
	if(win->glow_target) {
		SDL_SetTextureScaleMode(win->glow_target, SDL_SCALEMODE_LINEAR);
	}

	sdl_build_crt_mesh(win, w, h);
}

/* Fill the scanline tile: row 0 transparent, row 1 semi-transparent black.
 * intensity is 0-100 and maps to the alpha of the dark row. */
static void
sdl_fill_overlay(Window_info *win, int intensity)
{
	word32	buf[SCAN_TILE_W * 2];
	word32	argb;
	int	x, alpha;

	if(!win->overlay) {
		return;
	}
	alpha = intensity * 255 / 100;
	if(alpha > 255) { alpha = 255; }
	argb = (word32)alpha << 24;		/* black (RGB 0) with this alpha */
	for(x = 0; x < SCAN_TILE_W; x++) {
		buf[x] = 0;
		buf[SCAN_TILE_W + x] = argb;
	}
	SDL_UpdateTexture(win->overlay, NULL, buf,
				SCAN_TILE_W * (int)sizeof(word32));
	win->overlay_for = intensity;
}

/* Output pixels per mask texel so one stripe covers ~CRT_MASK_STRIPE_PX
 * physical screen pixels regardless of the highdpi setting. Device pixels per
 * output pixel is the display's content scale divided by the window's pixel
 * density (retina, highdpi=1: 2/2 -> output px ARE device px; highdpi=0: 2/1
 * -> the OS doubles each output px). Rounded to an integer so the stripes tile
 * evenly instead of beating against the pixel grid. */
static float
sdl_mask_scale(Window_info *win)
{
	float	density, content, scale;

	density = SDL_GetWindowPixelDensity(win->window);
	content = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(win->window));
	if(density <= 0.0f) { density = 1.0f; }
	if(content <= 0.0f) { content = 1.0f; }
	scale = (float)(int)((float)CRT_MASK_STRIPE_PX * density / content
								+ 0.5f);
	return (scale < 1.0f) ? 1.0f : scale;
}

/* Draw the phosphor mask and scanlines over the finished frame, in *output*
 * (host pixel) space. The picture textures are all at the emulator's logical
 * resolution, so blending these effects in that space and upscaling stretched
 * them along with the picture (~2+ host pixels per scanline/stripe, smeared by
 * the LINEAR upscale). Instead the tiles go on 1:1 after the image has been
 * scaled, so the effects land on actual screen pixels. Logical presentation is
 * turned off for the draw and restored right after (mouse-coordinate mapping
 * needs it). */
static void
sdl_render_overlays(Window_info *win)
{
	SDL_Renderer	*r = win->renderer;
	SDL_FRect	dst;
	int		ow, oh;
	int	want_mask = (g_crt && (g_crt_mask > 0) && win->mask);
	int	want_scan = ((g_scanline_simulator > 0) && win->overlay);

	if(!want_mask && !want_scan) {
		return;
	}
	SDL_SetRenderLogicalPresentation(r, 0, 0,
				SDL_LOGICAL_PRESENTATION_DISABLED);
	if(SDL_GetRenderOutputSize(r, &ow, &oh)) {
		dst.x = 0.0f;
		dst.y = 0.0f;
		dst.w = (float)ow;
		dst.h = (float)oh;
		/* Both cover the letterbox bars too, invisibly: multiplying
		 * black stays black, and blending black over black is black. */
		if(want_mask) {
			if(win->mask_for != g_crt_mask) {
				sdl_fill_mask(win);
			}
			SDL_RenderTextureTiled(r, win->mask, NULL,
						sdl_mask_scale(win), &dst);
		}
		if(want_scan) {
			if(win->overlay_for != g_scanline_simulator) {
				sdl_fill_overlay(win, g_scanline_simulator);
			}
			SDL_RenderTextureTiled(r, win->overlay, NULL, 1.0f, &dst);
		}
	}
	SDL_SetRenderLogicalPresentation(r, win->width_req, win->main_height,
		g_noaspect ? SDL_LOGICAL_PRESENTATION_STRETCH
			   : SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

static void
sdl_video_init(void)
{
	Kimage	*km;
	int	w, h;

	if(!SDL_Init(SDL_INIT_VIDEO)) {
		printf("SDL_Init failed: %s\n", SDL_GetError());
		exit(1);
	}

	km = video_get_kimage(0);		/* 0 = main window, 1 = debugger */
	w = video_get_x_width(km);
	h = video_get_x_height(km);

	/* Tell the core our pixel layout (ARGB8888 -> 0x00RRGGBB) and build the
	 * palettes. The Apple II text/lores/hires palette (g_a2palette_1624) is
	 * populated ONLY by video_set_palette(); without this call it stays all
	 * zeros and every text/HGR pixel renders black, while super-hires (which
	 * uses g_palette_8to1624 directly) still works. The X11 and Win32 drivers
	 * do exactly this in their own init. */
	video_set_red_mask(0xff0000);
	video_set_green_mask(0x00ff00);
	video_set_blue_mask(0x0000ff);
	/* Mark framebuffer pixels opaque (alpha 0xff), like the mac driver does.
	 * Without this every pixel has alpha 0, which makes the CRT glow pass
	 * (SDL_BLENDMODE_ADD scales source RGB by source alpha) contribute
	 * nothing, silently disabling the bloom half of the CRT effect. */
	video_set_alpha_mask(0xff000000);
	video_set_palette();

	g_mainwin_info.kimage_ptr = km;
	g_mainwin_info.width_req = w;
	g_mainwin_info.main_height = h;
	g_mainwin_info.pixels_per_line = w;
	g_mainwin_info.active = 1;
	g_mainwin_info.data = calloc((size_t)SDL_MAX_WIDTH * SDL_MAX_HEIGHT,
					sizeof(word32));

	video_update_scale(km, w, h, 1);

	/* Window flags and size from the configured display options. */
	SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
	if(g_highdpi)    { flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY; }
	if(g_borderless) { flags |= SDL_WINDOW_BORDERLESS; }
	if(g_fullscreen) { flags |= SDL_WINDOW_FULLSCREEN; g_is_fullscreen = 1; }

	g_mainwin_info.window = SDL_CreateWindow("GSplus",
				w, h, flags);
	if(!g_mainwin_info.window) {
		printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
		exit(1);
	}
	if(!g_fullscreen) {
		SDL_SetWindowPosition(g_mainwin_info.window,
					g_mainwin_xpos, g_mainwin_ypos);
	}

	/* Window/taskbar icon for Linux and Windows. Not on macOS: SDL3's
	 * cocoa backend implements this as setApplicationIconImage, which
	 * would replace the bundle's full-res .icns dock icon with this
	 * small bitmap. */
#ifndef __APPLE__
	{
		SDL_Surface *icon = SDL_CreateSurfaceFrom(gsplus_icon_width,
				gsplus_icon_height, SDL_PIXELFORMAT_RGBA32,
				(void *)gsplus_icon_rgba, gsplus_icon_width * 4);
		if(icon) {
			SDL_SetWindowIcon(g_mainwin_info.window, icon);
			SDL_DestroySurface(icon);
		}
	}
#endif

	/* "-nohwaccel 1" forces the software renderer; otherwise SDL picks the best. */
	g_mainwin_info.renderer = SDL_CreateRenderer(g_mainwin_info.window,
				g_nohwaccel ? "software" : NULL);
	if(!g_mainwin_info.renderer) {
		printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
		exit(1);
	}
	/* Keep vsync OFF: the core paces every frame to the emulated 60.05Hz via
	 * micro_sleep() in run_16ms(), so a vsync-blocked SDL_RenderPresent() would
	 * add a second ~16ms wait per frame, running the whole emulator (and its
	 * audio production) at half speed and starving the audio queue. Making vsync
	 * coexist with the core's pacer is a backlog item; for now the core is the
	 * sole frame-rate authority. */
	SDL_SetRenderVSync(g_mainwin_info.renderer, 0);
	printf("SDL renderer backend: %s\n",
		SDL_GetRendererName(g_mainwin_info.renderer));

	/* Keep the IIgs aspect ratio (letterbox) unless -noaspect stretches to fill. */
	SDL_SetRenderLogicalPresentation(g_mainwin_info.renderer, w, h,
		g_noaspect ? SDL_LOGICAL_PRESENTATION_STRETCH
			   : SDL_LOGICAL_PRESENTATION_LETTERBOX);

	sdl_create_texture(&g_mainwin_info, w, h);

	/* Mark this window active so the core renders into it. The X11 and Win32
	 * drivers do this in their window-create routines; without it kimage->active
	 * stays 0 and video_get_active() makes us skip every frame (black screen). */
	video_set_active(km, 1);

	/* Force the core to output the entire screen on the first frame. Without
	 * this, static content (e.g. the no-ROM config panel) is drawn once during
	 * kegs_init -- before this window/texture existed -- and never produces
	 * dirty rectangles again, leaving the texture black. The X11 and Win32
	 * drivers do the same on window expose. */
	video_set_x_refresh_needed(km, 1);

#ifdef __APPLE__
	/* SDL has now installed its default macOS menu bar. Strip the ⌘ shortcuts
	 * off its items so combos like ⌘W/⌘Q reach the emulated IIgs (which uses
	 * ⌘ as Open-Apple), and move GSplus's own Quit to ⌥⌘Q. */
	sdl_mac_fix_menu();
#endif
}

/* --------------------------------------------------------------------------
 * Keyboard mapping: SDL3 physical scancode -> Apple IIgs ADB raw keycode.
 *
 * We key off SDL_Scancode (the physical key position, layout-independent and
 * stable across SDL versions) rather than the layout-dependent keycode. The
 * ADB codes are exactly those the core expects (taken from xdriver.c's
 * g_x_a2_key_to_xsym table); they feed the same adb_physical_key_update() the
 * X11 and native-mac drivers use.
 *
 * Modifier keys appear twice (left/right) mapped to the same ADB code, matching
 * the IIgs which has a single code per modifier. GUI/⌘ -> Open-Apple (Command,
 * 0x37); Alt/Option -> Option (0x3a) -- consistent on macOS and PC.
 * ------------------------------------------------------------------------- */
struct sdl_key_map {
	SDL_Scancode	sc;
	int		a2;
};

static const struct sdl_key_map g_sdl_key_map[] = {
	{ SDL_SCANCODE_ESCAPE, 0x35 },
	{ SDL_SCANCODE_F1, 0x7a }, { SDL_SCANCODE_F2, 0x78 },
	{ SDL_SCANCODE_F3, 0x63 }, { SDL_SCANCODE_F4, 0x76 },
	{ SDL_SCANCODE_F5, 0x60 }, { SDL_SCANCODE_F6, 0x61 },
	{ SDL_SCANCODE_F7, 0x62 }, { SDL_SCANCODE_F8, 0x64 },
	{ SDL_SCANCODE_F9, 0x65 }, { SDL_SCANCODE_F10, 0x6d },
	{ SDL_SCANCODE_F11, 0x67 }, { SDL_SCANCODE_F12, 0x6f },
	{ SDL_SCANCODE_F13, 0x69 }, { SDL_SCANCODE_F14, 0x6b },
	{ SDL_SCANCODE_F15, 0x71 }, { SDL_SCANCODE_PAUSE, 0x7f },

	{ SDL_SCANCODE_GRAVE, 0x32 },
	{ SDL_SCANCODE_1, 0x12 }, { SDL_SCANCODE_2, 0x13 },
	{ SDL_SCANCODE_3, 0x14 }, { SDL_SCANCODE_4, 0x15 },
	{ SDL_SCANCODE_5, 0x17 }, { SDL_SCANCODE_6, 0x16 },
	{ SDL_SCANCODE_7, 0x1a }, { SDL_SCANCODE_8, 0x1c },
	{ SDL_SCANCODE_9, 0x19 }, { SDL_SCANCODE_0, 0x1d },
	{ SDL_SCANCODE_MINUS, 0x1b }, { SDL_SCANCODE_EQUALS, 0x18 },
	{ SDL_SCANCODE_BACKSPACE, 0x33 },

	{ SDL_SCANCODE_INSERT, 0x72 }, { SDL_SCANCODE_HOME, 0x73 },
	{ SDL_SCANCODE_PAGEUP, 0x74 },

	{ SDL_SCANCODE_TAB, 0x30 },
	{ SDL_SCANCODE_Q, 0x0c }, { SDL_SCANCODE_W, 0x0d },
	{ SDL_SCANCODE_E, 0x0e }, { SDL_SCANCODE_R, 0x0f },
	{ SDL_SCANCODE_T, 0x11 }, { SDL_SCANCODE_Y, 0x10 },
	{ SDL_SCANCODE_U, 0x20 }, { SDL_SCANCODE_I, 0x22 },
	{ SDL_SCANCODE_O, 0x1f }, { SDL_SCANCODE_P, 0x23 },
	{ SDL_SCANCODE_LEFTBRACKET, 0x21 }, { SDL_SCANCODE_RIGHTBRACKET, 0x1e },
	{ SDL_SCANCODE_BACKSLASH, 0x2a },
	{ SDL_SCANCODE_DELETE, 0x75 }, { SDL_SCANCODE_END, 0x77 },
	{ SDL_SCANCODE_PAGEDOWN, 0x79 },

	{ SDL_SCANCODE_CAPSLOCK, 0x39 },
	{ SDL_SCANCODE_A, 0x00 }, { SDL_SCANCODE_S, 0x01 },
	{ SDL_SCANCODE_D, 0x02 }, { SDL_SCANCODE_F, 0x03 },
	{ SDL_SCANCODE_G, 0x05 }, { SDL_SCANCODE_H, 0x04 },
	{ SDL_SCANCODE_J, 0x26 }, { SDL_SCANCODE_K, 0x28 },
	{ SDL_SCANCODE_L, 0x25 }, { SDL_SCANCODE_SEMICOLON, 0x29 },
	{ SDL_SCANCODE_APOSTROPHE, 0x27 }, { SDL_SCANCODE_RETURN, 0x24 },

	{ SDL_SCANCODE_LSHIFT, 0x38 }, { SDL_SCANCODE_RSHIFT, 0x38 },
	{ SDL_SCANCODE_Z, 0x06 }, { SDL_SCANCODE_X, 0x07 },
	{ SDL_SCANCODE_C, 0x08 }, { SDL_SCANCODE_V, 0x09 },
	{ SDL_SCANCODE_B, 0x0b }, { SDL_SCANCODE_N, 0x2d },
	{ SDL_SCANCODE_M, 0x2e }, { SDL_SCANCODE_COMMA, 0x2b },
	{ SDL_SCANCODE_PERIOD, 0x2f }, { SDL_SCANCODE_SLASH, 0x2c },
	{ SDL_SCANCODE_UP, 0x3e },

	{ SDL_SCANCODE_LCTRL, 0x36 }, { SDL_SCANCODE_RCTRL, 0x36 },
	{ SDL_SCANCODE_LALT, 0x3a }, { SDL_SCANCODE_RALT, 0x3a },  /* Option */
	{ SDL_SCANCODE_LGUI, 0x37 }, { SDL_SCANCODE_RGUI, 0x37 },  /* Open-Apple */
	{ SDL_SCANCODE_SPACE, 0x31 },
	{ SDL_SCANCODE_LEFT, 0x3b }, { SDL_SCANCODE_DOWN, 0x3d },
	{ SDL_SCANCODE_RIGHT, 0x3c },

	/* Numeric keypad */
	{ SDL_SCANCODE_NUMLOCKCLEAR, 0x47 }, { SDL_SCANCODE_KP_EQUALS, 0x51 },
	{ SDL_SCANCODE_KP_DIVIDE, 0x4b }, { SDL_SCANCODE_KP_MULTIPLY, 0x43 },
	{ SDL_SCANCODE_KP_7, 0x59 }, { SDL_SCANCODE_KP_8, 0x5b },
	{ SDL_SCANCODE_KP_9, 0x5c }, { SDL_SCANCODE_KP_MINUS, 0x4e },
	{ SDL_SCANCODE_KP_4, 0x56 }, { SDL_SCANCODE_KP_5, 0x57 },
	{ SDL_SCANCODE_KP_6, 0x58 }, { SDL_SCANCODE_KP_PLUS, 0x45 },
	{ SDL_SCANCODE_KP_1, 0x53 }, { SDL_SCANCODE_KP_2, 0x54 },
	{ SDL_SCANCODE_KP_3, 0x55 }, { SDL_SCANCODE_KP_0, 0x52 },
	{ SDL_SCANCODE_KP_PERIOD, 0x41 }, { SDL_SCANCODE_KP_ENTER, 0x4c },

	{ SDL_SCANCODE_UNKNOWN, -1 }		/* terminator */
};

static int
sdl_scancode_to_a2code(SDL_Scancode sc)
{
	int	i;

	for(i = 0; g_sdl_key_map[i].a2 >= 0; i++) {
		if(g_sdl_key_map[i].sc == sc) {
			return g_sdl_key_map[i].a2;
		}
	}
	return -1;
}

/* Translate SDL's modifier state into the IIgs c025 modifier register
 * (bit0 = shift, bit1 = control, bit2 = caps lock), as x_update_modifier_state
 * does for X11. */
static void
sdl_update_modifiers(Window_info *win)
{
	SDL_Keymod mod = SDL_GetModState();
	word32	c025_val = 0;

	if(mod & SDL_KMOD_SHIFT) { c025_val |= 1; }
	if(mod & SDL_KMOD_CTRL)  { c025_val |= 2; }
	if(mod & SDL_KMOD_CAPS)  { c025_val |= 4; }
	adb_update_c025_mask(win->kimage_ptr, c025_val, 7);
}

static void
sdl_handle_key(Window_info *win, SDL_Scancode sc, int is_up, int repeat)
{
	int	a2code;

	if(repeat) {
		return;		/* the IIgs ADB does its own key repeat */
	}
	sdl_update_modifiers(win);

	a2code = sdl_scancode_to_a2code(sc);
	if(a2code >= 0) {
		adb_physical_key_update(win->kimage_ptr, a2code, 0, is_up);
	}
}

/* Mouse button index -> IIgs button mask. SDL buttons are 1=left, 2=middle,
 * 3=right; (1 << b) >> 1 gives left=1, middle=2, right=4 (same as xdriver). */
static int
sdl_button_mask(int sdl_button)
{
	return (1 << sdl_button) >> 1;
}

/* ----------------------------------------------------------------------- */
/* Game controller (joystick / paddle) support.                            */
/*                                                                          */
/* The IIgs sees a 2-axis, 2-button analog joystick on the paddle inputs.   */
/* We drive it from SDL3's high-level Gamepad API, which gives a standard    */
/* layout (left stick + A/B) and a built-in mapping database across         */
/* XInput/DirectInput/HID/Bluetooth pads. The core polls us via             */
/* joystick_update() whenever a program reads the paddle trigger ($C070);   */
/* we just sample current state, which SDL keeps fresh because              */
/* sdl_poll_events() pumps the event queue every frame (and handles         */
/* hotplug). These three entry points replace the native IOKit/joydev/      */
/* mmsystem backends in joystick_driver.c, which is compiled out for the    */
/* SDL build (see SDL_INPUT in CMakeLists.txt). The user still picks        */
/* "Native Joystick 1" in the config menu (F4) to route paddles here.       */

extern int g_joystick_native_type1;	/* paddles.c: -1 = no joystick present */
extern int g_paddle_buttons;		/* paddles.c: bits 0,1 = buttons 0,1 */
extern int g_paddle_val[4];		/* paddles.c: [0]=X [1]=Y, -32768..32767 */

static SDL_Gamepad *g_sdl_gamepad = NULL;	/* the open controller, or NULL */

/* Open the first connected controller SDL has a gamepad mapping for. */
static void
sdl_open_first_gamepad(void)
{
	SDL_JoystickID *ids;
	int	count, i;

	if(g_sdl_gamepad) {
		return;				/* already have one */
	}
	ids = SDL_GetGamepads(&count);
	if(!ids) {
		return;
	}
	for(i = 0; i < count; i++) {
		if(!SDL_IsGamepad(ids[i])) {
			continue;
		}
		g_sdl_gamepad = SDL_OpenGamepad(ids[i]);
		if(g_sdl_gamepad) {
			g_joystick_native_type1 = 1;
			printf("SDL gamepad opened: %s\n",
				SDL_GetGamepadName(g_sdl_gamepad));
			break;
		}
	}
	SDL_free(ids);
}

/* Read the two face buttons into the low bits of g_paddle_buttons. */
void
joystick_update_buttons(void)
{
	int	buttons;

	if(!g_sdl_gamepad) {
		return;
	}
	buttons = 0;
	if(SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_SOUTH)) {
		buttons |= 1;		/* button 0 (e.g. A) */
	}
	if(SDL_GetGamepadButton(g_sdl_gamepad, SDL_GAMEPAD_BUTTON_EAST)) {
		buttons |= 2;		/* button 1 (e.g. B) */
	}
	g_paddle_buttons = (g_paddle_buttons & ~3) | buttons;
}

/* Sample axes + buttons into the paddle globals. Called from paddles.c. */
void
joystick_update(dword64 dfcyc)
{
	int	i;

	/* Default: centered, both buttons up. 0xc keeps the unused upper
	 * paddle buttons high, matching the native backends. */
	for(i = 0; i < 4; i++) {
		g_paddle_val[i] = 32767;
	}
	g_paddle_buttons = 0xc;

	if(!g_sdl_gamepad) {
		return;
	}
	/* SDL gamepad axes are already Sint16 (-32768..32767), exactly the
	 * paddle range the core expects. */
	g_paddle_val[0] = SDL_GetGamepadAxis(g_sdl_gamepad,
						SDL_GAMEPAD_AXIS_LEFTX);
	g_paddle_val[1] = SDL_GetGamepadAxis(g_sdl_gamepad,
						SDL_GAMEPAD_AXIS_LEFTY);
	joystick_update_buttons();
	paddle_update_trigger_dcycs(dfcyc);
}

/* Called once from kegs_init() (before sdl_video_init's SDL_Init). */
void
joystick_init(void)
{
	g_joystick_native_type1 = -1;
	if(!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
		printf("SDL_INIT_GAMEPAD failed: %s\n", SDL_GetError());
		return;
	}
	sdl_open_first_gamepad();
}

/* Apply the g_fullscreen config var to the live window whenever it changes.
 * Both F11 and the F4 config menu write g_fullscreen; this keeps the actual
 * window state (g_is_fullscreen) in sync with it, so toggling the option in
 * the config panel takes effect immediately, not just on the next launch. */
static void
sdl_sync_fullscreen(Window_info *win)
{
	int	want = g_fullscreen ? 1 : 0;

	if(want != g_is_fullscreen) {
		g_is_fullscreen = want;
		SDL_SetWindowFullscreen(win->window, want);
	}
}

/* Hide the host OS cursor while it is over our content -- always in fullscreen,
 * and over the window in windowed mode -- unless the user turned it off via the
 * "Hide Mouse Cursor" config option (g_hide_mouse). SDL cursor visibility is
 * app-wide but only manifests over our window, so when the pointer leaves a
 * windowed-mode window we restore it for the rest of the desktop. The early-out
 * keeps this cheap to call every frame, so it also picks up g_hide_mouse and
 * fullscreen changes made from the F4 config menu. */
static void
sdl_update_cursor_visibility(void)
{
	int	want_hidden = g_hide_mouse &&
				(g_is_fullscreen || g_mouse_over_window);

	if(want_hidden == g_cursor_hidden) {
		return;
	}
	g_cursor_hidden = want_hidden;
	if(want_hidden) {
		SDL_HideCursor();
	} else {
		SDL_ShowCursor();
	}
}

static void
sdl_poll_events(void)
{
	SDL_Event ev;
	Window_info *win = &g_mainwin_info;
	int	mask, mx, my;

	/* Pick up fullscreen / hide-cursor changes made from the F4 config menu. */
	sdl_sync_fullscreen(win);
	sdl_update_cursor_visibility();

	while(SDL_PollEvent(&ev)) {
		switch(ev.type) {
		case SDL_EVENT_QUIT:
			g_quit_requested = 1;
			break;
		case SDL_EVENT_WINDOW_EXPOSED:
		case SDL_EVENT_WINDOW_SHOWN:
		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		case SDL_EVENT_WINDOW_RESTORED:
			/* Repaint the whole screen when the window (re)appears or resizes. */
			video_set_x_refresh_needed(g_mainwin_info.kimage_ptr, 1);
			break;
		case SDL_EVENT_WINDOW_MOUSE_ENTER:
			g_mouse_over_window = 1;
			sdl_update_cursor_visibility();
			break;
		case SDL_EVENT_WINDOW_MOUSE_LEAVE:
			g_mouse_over_window = 0;
			sdl_update_cursor_visibility();
			break;
		case SDL_EVENT_DROP_FILE:
			/* Drop a disk image on the window to mount it (slot guessed
			 * from the file size). ev.drop.data is owned by SDL. */
			if(ev.drop.data) {
				cfg_inspect_maybe_insert_file(ev.drop.data);
				video_set_x_refresh_needed(win->kimage_ptr, 1);
			}
			break;
		case SDL_EVENT_KEY_DOWN:
			/* F10 saves a screenshot (gsplus convention). F10 and F11 are
			 * the only function keys KEGS leaves unused, so they're safe to
			 * claim without shadowing an emulator hotkey (notably F12 =
			 * reset). Not sent to the IIgs. */
			if(ev.key.scancode == SDL_SCANCODE_F10) {
				if(!ev.key.repeat) {
					g_screenshot_requested = 1;
				}
				break;
			}
			/* F11 toggles fullscreen; Shift+F11 toggles scanlines;
			 * Ctrl+F11 toggles the curved CRT effect (gsplus
			 * convention). None is sent to the IIgs. */
			if(ev.key.scancode == SDL_SCANCODE_F11) {
				if(!ev.key.repeat) {
					if(SDL_GetModState() & SDL_KMOD_CTRL) {
						g_crt = !g_crt;
					} else if(SDL_GetModState() & SDL_KMOD_SHIFT) {
						if(g_scanline_simulator > 0) {
							g_scanline_saved = g_scanline_simulator;
							g_scanline_simulator = 0;
						} else {
							g_scanline_simulator = g_scanline_saved;
						}
					} else {
						/* Toggle the config var and apply it,
						 * so F11 and the config menu agree. */
						g_fullscreen = !g_is_fullscreen;
						sdl_sync_fullscreen(win);
					}
				}
				break;
			}
			sdl_handle_key(win, ev.key.scancode, 0, ev.key.repeat);
			break;
		case SDL_EVENT_KEY_UP:
			if(ev.key.scancode == SDL_SCANCODE_F11) {
				break;
			}
			/* Swallow the matching F10 release so the IIgs never sees a
			 * stray F10 key-up from a screenshot press. */
			if(ev.key.scancode == SDL_SCANCODE_F10) {
				break;
			}
			sdl_handle_key(win, ev.key.scancode, 1, 0);
			break;
		case SDL_EVENT_MOUSE_MOTION:
			/* Map window coords through the renderer's logical
			 * presentation, then into the IIgs framebuffer. */
			SDL_ConvertEventToRenderCoordinates(win->renderer, &ev);
			mx = video_scale_mouse_x(win->kimage_ptr, (int)ev.motion.x, 0);
			my = video_scale_mouse_y(win->kimage_ptr, (int)ev.motion.y, 0);
			adb_update_mouse(win->kimage_ptr, mx, my, 0, 0);
			/* Motion means the pointer is over us; covers the case where
			 * the window opened under the cursor with no ENTER event. */
			if(!g_mouse_over_window) {
				g_mouse_over_window = 1;
				sdl_update_cursor_visibility();
			}
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
			SDL_ConvertEventToRenderCoordinates(win->renderer, &ev);
			mask = sdl_button_mask(ev.button.button);
			mx = video_scale_mouse_x(win->kimage_ptr, (int)ev.button.x, 0);
			my = video_scale_mouse_y(win->kimage_ptr, (int)ev.button.y, 0);
			adb_update_mouse(win->kimage_ptr, mx, my,
				(ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN) ? mask : 0,
				mask & 7);
			break;
		case SDL_EVENT_GAMEPAD_ADDED:
			/* A controller was plugged in; adopt it if we have none. */
			sdl_open_first_gamepad();
			break;
		case SDL_EVENT_GAMEPAD_REMOVED:
			/* If the pad we were using went away, drop it and try to
			 * fall back to any other still-connected controller. */
			if(g_sdl_gamepad &&
					SDL_GetGamepadID(g_sdl_gamepad) ==
							ev.gdevice.which) {
				SDL_CloseGamepad(g_sdl_gamepad);
				g_sdl_gamepad = NULL;
				g_joystick_native_type1 = -1;
				sdl_open_first_gamepad();
			}
			break;
		default:
			break;
		}
	}
}

/* Build "[<ssdir>/]gsplus_screenshot_YYYYMMDD_HHMMSS.png" into buf. With no
 * ssdir configured the file lands in the current working directory. */
static void
sdl_build_screenshot_path(char *buf, size_t buflen)
{
	char	fname[64];
	time_t	now;
	struct tm *tmv;

	now = time(NULL);
	tmv = localtime(&now);
	strftime(fname, sizeof(fname),
			"gsplus_screenshot_%Y%m%d_%H%M%S.png", tmv);

	if(g_cfg_ssdir && g_cfg_ssdir[0]) {
		size_t len = strlen(g_cfg_ssdir);
		const char *sep = (g_cfg_ssdir[len - 1] == '/') ? "" : "/";
		snprintf(buf, buflen, "%s%s%s", g_cfg_ssdir, sep, fname);
	} else {
		snprintf(buf, buflen, "%s", fname);
	}
}

/* Grab the currently-rendered frame (post-scaling, including any scanline
 * overlay and letterbox borders) and write it to a PNG. Called after the frame
 * is drawn but before SDL_RenderPresent, so the back buffer is still valid. */
static void
sdl_save_screenshot(Window_info *win)
{
	SDL_Surface *shot, *conv;
	unsigned char *packed;
	char	path[1100];
	size_t	rowbytes;
	int	w, h, y, rc;

	shot = SDL_RenderReadPixels(win->renderer, NULL);
	if(!shot) {
		printf("Screenshot failed: SDL_RenderReadPixels: %s\n",
			SDL_GetError());
		return;
	}
	/* Normalise to byte-order RGBA, which is what write_png_rgba() expects. */
	conv = SDL_ConvertSurface(shot, SDL_PIXELFORMAT_RGBA32);
	SDL_DestroySurface(shot);
	if(!conv) {
		printf("Screenshot failed: SDL_ConvertSurface: %s\n",
			SDL_GetError());
		return;
	}

	/* SDL surfaces may pad rows (pitch >= w*4); pack them tightly. */
	w = conv->w;
	h = conv->h;
	rowbytes = (size_t)w * 4;
	packed = malloc(rowbytes * (size_t)h);
	if(!packed) {
		SDL_DestroySurface(conv);
		return;
	}
	for(y = 0; y < h; y++) {
		unsigned char *dst = packed + (size_t)y * rowbytes;
		memcpy(dst, (unsigned char *)conv->pixels
				+ (size_t)y * conv->pitch, rowbytes);
		/* The renderer's back buffer can carry a non-opaque alpha, which
		 * makes viewers composite the PNG over white and wash the colors
		 * out. A screenshot is opaque, so force every alpha byte to 0xff. */
		for(int x = 0; x < w; x++) {
			dst[x * 4 + 3] = 0xff;
		}
	}
	SDL_DestroySurface(conv);

	sdl_build_screenshot_path(path, sizeof(path));
	rc = write_png_rgba(path, packed, w, h);
	free(packed);

	if(rc) {
		printf("Screenshot failed: could not write %s\n", path);
	} else {
		printf("Screenshot saved: %s (%dx%d)\n", path, w, h);
	}
}

/* Linear blur, horizontal (g_hblur) and/or vertical (g_vblur). Each is 0-100 and
 * maps to a blur radius of up to BLUR_MAX_RADIUS source pixels (so 100 spreads
 * each pixel into ~2.5 of its neighbours along that axis, simulating composite-
 * video softness). The kernel is a normalized triangle (tent) of integer-offset
 * taps; a box would look flatter and a gaussian needs more taps for no visible
 * gain at this radius. */
#define BLUR_MAX_RADIUS		2.5f	/* source-pixel blur radius at blur==100 */
#define BLUR_MAX_HALF		3	/* max taps per side (ceil of MAX_RADIUS) */

/* Build a normalized 1D tent kernel for a 0-100 blur amount. Fills
 * weight[0..2*half] over offsets -half..+half so the taps sum to 1, and returns
 * half (0 when blur is off, i.e. a single implied centre tap of weight 1). */
static int
sdl_blur_kernel(int blur, float *weight)
{
	float	radius, base, sum;
	int	k, half;

	if(blur <= 0) {
		weight[0] = 1.0f;
		return 0;
	}
	radius = (float)blur / 100.0f * BLUR_MAX_RADIUS;
	half = (int)radius;
	if(radius > (float)half) { half++; }		/* ceil */
	if(half > BLUR_MAX_HALF) { half = BLUR_MAX_HALF; }
	base = radius + 1.0f;				/* tent half-base; radius 0 => identity */

	sum = 0.0f;
	for(k = -half; k <= half; k++) {
		float wt = 1.0f - (float)(k < 0 ? -k : k) / base;
		if(wt < 0.0f) { wt = 0.0f; }
		weight[k + half] = wt;
		sum += wt;
	}
	for(k = 0; k <= 2 * half; k++) { weight[k] /= sum; }	/* normalize to sum 1 */
	return half;
}

/* Draw win->texture into the current render target, filling the w x h logical
 * area, with the horizontal/vertical blur applied when g_hblur/g_vblur > 0. The
 * taps are summed with additive blending and their weights sum to 1, so the
 * result is a weighted average -- which requires the target to already be cleared
 * to black (both call sites do). With both blurs == 0 this is just the normal
 * crisp 1:1 copy. */
static void
sdl_render_framebuffer(Window_info *win, int w, int h)
{
	SDL_Renderer	*r = win->renderer;
	SDL_FRect	dst;
	float		hweight[2 * BLUR_MAX_HALF + 1];
	float		vweight[2 * BLUR_MAX_HALF + 1];
	int		kx, ky, hhalf, vhalf, mod;
	SDL_BlendMode	add;

	hhalf = sdl_blur_kernel(g_hblur, hweight);
	vhalf = sdl_blur_kernel(g_vblur, vweight);

	if((hhalf == 0) && (vhalf == 0)) {
		/* No blur: opaque nearest-neighbour copy, as before. */
		SDL_RenderTexture(r, win->texture, NULL, NULL);
		return;
	}

	/* Accumulate the weighted, shifted taps additively. We can't use
	 * SDL_BLENDMODE_ADD here: it scales the source by its alpha (dstRGB =
	 * srcRGB*srcA + dstRGB), and the framebuffer texture's alpha reaches the
	 * blender as 0, which zeroes every tap and leaves a black screen. This custom
	 * mode adds the colour-modulated source RGB directly (factor ONE, source alpha
	 * ignored) and leaves the destination alpha untouched, so an offscreen target
	 * stays opaque for any later sampling (e.g. the CRT glow pass).
	 *
	 * Keep the texture at NEAREST: the taps are whole-source-pixel shifts, so the
	 * blur comes entirely from summing the shifted copies. Switching to LINEAR
	 * would also let the upscale-to-window bilinear-filter the image, mushing it
	 * in both axes regardless of which blur is on. The shifts kx/ky are in source
	 * pixels (dst is the same logical space the plain copy fills). */
	add = SDL_ComposeCustomBlendMode(
			SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE,
			SDL_BLENDOPERATION_ADD,
			SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE,
			SDL_BLENDOPERATION_ADD);
	SDL_SetTextureBlendMode(win->texture, add);
	dst.w = (float)w;
	dst.h = (float)h;

	/* Separable 2D tent done in a single accumulation step: each (kx,ky) tap gets
	 * weight hweight[kx]*vweight[ky]. Whichever axis is off has half==0 and one
	 * unit-weight centre tap, so a one-axis blur costs exactly what it did before
	 * and enabling both gives the outer product of the two 1D kernels. */
	for(ky = -vhalf; ky <= vhalf; ky++) {
		float vw = vweight[ky + vhalf];
		dst.y = (float)ky;
		for(kx = -hhalf; kx <= hhalf; kx++) {
			mod = (int)(hweight[kx + hhalf] * vw * 255.0f + 0.5f);
			SDL_SetTextureColorMod(win->texture, (Uint8)mod, (Uint8)mod,
						(Uint8)mod);
			dst.x = (float)kx;
			SDL_RenderTexture(r, win->texture, NULL, &dst);
		}
	}

	/* Restore the texture's normal state for any later/non-blur use. */
	SDL_SetTextureColorMod(win->texture, 255, 255, 255);
	SDL_SetTextureBlendMode(win->texture, SDL_BLENDMODE_NONE);
}

/* Render the framebuffer with the full curved-CRT effect. Assumes the changed
 * rectangles have already been uploaded into win->texture. Three passes:
 *   1. copy the (optionally blurred) framebuffer into crt_target (no curvature),
 *   2. downscale crt_target into glow_target (a cheap blur for bloom),
 *   3. warp crt_target onto the curvature mesh in the window, add the glow,
 *      then draw the mask + scanlines over the result in host-pixel space.
 * Logical presentation is disabled for the offscreen passes (we want a 1:1 fill)
 * and restored for the final on-window geometry pass. */
static void
sdl_render_crt(Window_info *win)
{
	SDL_Renderer *r = win->renderer;
	int	w = win->width_req;
	int	h = win->main_height;

	/* Rebuild the mesh if the curvature or vignette knob changed at runtime. */
	if((win->crt_curve_for != g_crt_curve) ||
				(win->crt_vig_for != g_crt_vignette)) {
		sdl_build_crt_mesh(win, w, h);
		if(!win->crt_verts) {
			return;
		}
	}

	/* --- Pass 1: copy into crt_target (1:1, no logical scaling). --- */
	SDL_SetRenderLogicalPresentation(r, 0, 0,
				SDL_LOGICAL_PRESENTATION_DISABLED);
	SDL_SetRenderTarget(r, win->crt_target);
	/* Clear to transparent black first: the blur path accumulates additively, so
	 * it needs a cleared target. Crucially the alpha is 0, not 255: without blur,
	 * crt_target is a plain (NONE) copy of the framebuffer texture, whose alpha is
	 * 0 -- which leaves the later glow pass (it ADD-blends scaled by source alpha)
	 * dormant. Clearing to opaque here would instead switch the glow on only when
	 * blur is enabled, washing the picture out. Matching alpha 0 keeps the CRT look
	 * identical with blur on or off. */
	SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
	SDL_RenderClear(r);
	sdl_render_framebuffer(win, w, h);

	/* --- Pass 2: downscale crt_target -> glow_target (LINEAR = blur). --- */
	SDL_SetRenderTarget(r, win->glow_target);
	SDL_RenderTexture(r, win->crt_target, NULL, NULL);

	/* --- Pass 3: warp onto the curve in the window, then add the glow. --- */
	SDL_SetRenderTarget(r, NULL);
	SDL_SetRenderLogicalPresentation(r, w, h,
		g_noaspect ? SDL_LOGICAL_PRESENTATION_STRETCH
			   : SDL_LOGICAL_PRESENTATION_LETTERBOX);
	SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
	SDL_RenderClear(r);

	SDL_SetTextureBlendMode(win->crt_target, SDL_BLENDMODE_NONE);
	SDL_SetTextureColorMod(win->crt_target, 255, 255, 255);
	SDL_RenderGeometry(r, win->crt_target, win->crt_verts, win->crt_nverts,
				win->crt_idx, win->crt_nidx);

	SDL_SetTextureBlendMode(win->glow_target, SDL_BLENDMODE_ADD);
	SDL_SetTextureColorMod(win->glow_target, CRT_GLOW_LEVEL, CRT_GLOW_LEVEL,
				CRT_GLOW_LEVEL);
	SDL_RenderGeometry(r, win->glow_target, win->crt_verts, win->crt_nverts,
				win->crt_idx, win->crt_nidx);

	/* Mask and scanlines go on last, in host-pixel space over the warped
	 * image, so they stay at fixed screen-pixel sizes regardless of window
	 * size. (They no longer feed the glow pass; the bloom now comes from
	 * the clean picture, which reads a touch smoother.) */
	sdl_render_overlays(win);
}

static void
sdl_update_display(Window_info *win)
{
	Change_rect rect;
	int	i, w, h;
	word32	*src;
	SDL_Rect r;

	if(!win->renderer) {
		return;
	}
	if(!video_get_active(win->kimage_ptr)) {
		return;
	}

	/* If the IIgs changed video mode, the logical size changes: resize the
	 * window and rebuild the texture to match. */
	if(video_change_aspect_needed(win->kimage_ptr, win->width_req,
						win->main_height)) {
		w = video_get_x_width(win->kimage_ptr);
		h = video_get_x_height(win->kimage_ptr);
		win->width_req = w;
		win->main_height = h;
		win->pixels_per_line = w;
		video_update_scale(win->kimage_ptr, w, h, 1);
		SDL_SetWindowSize(win->window, w, h);
		/* Honor -noaspect here too, matching init and the CRT path; otherwise
		 * a stretched view reverts to letterbox after the first mode change. */
		SDL_SetRenderLogicalPresentation(win->renderer, w, h,
			g_noaspect ? SDL_LOGICAL_PRESENTATION_STRETCH
				   : SDL_LOGICAL_PRESENTATION_LETTERBOX);
		sdl_create_texture(win, w, h);
		/* The new texture starts out blank, so ask the core to re-emit
		 * the whole screen rather than just the next frame's deltas. */
		video_set_x_refresh_needed(win->kimage_ptr, 1);
	}

	/* Ask the core for each changed rectangle (it writes pixels into our
	 * buffer) and upload that rectangle to the texture. */
	for(i = 0; i < MAX_CHANGE_RECTS; i++) {
		if(!video_out_data(win->data, win->kimage_ptr,
					win->pixels_per_line, &rect, i)) {
			break;
		}
		r.x = rect.x;
		r.y = rect.y;
		r.w = rect.width;
		r.h = rect.height;
		src = win->data + (size_t)rect.y * win->pixels_per_line + rect.x;
		SDL_UpdateTexture(win->texture, &r, src,
				win->pixels_per_line * (int)sizeof(word32));
	}

	if(g_crt && win->crt_target && win->glow_target && win->crt_verts) {
		/* Full curved-CRT path (curvature + mask + glow + vignette). */
		sdl_render_crt(win);
	} else {
		/* Plain path: framebuffer straight to the window (with optional
		 * blur), scanlines drawn over it in host-pixel space. Clear to
		 * black explicitly since the blur path accumulates additively. */
		SDL_SetRenderDrawColor(win->renderer, 0, 0, 0, 255);
		SDL_RenderClear(win->renderer);
		sdl_render_framebuffer(win, win->width_req, win->main_height);
		sdl_render_overlays(win);
	}

	/* Service a pending Shift+F12 capture now, while the just-drawn frame is
	 * still in the back buffer (RenderPresent may invalidate it). */
	if(g_screenshot_requested) {
		sdl_save_screenshot(win);
		g_screenshot_requested = 0;
	}

	SDL_RenderPresent(win->renderer);
}

/* --------------------------------------------------------------------------
 * Terminal debugger REPL.
 *
 * The core's built-in 65816 monitor (debugger.c) already writes all its output
 * to stdout. What it lacked under SDL was a way to receive input -- it used to
 * read keystrokes from a dedicated debugger window. Rather than rebuild a
 * terminal inside SDL (scrollback, copy/paste, history are all hard), we drive
 * the monitor straight from the launching terminal, which already has them.
 *
 * A reader thread blocks on stdin so the SDL main loop never stalls; complete
 * lines land in a small queue that sdl_debugger_poll() drains while the CPU is
 * halted (middle-click break, or the F7 toggle). Launched with no terminal
 * (double-clicked .app/.exe), stdin hits EOF and the thread simply exits -- the
 * emulator runs normally, just without an interactive debugger.
 * ------------------------------------------------------------------------- */

#define DBG_LINE_MAX	256
#define DBG_QUEUE_LEN	32

static char g_dbg_queue[DBG_QUEUE_LEN][DBG_LINE_MAX];
static int g_dbg_q_head = 0;		/* next slot to read (main thread)   */
static int g_dbg_q_tail = 0;		/* next slot to write (reader thread) */
static SDL_Mutex *g_dbg_mutex = NULL;
static int g_dbg_prompt_shown = 0;

/* Reader thread: block on stdin, push each completed line onto the queue. */
static int SDLCALL
sdl_stdin_reader(void *data)
{
	char	line[DBG_LINE_MAX];
	int	next;

	(void)data;
	while(fgets(line, sizeof(line), stdin)) {
		SDL_LockMutex(g_dbg_mutex);
		next = (g_dbg_q_tail + 1) % DBG_QUEUE_LEN;
		if(next != g_dbg_q_head) {		/* silently drop if full */
			SDL_strlcpy(g_dbg_queue[g_dbg_q_tail], line, DBG_LINE_MAX);
			g_dbg_q_tail = next;
		}
		SDL_UnlockMutex(g_dbg_mutex);
	}
	return 0;		/* EOF: no terminal attached, or input closed */
}

/* Pop one queued line into out[DBG_LINE_MAX], stripping the trailing newline.
 * Returns 1 if a line was available, 0 otherwise. */
static int
sdl_dbg_dequeue(char *out)
{
	int	got = 0, i;

	SDL_LockMutex(g_dbg_mutex);
	if(g_dbg_q_head != g_dbg_q_tail) {
		SDL_strlcpy(out, g_dbg_queue[g_dbg_q_head], DBG_LINE_MAX);
		g_dbg_q_head = (g_dbg_q_head + 1) % DBG_QUEUE_LEN;
		got = 1;
	}
	SDL_UnlockMutex(g_dbg_mutex);
	if(got) {
		for(i = 0; out[i]; i++) {
			if((out[i] == '\n') || (out[i] == '\r')) {
				out[i] = 0;
				break;
			}
		}
	}
	return got;
}

static void
sdl_dbg_prompt(void)
{
	printf("gsplus> ");
	fflush(stdout);
	g_dbg_prompt_shown = 1;
}

/* Create the stdin reader thread. Non-fatal on failure: the emulator still
 * runs, just without terminal debugger input. */
static void
sdl_debugger_init(void)
{
	SDL_Thread *thread;

	g_dbg_mutex = SDL_CreateMutex();
	if(!g_dbg_mutex) {
		printf("Debugger: SDL_CreateMutex failed: %s\n", SDL_GetError());
		return;
	}
	thread = SDL_CreateThread(sdl_stdin_reader, "stdin-reader", NULL);
	if(!thread) {
		printf("Debugger: SDL_CreateThread failed: %s\n", SDL_GetError());
		return;
	}
	SDL_DetachThread(thread);	/* fire-and-forget; dies with the process */
}

/* Drive the terminal monitor: prompt once on halt, then feed queued lines to
 * the core's command parser. Called once per frame from the main loop. */
static void
sdl_debugger_poll(void)
{
	char	line[DBG_LINE_MAX];

	if(!g_dbg_mutex || !g_halt_sim) {
		g_dbg_prompt_shown = 0;		/* running: re-prompt on next halt */
		return;
	}
	if(!g_dbg_prompt_shown) {
		printf("\n[debugger] CPU halted -- type 'h' for help, 'g' to "
							"continue\n");
		sdl_dbg_prompt();
	}
	/* The terminal echoes what the user types; do_debug_cmd() echoes the
	 * command and prints its output. Stop if a command (g/s) resumes the CPU. */
	while(g_halt_sim && sdl_dbg_dequeue(line)) {
		do_debug_cmd(line);
		if(g_halt_sim) {
			sdl_dbg_prompt();
		} else {
			g_dbg_prompt_shown = 0;
		}
	}
}

int
main(int argc, char **argv)
{
	int	mdepth = 32;		/* ARGB8888 -> 32-bit pixels */
	int	ret;

	printf("GSplus %s (SDL3)\n", GSPLUS_VERSION_STR);

	ret = parse_argv(argc, argv, 1);
	if(ret) {
		printf("parse_argv ret: %d, stopping\n", ret);
		exit(1);
	}

	ret = kegs_init(mdepth, SDL_MAX_WIDTH, SDL_MAX_HEIGHT, 0);
	if(ret) {
		printf("kegs_init ret: %d, stopping\n", ret);
		exit(1);
	}

	sdl_video_init();
	sdl_debugger_init();

	/* Main loop: run_16ms() runs one video frame's worth of CPU + video. */
	while(!g_quit_requested) {
		ret = run_16ms();
		if(ret != 0) {
			printf("run_16ms returned: %d\n", ret);
			break;
		}
		sdl_poll_events();
		sdl_debugger_poll();
		sdl_update_display(&g_mainwin_info);
	}

	sdl_snd_shutdown();
	SDL_Quit();
	return 0;
}
