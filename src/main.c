/*
 * AutoClicker Advanced, an autoclicker for Linux with a step sequence.
 *
 * Based on XClicker by robiot (https://github.com/robiot/xclicker).
 * All source files are licensed under the GNU General Public License v3.0.
 * Modified 2026-11-03, see NOTICE.md.
 */
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <time.h>

#include "config.h"
#include "integration.h"
#include "mainwin.h"
#include "overlay.h"
#include "version.h"
#include "x11api.h"

static GtkWidget *win;

static void on_activate(GtkApplication *app, gpointer u)
{
    (void)u;
    if (win) {
        gtk_window_present(GTK_WINDOW(win));
        return;
    }
    config_load();
    if (!ov_init()) {
        GtkWidget *m = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                              "Cannot connect to the X server (Xwayland).");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(m), "DISPLAY=%s",
                                                 g_getenv("DISPLAY") ? g_getenv("DISPLAY") : "(not set)");
        gtk_dialog_run(GTK_DIALOG(m));
        gtk_widget_destroy(m);
        return;
    }
    if (cfg.key_main <= 0)
        cfg.key_main = XKeysymToKeycode(ov_display(), XK_F8);
    if (cfg.shot_key <= 0)
        cfg.shot_key = XKeysymToKeycode(ov_display(), XK_F9);
    if (cfg.start_menu_entry && integ_available())
        integ_install();

    GdkPixbuf *icon = gdk_pixbuf_new_from_resource("/xca/icon.png", NULL);
    if (icon) {
        gtk_window_set_default_icon(icon);
        g_object_unref(icon);
    }
    win = mw_create(app);
    g_object_add_weak_pointer(G_OBJECT(win), (gpointer *)&win);
}

int main(int argc, char **argv)
{
    XInitThreads();
    XSetErrorHandler(xa_error_handler);
    srand((unsigned)time(NULL));
    g_set_prgname("autoclicker-advanced");
    g_set_application_name(XCA_NAME);
    GtkApplication *app = gtk_application_new(XCA_APP_ID, (GApplicationFlags)0);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
