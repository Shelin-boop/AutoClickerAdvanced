/*
 * AutoClicker Advanced - X11 helpers
 * Based on XClicker by robiot (https://github.com/robiot/xclicker), GPL-3.0.
 * Modified 2026-11-03: clicks are sent with XSendEvent directly to the window
 * under the target position; the mouse pointer is never moved.
 */
#ifndef XCA_X11API_H
#define XCA_X11API_H

#include <X11/Xlib.h>
#include <glib.h>

/* error handler that only logs; install before GTK starts */
int xa_error_handler(Display *d, XErrorEvent *e);
/* number of X errors so far (for simple error checks around a request) */
int xa_error_count(void);

/* windows created by this process (overlays) are never click targets */
void xa_register_own(Window w);
gboolean xa_is_own(Display *d, Window w);

/* current pointer position in root coordinates; FALSE if unknown */
gboolean xa_pointer(Display *d, int *x, int *y);
/* same, *buttons tells whether the user holds a real mouse button */
gboolean xa_pointer_buttons(Display *d, int *x, int *y, gboolean *buttons);

/* topmost foreign window at a root position (deepest child), with local coordinates */
Window xa_target_at(Display *d, int rx, int ry, int *lx, int *ly);

/*
 * Click at root position (rx, ry) with button 1..3, keeping it pressed for
 * press_ms. settle_ms is waited after the move to the position and after the
 * release, so that a game sees each event in its own frame. If move_back is
 * TRUE, the window is told afterwards where the real pointer is. If shield is
 * TRUE, real pointer events are kept away from the target window during the
 * click (*shielded tells whether that worked).
 * Returns FALSE if no target window was found.
 */
gboolean xa_click(Display *d, int rx, int ry, int button, int press_ms, int settle_ms, gboolean move_back,
                  gboolean shield, gboolean *shielded);

/* human readable key name; returns a static string */
const char *xa_key_name(Display *d, int keycode);

void xa_sleep_ms(int ms);
gint64 xa_now_ms(void);

#endif
