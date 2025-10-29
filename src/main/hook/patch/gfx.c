#define LOG_MODULE "patch-gfx"

#include <GL/gl.h>
#include <X11/Xutil.h>
#include <stdbool.h>

// OpenGL 3.x+
#include <GL/glx.h>

#include "capnhook/hook/lib.h"

#include "util/log.h"
#include "util/str.h"

#include "gfx.h"

typedef Window (*XCreateWindow_t)(
    Display *display,
    Window parent,
    int x,
    int y,
    unsigned int width,
    unsigned int height,
    unsigned int border_width,
    int depth,
    unsigned int _class,
    Visual *visual,
    unsigned long valuemask,
    XSetWindowAttributes *attributes);
typedef Display *(*XOpenDisplay_t)(const char *display_name);
typedef XVisualInfo *(*glXChooseVisual_t)(
    Display *dpy, int screen, int *attribList);

// OpenGL function types for scaling
typedef void (*glViewport_t)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (*glMatrixMode_t)(GLenum mode);
typedef void (*glLoadIdentity_t)(void);
typedef void (*glOrtho_t)(GLdouble left, GLdouble right, GLdouble bottom, 
                         GLdouble top, GLdouble near, GLdouble far);
typedef void (*glScalef_t)(GLfloat x, GLfloat y, GLfloat z);
typedef void (*glPushMatrix_t)(void);
typedef void (*glPopMatrix_t)(void);
typedef void (*glClear_t)(GLbitfield mask);
typedef void (*glClearColor_t)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);

static bool patch_gfx_initialized;
static XCreateWindow_t patch_gfx_real_XCreateWindow;
static XOpenDisplay_t patch_gfx_real_XOpenDisplay;
static glXChooseVisual_t patch_gfx_real_GlXChooseVisual;

// OpenGL function pointers for scaling
static glViewport_t patch_gfx_real_glViewport;
static glMatrixMode_t patch_gfx_real_glMatrixMode;
static glLoadIdentity_t patch_gfx_real_glLoadIdentity;
static glOrtho_t patch_gfx_real_glOrtho;
static glScalef_t patch_gfx_real_glScalef;
static glPushMatrix_t patch_gfx_real_glPushMatrix;
static glPopMatrix_t patch_gfx_real_glPopMatrix;
static glClear_t patch_gfx_real_glClear;
static glClearColor_t patch_gfx_real_glClearColor;

// Scaling state
static enum patch_gfx_scale_mode patch_gfx_current_scale_mode = PATCH_GFX_SCALE_MODE_INVALID;
static bool patch_gfx_scaling_active = false;

static char *patch_gfx_attrib_list_to_str(int *attrib_list)
{
  int *ptr = attrib_list;
  char *str = util_str_dup("");
  char buf[32];

  while (*ptr != None) {
    sprintf(buf, "%d, ", *ptr);

    char *tmp = util_str_merge(str, buf);

    free(str);
    str = tmp;

    ptr++;
  }

  return str;
}

static void patch_gfx_get_scale_params(enum patch_gfx_scale_mode scale_mode, 
                                       int *src_width, int *src_height,
                                       int *dst_width, int *dst_height)
{
  switch (scale_mode) {
    case PATCH_GFX_SCALE_MODE_SD_480_TO_PILLARBOX_HD_720:
      *src_width = 640;
      *src_height = 480;
      *dst_width = 1280;
      *dst_height = 720;
      break;
    case PATCH_GFX_SCALE_MODE_SD_480_TO_PILLARBOX_HD_1080:
      *src_width = 640;
      *src_height = 480;
      *dst_width = 1920;
      *dst_height = 1080;
      break;
    case PATCH_GFX_SCALE_MODE_SD_480_TO_SD_960:
      *src_width = 640;
      *src_height = 480;
      *dst_width = 1280;
      *dst_height = 960;
      break;
    case PATCH_GFX_SCALE_MODE_HD_720_TO_HD_1080:
      *src_width = 1280;
      *src_height = 720;
      *dst_width = 1920;
      *dst_height = 1080;
      break;
    default:
      *src_width = 640;
      *src_height = 480;
      *dst_width = 640;
      *dst_height = 480;
      break;
  }
}

Window XCreateWindow(
    Display *display,
    Window parent,
    int x,
    int y,
    unsigned int width,
    unsigned int height,
    unsigned int border_width,
    int depth,
    unsigned int _class,
    Visual *visual,
    unsigned long valuemask,
    XSetWindowAttributes *attributes)
{
  log_debug("XCreateWindow");

  if (!patch_gfx_real_XCreateWindow) {
    patch_gfx_real_XCreateWindow =
        (XCreateWindow_t) cnh_lib_get_func_addr("XCreateWindow");
  }

  if (patch_gfx_initialized) {
    // During early Exceed-Era code, CWColorMap was not included as a valuemask for creation of a window.
    // Eventually, this was brought back, but it's suspected that NVIDIA cards, drivers at the time, or 
    // perhaps even a combination of that and X11 supported querying the gpu itself for a colormap. 
    // Either way, this no longer works consistently and we need to account for that.
    if ((valuemask & CWColormap) == 0) {
      log_info("Adding CWColorMap to ValueMask for Non-NVIDIA Cards.");
      /* enables usage of nvidia cards where hardware colormaps are not available or supported */
      valuemask |= CWColormap;
    }
  }

  return patch_gfx_real_XCreateWindow(
      display,
      parent,
      x,
      y,
      width,
      height,
      border_width,
      depth,
      _class,
      visual,
      valuemask,
      attributes);
}

Display *XOpenDisplay(const char *display_name)
{
  if (!patch_gfx_real_XOpenDisplay) {
    patch_gfx_real_XOpenDisplay =
        (XOpenDisplay_t) cnh_lib_get_func_addr("XOpenDisplay");
  }

  log_info("XOpenDisplay: %s", display_name);

  Display *res = patch_gfx_real_XOpenDisplay(display_name);

  if (!res) {
    log_error(
        "XOpenDisplay returned NULL. This might indicate that your environment "
        "is not properly setup. Check "
        " that you have GPU drivers installed and configured properly and that "
        "OpenGL hardware acceleration is "
        " working (use the command line tool \"glxinfo\").");
  }

  return res;
}

XVisualInfo *glXChooseVisual(Display *dpy, int screen, int *attribList)
{
  if (!patch_gfx_real_GlXChooseVisual) {
    patch_gfx_real_GlXChooseVisual =
        (glXChooseVisual_t) cnh_lib_get_func_addr("glXChooseVisual");
  }

  char *attrib_list_str = patch_gfx_attrib_list_to_str(attribList);

  log_info(
      "glXChooseVisual: dpy %p, screen %d, attribList %s",
      dpy,
      screen,
      attrib_list_str);

  free(attrib_list_str);

  XVisualInfo *res = patch_gfx_real_GlXChooseVisual(dpy, screen, attribList);

  if (!res) {
    log_error(
        "glXChooseVisual returned NULL. It is likely that your current "
        "environment does support hardware"
        "acceleration. Either you do not have any GPU drivers installed or the "
        "drivers are not configured "
        "properly. Fix that and use the command line tool \"glxinfo\" for "
        "debugging this.");
  }

  return res;
}

static void patch_gfx_init_gl_functions()
{
  if (!patch_gfx_real_glViewport) {
    patch_gfx_real_glViewport = (glViewport_t) cnh_lib_get_func_addr("glViewport");
  }
  if (!patch_gfx_real_glMatrixMode) {
    patch_gfx_real_glMatrixMode = (glMatrixMode_t) cnh_lib_get_func_addr("glMatrixMode");
  }
  if (!patch_gfx_real_glLoadIdentity) {
    patch_gfx_real_glLoadIdentity = (glLoadIdentity_t) cnh_lib_get_func_addr("glLoadIdentity");
  }
  if (!patch_gfx_real_glOrtho) {
    patch_gfx_real_glOrtho = (glOrtho_t) cnh_lib_get_func_addr("glOrtho");
  }
  if (!patch_gfx_real_glScalef) {
    patch_gfx_real_glScalef = (glScalef_t) cnh_lib_get_func_addr("glScalef");
  }
  if (!patch_gfx_real_glPushMatrix) {
    patch_gfx_real_glPushMatrix = (glPushMatrix_t) cnh_lib_get_func_addr("glPushMatrix");
  }
  if (!patch_gfx_real_glPopMatrix) {
    patch_gfx_real_glPopMatrix = (glPopMatrix_t) cnh_lib_get_func_addr("glPopMatrix");
  }
  if (!patch_gfx_real_glClear) {
    patch_gfx_real_glClear = (glClear_t) cnh_lib_get_func_addr("glClear");
  }
  if (!patch_gfx_real_glClearColor) {
    patch_gfx_real_glClearColor = (glClearColor_t) cnh_lib_get_func_addr("glClearColor");
  }
}

void patch_gfx_init()
{
  patch_gfx_initialized = true;
  log_info("Initialized");
}

// OpenGL hooked functions
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
  patch_gfx_init_gl_functions();
  
  if (!patch_gfx_real_glViewport) {
    log_error("Failed to get real glViewport function");
    return;
  }

  if (patch_gfx_scaling_active && patch_gfx_current_scale_mode != PATCH_GFX_SCALE_MODE_INVALID) {
    int src_width, src_height, dst_width, dst_height;
    patch_gfx_get_scale_params(patch_gfx_current_scale_mode, 
                               &src_width, &src_height, &dst_width, &dst_height);
    
    // Calculate aspect ratio for pillarboxing/letterboxing
    float target_aspect = (float)src_width / (float)src_height;
    float screen_aspect = (float)width / (float)height;
    
    int scaled_width = width;
    int scaled_height = height;
    int vp_x = x;
    int vp_y = y;
    
    if (screen_aspect > target_aspect) {
      // Screen is wider - pillarbox (black bars on sides)
      scaled_width = (int)(height * target_aspect + 0.5f);
      vp_x = x + (width - scaled_width) / 2;
    } else if (screen_aspect < target_aspect) {
      // Screen is taller - letterbox (black bars on top/bottom)
      scaled_height = (int)(width / target_aspect + 0.5f);
      vp_y = y + (height - scaled_height) / 2;
    }
    
    log_debug("Scaling viewport: original(%d,%d,%d,%d) -> scaled(%d,%d,%d,%d)", 
              x, y, width, height, vp_x, vp_y, scaled_width, scaled_height);
    
    patch_gfx_real_glViewport(vp_x, vp_y, scaled_width, scaled_height);
  } else {
    patch_gfx_real_glViewport(x, y, width, height);
  }
}

void glClear(GLbitfield mask)
{
  patch_gfx_init_gl_functions();
  
  if (!patch_gfx_real_glClear) {
    log_error("Failed to get real glClear function");
    return;
  }

  // Always clear with black to create proper letterbox/pillarbox bars
  if (patch_gfx_scaling_active && (mask & GL_COLOR_BUFFER_BIT)) {
    if (patch_gfx_real_glClearColor) {
      patch_gfx_real_glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    }
  }
  
  patch_gfx_real_glClear(mask);
}

// Note, consider improving scaling, see
// http://www.david-amador.com/2013/04/opengl-2d-independent-resolution-rendering/
void patch_gfx_scale(enum patch_gfx_scale_mode scale_mode)
{
  if (scale_mode == PATCH_GFX_SCALE_MODE_INVALID) {
    patch_gfx_scaling_active = false;
    patch_gfx_current_scale_mode = PATCH_GFX_SCALE_MODE_INVALID;
    log_info("Graphics scaling disabled");
    return;
  }

  patch_gfx_current_scale_mode = scale_mode;
  patch_gfx_scaling_active = true;
  
  int src_width, src_height, dst_width, dst_height;
  patch_gfx_get_scale_params(scale_mode, &src_width, &src_height, &dst_width, &dst_height);
  
  const char *mode_desc;
  switch (scale_mode) {
    case PATCH_GFX_SCALE_MODE_SD_480_TO_PILLARBOX_HD_720:
      mode_desc = "SD 480p to pillarbox HD 720p";
      break;
    case PATCH_GFX_SCALE_MODE_SD_480_TO_PILLARBOX_HD_1080:
      mode_desc = "SD 480p to pillarbox HD 1080p";
      break;
    case PATCH_GFX_SCALE_MODE_SD_480_TO_SD_960:
      mode_desc = "SD 480p to SD 960p (2x scale)";
      break;
    case PATCH_GFX_SCALE_MODE_HD_720_TO_HD_1080:
      mode_desc = "HD 720p to HD 1080p";
      break;
    default:
      mode_desc = "Unknown";
      break;
  }
  
  log_info("Graphics scaling enabled: %s (%dx%d -> %dx%d)", 
           mode_desc, src_width, src_height, dst_width, dst_height);
}
