/*
 * AutoClicker Advanced - global hotkeys (XInput2 raw events)
 * Based on XClicker by robiot (https://github.com/robiot/xclicker), GPL-3.0.
 * Modified 2026-11-03: raw key and button events on the root window, used for
 * start/stop, for confirming a picked position and for hotkey capture.
 */
#include "hotkeys.h"

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/keysym.h>
#include <stdlib.h>

#include "config.h"
#include "x11api.h"

static HotkeyCallbacks cbs;
static gint picking, capturing;

void hk_set_picking(int step)
{
    g_atomic_int_set(&picking, step);
}

int hk_picking(void)
{
    return g_atomic_int_get(&picking);
}

void hk_capture_begin(void)
{
    g_atomic_int_set(&capturing, 1);
}

void hk_capture_cancel(void)
{
    g_atomic_int_set(&capturing, 0);
}

typedef struct {
    int kind; /* 0 hotkey, 1 pick, 2 capture, 3 screenshot */
    int a, b, c, d;
} Msg;

static gboolean dispatch(gpointer p)
{
    Msg *m = p;
    if (m->kind == 0 && cbs.hotkey)
        cbs.hotkey(m->a);
    else if (m->kind == 1 && cbs.pick_done)
        cbs.pick_done(m->a, m->b, m->c, m->d);
    else if (m->kind == 2 && cbs.capture)
        cbs.capture(m->a, m->b, m->c);
    else if (m->kind == 3 && cbs.shot)
        cbs.shot();
    g_free(m);
    return G_SOURCE_REMOVE;
}

static void post(int kind, int a, int b, int c, int d)
{
    Msg *m = g_new0(Msg, 1);
    m->kind = kind;
    m->a = a;
    m->b = b;
    m->c = c;
    m->d = d;
    g_idle_add(dispatch, m);
}

static gpointer hk_thread(gpointer u)
{
    (void)u;
    Display *d = XOpenDisplay(NULL);
    if (!d)
        return NULL;
    int op, ev, er;
    if (!XQueryExtension(d, "XInputExtension", &op, &ev, &er)) {
        g_printerr("autoclicker-advanced: XInput2 not available, hotkeys disabled\n");
        return NULL;
    }
    unsigned char m[XIMaskLen(XI_LASTEVENT)] = {0};
    XIEventMask mask = {XIAllMasterDevices, sizeof m, m};
    XISetMask(m, XI_RawKeyPress);
    XISetMask(m, XI_RawKeyRelease);
    XISetMask(m, XI_RawButtonPress);
    XISelectEvents(d, DefaultRootWindow(d), &mask, 1);
    XSync(d, False);

    const KeySym mod_syms[] = {XK_Shift_L, XK_Shift_R, XK_Control_L, XK_Control_R, XK_Alt_L,
                               XK_Alt_R,   XK_Super_L, XK_Super_R,   XK_ISO_Level3_Shift};
    KeyCode mods[G_N_ELEMENTS(mod_syms)];
    for (unsigned i = 0; i < G_N_ELEMENTS(mod_syms); i++)
        mods[i] = XKeysymToKeycode(d, mod_syms[i]);
    KeyCode esc = XKeysymToKeycode(d, XK_Escape);

    gboolean mod_down = FALSE, main_down = FALSE;
    int cap_mod = -1;

    for (;;) {
        XEvent e;
        XNextEvent(d, &e);
        XGenericEventCookie *ck = &e.xcookie;
        if (ck->type != GenericEvent || ck->extension != op || !XGetEventData(d, ck))
            continue;
        XIRawEvent *re = ck->data;
        int kc = re->detail;
        int key_mod = cfg.key_mod, key_main = cfg.key_main, key_shot = cfg.shot_key;
        int pk = g_atomic_int_get(&picking);

        if (ck->evtype == XI_RawKeyPress && !(re->flags & XIKeyRepeat)) {
            gboolean is_mod = FALSE;
            for (unsigned i = 0; i < G_N_ELEMENTS(mods); i++)
                if (kc == mods[i])
                    is_mod = TRUE;
            if (g_atomic_int_get(&capturing)) {
                if (is_mod) {
                    cap_mod = kc;
                    post(2, cap_mod, -1, FALSE, 0);
                } else {
                    post(2, cap_mod, kc, TRUE, 0);
                    cap_mod = -1;
                    g_atomic_int_set(&capturing, 0);
                }
            } else {
                if (kc == key_mod)
                    mod_down = TRUE;
                gboolean hit = kc == key_main && (key_mod < 0 || mod_down);
                if (pk) {
                    int x = 0, y = 0;
                    if (kc == esc)
                        post(1, pk, 0, 0, FALSE);
                    else if (hit && xa_pointer(d, &x, &y))
                        post(1, pk, x, y, TRUE);
                } else if (hit) {
                    main_down = TRUE;
                    post(0, TRUE, 0, 0, 0);
                } else if (kc == key_shot && key_shot > 0) {
                    post(3, 0, 0, 0, 0);
                }
            }
        } else if (ck->evtype == XI_RawKeyRelease) {
            if (kc == key_mod)
                mod_down = FALSE;
            if (kc == key_main && main_down) {
                main_down = FALSE;
                post(0, FALSE, 0, 0, 0);
            }
        } else if (ck->evtype == XI_RawButtonPress && pk && re->detail == 1) {
            int x = 0, y = 0;
            if (xa_pointer(d, &x, &y))
                post(1, pk, x, y, TRUE);
        }
        XFreeEventData(d, ck);
    }
    return NULL;
}

void hk_start(const HotkeyCallbacks *cb)
{
    cbs = *cb;
    g_thread_unref(g_thread_new("hotkeys", hk_thread, NULL));
}
