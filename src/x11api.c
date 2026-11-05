/*
 * AutoClicker Advanced - X11 helpers
 * Based on XClicker by robiot (https://github.com/robiot/xclicker), GPL-3.0.
 * Modified 2026-11-03: XSendEvent clicks without pointer movement, target
 * window lookup that skips own windows.
 */
#include "x11api.h"

#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static Window own[32];
static int n_own;
static GMutex own_lock;

static gint err_count;

int xa_error_count(void)
{
    return g_atomic_int_get(&err_count);
}

int xa_error_handler(Display *d, XErrorEvent *e)
{
    g_atomic_int_inc(&err_count);
    char buf[128] = "";
    XGetErrorText(d, e->error_code, buf, sizeof buf);
    g_debug("X error ignored: %s (request %d)", buf, e->request_code);
    return 0;
}

void xa_register_own(Window w)
{
    g_mutex_lock(&own_lock);
    if (n_own < (int)G_N_ELEMENTS(own))
        own[n_own++] = w;
    g_mutex_unlock(&own_lock);
}

gboolean xa_is_own(Display *d, Window w)
{
    gboolean found = FALSE;
    g_mutex_lock(&own_lock);
    for (int i = 0; i < n_own; i++)
        if (own[i] == w)
            found = TRUE;
    g_mutex_unlock(&own_lock);
    if (found)
        return TRUE;

    Atom a = XInternAtom(d, "_NET_WM_PID", True), type;
    if (a == None)
        return FALSE;
    int fmt;
    unsigned long n, after;
    unsigned char *data = NULL;
    if (XGetWindowProperty(d, w, a, 0, 1, False, XA_CARDINAL, &type, &fmt, &n, &after, &data) == Success && data) {
        if (n == 1 && fmt == 32 && (pid_t)(*(long *)data) == getpid())
            found = TRUE;
        XFree(data);
    }
    return found;
}

gboolean xa_pointer(Display *d, int *x, int *y)
{
    return xa_pointer_buttons(d, x, y, NULL);
}

gboolean xa_pointer_buttons(Display *d, int *x, int *y, gboolean *buttons)
{
    Window rt, ch;
    int wx, wy;
    unsigned m = 0;
    Bool r = XQueryPointer(d, DefaultRootWindow(d), &rt, &ch, x, y, &wx, &wy, &m);
    if (buttons)
        *buttons = (m & (Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask)) != 0;
    return r;
}

Window xa_target_at(Display *d, int rx, int ry, int *lx, int *ly)
{
    Window root = DefaultRootWindow(d), rr, parent, *kids = NULL;
    unsigned nk = 0;
    if (!XQueryTree(d, root, &rr, &parent, &kids, &nk))
        return None;
    Window top = None;
    for (int i = (int)nk - 1; i >= 0; i--) { /* top of the stacking order first */
        XWindowAttributes a;
        if (!XGetWindowAttributes(d, kids[i], &a))
            continue;
        if (a.map_state != IsViewable || a.class == InputOnly)
            continue;
        int bw = 2 * a.border_width;
        if (rx < a.x || ry < a.y || rx >= a.x + a.width + bw || ry >= a.y + a.height + bw)
            continue;
        if (xa_is_own(d, kids[i]))
            continue;
        top = kids[i];
        break;
    }
    if (kids)
        XFree(kids);
    if (top == None)
        return None;

    Window w = top, child = None;
    int x = 0, y = 0;
    for (int k = 0; k < 16; k++) {
        if (!XTranslateCoordinates(d, root, w, rx, ry, &x, &y, &child))
            break;
        if (child == None || xa_is_own(d, child))
            break;
        w = child;
    }
    XTranslateCoordinates(d, root, w, rx, ry, lx, ly, &child);
    return w;
}

static void send_motion(Display *d, Window w, int x, int y, int rx, int ry)
{
    XEvent e;
    memset(&e, 0, sizeof e);
    e.xmotion.type = MotionNotify;
    e.xmotion.display = d;
    e.xmotion.window = w;
    e.xmotion.root = DefaultRootWindow(d);
    e.xmotion.time = CurrentTime;
    e.xmotion.x = x;
    e.xmotion.y = y;
    e.xmotion.x_root = rx;
    e.xmotion.y_root = ry;
    e.xmotion.same_screen = True;
    XSendEvent(d, w, True, PointerMotionMask, &e);
}

static void send_button(Display *d, Window w, int type, int button, int x, int y, int rx, int ry)
{
    static const unsigned masks[4] = {0, Button1Mask, Button2Mask, Button3Mask};
    XEvent e;
    memset(&e, 0, sizeof e);
    e.xbutton.type = type;
    e.xbutton.display = d;
    e.xbutton.window = w;
    e.xbutton.root = DefaultRootWindow(d);
    e.xbutton.time = CurrentTime;
    e.xbutton.x = x;
    e.xbutton.y = y;
    e.xbutton.x_root = rx;
    e.xbutton.y_root = ry;
    e.xbutton.button = (unsigned)button;
    e.xbutton.state = type == ButtonRelease ? masks[button & 3] : 0;
    e.xbutton.same_screen = True;
    XSendEvent(d, w, True, type == ButtonPress ? ButtonPressMask : ButtonReleaseMask, &e);
}

gboolean xa_click(Display *d, int rx, int ry, int button, int press_ms, int settle_ms, gboolean move_back,
                  gboolean shield, gboolean *shielded)
{
    int lx, ly;
    if (shielded)
        *shielded = FALSE;
    Window w = xa_target_at(d, rx, ry, &lx, &ly);
    if (w == None)
        return FALSE;
    if (button < 1 || button > 3)
        button = 1;
    /*
     * A game has only one pointer. While the user moves the real mouse, its
     * motion events would reach the game during the click and the game would
     * see the click at the real mouse. A synchronous pointer grab on the target
     * window freezes the real pointer events for the duration of the click:
     * the X server queues them and delivers them normally after the ungrab,
     * so the user's own movements and clicks are only delayed, never lost.
     * XSendEvent is not affected by grabs. Because the grab window is the
     * window the pointer is already in, the game gets no crossing events.
     */
    gboolean grabbed = FALSE;
    if (shield) {
        grabbed = XGrabPointer(d, w, False, 0, GrabModeSync, GrabModeAsync, None, None, CurrentTime) == GrabSuccess;
        if (shielded)
            *shielded = grabbed;
    }
    /*
     * Games read their input once per frame and use the pointer position of
     * that frame. Every event therefore gets its own frame: move, wait, press,
     * hold, release, wait, move back. Without the waits the game sees the
     * release at the real mouse position (version 0.1.0).
     */
    send_motion(d, w, lx, ly, rx, ry);
    XFlush(d);
    xa_sleep_ms(settle_ms);
    send_button(d, w, ButtonPress, button, lx, ly, rx, ry);
    XFlush(d);
    xa_sleep_ms(press_ms);
    send_button(d, w, ButtonRelease, button, lx, ly, rx, ry);
    XFlush(d);
    if (move_back) {
        xa_sleep_ms(settle_ms);
        int bx, by, blx, bly;
        if (xa_pointer(d, &bx, &by) && xa_target_at(d, bx, by, &blx, &bly) == w) {
            send_motion(d, w, blx, bly, bx, by);
            XFlush(d);
        }
    }
    if (grabbed) {
        XUngrabPointer(d, CurrentTime);
        XFlush(d);
    }
    return TRUE;
}

const char *xa_key_name(Display *d, int keycode)
{
    if (keycode <= 0)
        return "?";
    KeySym ks = XkbKeycodeToKeysym(d, (KeyCode)keycode, 0, 0);
    const char *s = XKeysymToString(ks);
    return s ? s : "?";
}

void xa_sleep_ms(int ms)
{
    if (ms <= 0)
        return;
    struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000L};
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
        ;
}

gint64 xa_now_ms(void)
{
    return g_get_monotonic_time() / 1000;
}
