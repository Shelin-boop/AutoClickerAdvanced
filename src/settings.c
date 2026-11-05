/*
 * AutoClicker Advanced - settings dialog
 * Based on XClicker by robiot (https://github.com/robiot/xclicker), GPL-3.0.
 * Modified 2026-11-03: built in code; safe mode (100 ms / 50 ms), start/stop
 * hotkey, mouse position step, double click gap, start menu entry, reset.
 * The XEvent/XTest switch of the original is gone: clicks always use XSendEvent.
 */
#include "settings.h"

#include <stdlib.h>

#include "config.h"
#include "hotkeys.h"
#include "integration.h"
#include "mainwin.h"
#include "overlay.h"
#include "version.h"
#include "x11api.h"

static struct {
    GtkWidget *dlg, *keys_entry, *change_btn, *menu_label, *menu_switch, *shot_entry, *shot_btn;
    int cap_mod;
    int cap_target; /* 0 start/stop hotkey, 1 screenshot key */
} S;

static GtkWidget *section(const char *title, GtkWidget *content)
{
    GtkWidget *f = gtk_frame_new(title);
    gtk_widget_set_margin_start(content, 10);
    gtk_widget_set_margin_end(content, 10);
    gtk_widget_set_margin_top(content, 6);
    gtk_widget_set_margin_bottom(content, 10);
    gtk_container_add(GTK_CONTAINER(f), content);
    return f;
}

static GtkWidget *text_row(const char *text, GtkWidget *right)
{
    GtkWidget *b = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *l = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(l), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(l), 46);
    gtk_box_pack_start(GTK_BOX(b), l, TRUE, TRUE, 0);
    if (right) {
        gtk_widget_set_valign(right, GTK_ALIGN_CENTER);
        gtk_box_pack_end(GTK_BOX(b), right, FALSE, FALSE, 0);
    }
    return b;
}

static void keys_text(char *buf, size_t n, int mod, int key)
{
    Display *d = ov_display();
    if (!d) {
        snprintf(buf, n, "F8");
        return;
    }
    if (mod >= 0 && key > 0)
        snprintf(buf, n, "%s + %s", xa_key_name(d, mod), xa_key_name(d, key));
    else if (mod >= 0)
        snprintf(buf, n, "%s + ...", xa_key_name(d, mod));
    else
        snprintf(buf, n, "%s", xa_key_name(d, key));
}

void settings_on_capture(int mod, int key, gboolean done)
{
    if (!S.dlg)
        return;
    char buf[96];
    if (S.cap_target == 1) { /* screenshot key: a single key */
        if (!done)
            return;
        cfg.shot_key = key;
        config_save();
        keys_text(buf, sizeof buf, -1, key);
        gtk_entry_set_text(GTK_ENTRY(S.shot_entry), buf);
        mw_set_capturing(FALSE);
        mw_update_shot_label();
        gtk_widget_set_sensitive(S.shot_btn, TRUE);
        return;
    }
    keys_text(buf, sizeof buf, mod, key);
    gtk_entry_set_text(GTK_ENTRY(S.keys_entry), buf);
    if (done) {
        cfg.key_mod = mod;
        cfg.key_main = key;
        config_save();
        mw_set_capturing(FALSE);
        mw_update_hotkey_labels();
        gtk_widget_set_sensitive(S.change_btn, TRUE);
    }
}

static void on_change_hotkey(GtkButton *b, gpointer u)
{
    S.cap_target = GPOINTER_TO_INT(u);
    gtk_widget_set_sensitive(GTK_WIDGET(b), FALSE);
    gtk_entry_set_text(GTK_ENTRY(S.cap_target ? S.shot_entry : S.keys_entry),
                       S.cap_target ? "Press the desired key" : "Press the desired keys");
    mw_set_capturing(TRUE);
    hk_capture_begin();
}

static void on_spin_pct(GtkSpinButton *sp, gpointer u)
{
    *(int *)u = (int)(gtk_spin_button_get_value(sp) * 100 + 0.5);
    config_save();
    mw_update_shot_label();
}

static void on_downscale(GtkComboBox *c, gpointer u)
{
    (void)u;
    const char *id = gtk_combo_box_get_active_id(c);
    cfg.downscale = id ? atoi(id) : 2;
    config_save();
}

static void on_countdown(GtkSpinButton *sp, gpointer u)
{
    (void)u;
    cfg.shot_countdown_s = gtk_spin_button_get_value_as_int(sp);
    config_save();
    mw_update_shot_label();
}

static gboolean on_safe(GtkSwitch *sw, gboolean state, gpointer u)
{
    (void)u;
    cfg.safe_mode = state;
    config_save();
    gtk_switch_set_state(sw, state);
    return TRUE;
}

static void on_spin(GtkSpinButton *sp, gpointer u)
{
    *(int *)u = gtk_spin_button_get_value_as_int(sp);
    config_save();
}

static void on_check(GtkToggleButton *t, gpointer u)
{
    *(gboolean *)u = gtk_toggle_button_get_active(t);
    config_save();
}

static GtkWidget *dim_note(const char *text)
{
    GtkWidget *l = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(l), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(l), 52);
    gtk_style_context_add_class(gtk_widget_get_style_context(l), "dim-label");
    return l;
}

static void menu_label_update(void)
{
    if (!integ_available()) {
        gtk_label_set_text(GTK_LABEL(S.menu_label),
                           "Adds AutoClicker Advanced to the start menu. Only available when started from the AppImage.");
        return;
    }
    if (integ_installed()) {
        char *p = integ_desktop_path();
        char *t = g_strdup_printf("Entry in the start menu: on, updated at every start.\n%s", p);
        gtk_label_set_text(GTK_LABEL(S.menu_label), t);
        g_free(t);
        g_free(p);
    } else
        gtk_label_set_text(GTK_LABEL(S.menu_label), "Entry in the start menu: off.");
}

static gboolean on_menu(GtkSwitch *sw, gboolean state, gpointer u)
{
    (void)u;
    cfg.start_menu_entry = state;
    config_save();
    if (state)
        integ_install();
    else
        integ_remove();
    gtk_switch_set_state(sw, state);
    menu_label_update();
    return TRUE;
}

static void on_reset(GtkButton *b, gpointer u)
{
    (void)b;
    (void)u;
    config_reset_presets();
    mw_reload();
}

static GtkWidget *spin(int lo, int hi, int *value)
{
    GtkWidget *s = gtk_spin_button_new_with_range(lo, hi, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(s), *value);
    g_signal_connect(s, "value-changed", G_CALLBACK(on_spin), value);
    return s;
}

void settings_run(GtkWindow *parent)
{
    memset(&S, 0, sizeof S);
    S.dlg = gtk_dialog_new_with_buttons("Settings", parent, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        "Close", GTK_RESPONSE_CLOSE, NULL);
    GtkWidget *area = gtk_dialog_get_content_area(GTK_DIALOG(S.dlg));
    GtkWidget *nb = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(area), nb, TRUE, TRUE, 0);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *box2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *box3 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 10);
    gtk_container_set_border_width(GTK_CONTAINER(box2), 10);
    gtk_container_set_border_width(GTK_CONTAINER(box3), 10);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), box, gtk_label_new("General"));
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), box2, gtk_label_new("Mouse and timing"));
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), box3, gtk_label_new("Pattern reactions"));

    /* safe mode */
    GtkWidget *sw = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(sw), cfg.safe_mode);
    g_signal_connect(sw, "state-set", G_CALLBACK(on_safe), NULL);
    gtk_box_pack_start(GTK_BOX(box),
                       section("Safe mode", text_row("Intervals of at least 100 milliseconds. "
                                                     "Without safe mode the minimum is 50 milliseconds.",
                                                     sw)),
                       FALSE, FALSE, 0);

    /* hotkey */
    GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_set_homogeneous(GTK_BOX(hb), TRUE);
    S.change_btn = gtk_button_new_with_label("Change");
    S.keys_entry = gtk_entry_new();
    gtk_editable_set_editable(GTK_EDITABLE(S.keys_entry), FALSE);
    gtk_widget_set_can_focus(S.keys_entry, FALSE);
    char kb[96];
    keys_text(kb, sizeof kb, cfg.key_mod, cfg.key_main);
    gtk_entry_set_text(GTK_ENTRY(S.keys_entry), kb);
    g_signal_connect(S.change_btn, "clicked", G_CALLBACK(on_change_hotkey), GINT_TO_POINTER(0));
    gtk_box_pack_start(GTK_BOX(hb), S.change_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hb), S.keys_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), section("Start/Stop hotkey (also confirms \"Get\")", hb), FALSE, FALSE, 0);

    /* mouse position step */
    GtkWidget *mg = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(mg), 6);
    gtk_grid_set_column_spacing(GTK_GRID(mg), 10);
    GtkWidget *l1 = gtk_label_new("Pause when the mouse moves more than (px)");
    GtkWidget *l2 = gtk_label_new("Continue after the mouse rested for (ms)");
    gtk_label_set_xalign(GTK_LABEL(l1), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(l2), 0.0f);
    gtk_widget_set_hexpand(l1, TRUE);
    gtk_grid_attach(GTK_GRID(mg), l1, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(mg), spin(1, 100, &cfg.motion_threshold_px), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(mg), l2, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(mg), spin(50, 5000, &cfg.motion_rest_ms), 1, 1, 1, 1);
    GtkWidget *pc = gtk_check_button_new_with_label("Also pause positions 1 to 3 while the mouse moves");
    gtk_widget_set_tooltip_text(pc, "A game has only one mouse pointer. If you move the mouse while a position "
                                    "is clicked, the game can see the click at your mouse.");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pc), cfg.pause_positions);
    g_signal_connect(pc, "toggled", G_CALLBACK(on_check), &cfg.pause_positions);
    gtk_grid_attach(GTK_GRID(mg), pc, 0, 2, 2, 1);
    GtkWidget *sc = gtk_check_button_new_with_label("Hold the mouse off the game during a position click");
    gtk_widget_set_tooltip_text(sc, "Keeps position clicks on target while you move the mouse. Your own mouse "
                                    "events during a position click (about 130 ms) reach the game right after it.");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sc), cfg.shield_pointer);
    g_signal_connect(sc, "toggled", G_CALLBACK(on_check), &cfg.shield_pointer);
    gtk_grid_attach(GTK_GRID(mg), sc, 0, 3, 2, 1);
    GtkWidget *uc = gtk_check_button_new_with_label("All clicks wait while you click; continue after (ms)");
    gtk_widget_set_tooltip_text(uc, "While you hold a mouse button, and this long after you release it, "
                                    "AutoClicker Advanced does not click.");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(uc), cfg.manual_priority);
    g_signal_connect(uc, "toggled", G_CALLBACK(on_check), &cfg.manual_priority);
    gtk_grid_attach(GTK_GRID(mg), uc, 0, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(mg), spin(0, 5000, &cfg.manual_pause_ms), 1, 4, 1, 1);
    gtk_box_pack_start(GTK_BOX(box2), section("Your mouse", mg), FALSE, FALSE, 0);

    /* click timing */
    GtkWidget *tg = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(tg), 6);
    gtk_grid_set_column_spacing(GTK_GRID(tg), 10);
    GtkWidget *l3 = gtk_label_new("Wait before pressing and after releasing (ms)");
    GtkWidget *l4 = gtk_label_new("Gap between the two clicks of a double click (ms)");
    GtkWidget *l5 = dim_note("Games read the mouse once per frame. 40 ms suits games with 25 frames per "
                             "second or more. A position click then takes about 130 ms, a mouse position click "
                             "about 50 ms. Between two clicks there is at least this wait.");
    gtk_label_set_xalign(GTK_LABEL(l3), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(l4), 0.0f);
    gtk_widget_set_hexpand(l3, TRUE);
    gtk_grid_attach(GTK_GRID(tg), l3, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(tg), spin(0, 200, &cfg.settle_ms), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(tg), l4, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(tg), spin(20, 500, &cfg.double_gap_ms), 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(tg), l5, 0, 2, 2, 1);
    gtk_box_pack_start(GTK_BOX(box2), section("Click timing", tg), FALSE, FALSE, 0);

    /* pattern reactions */
    GtkWidget *rg = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(rg), 6);
    gtk_grid_set_column_spacing(GTK_GRID(rg), 10);
    int row = 0;
#define RROW(text, widget, tip)                                                                                       \
    do {                                                                                                              \
        GtkWidget *l_ = gtk_label_new(text);                                                                          \
        gtk_label_set_xalign(GTK_LABEL(l_), 0.0f);                                                                    \
        gtk_widget_set_hexpand(l_, TRUE);                                                                             \
        gtk_widget_set_tooltip_text(l_, tip);                                                                         \
        gtk_grid_attach(GTK_GRID(rg), l_, 0, row, 1, 1);                                                              \
        gtk_grid_attach(GTK_GRID(rg), widget, 1, row, 1, 1);                                                          \
        row++;                                                                                                        \
    } while (0)
    GtkWidget *thr = gtk_spin_button_new_with_range(0.50, 0.99, 0.01);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(thr), 2);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(thr), cfg.threshold_pct / 100.0);
    g_signal_connect(thr, "value-changed", G_CALLBACK(on_spin_pct), &cfg.threshold_pct);
    RROW("Similarity threshold (1.00 = identical)", thr,
         "A pattern counts as found from this similarity on. The status shows the current value per frame.");
    GtkWidget *ds = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ds), "1", "1 (exact, slowest)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ds), "2", "2 (default)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ds), "4", "4 (fastest)");
    char dsid[4];
    snprintf(dsid, sizeof dsid, "%d", cfg.downscale);
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(ds), dsid);
    g_signal_connect(ds, "changed", G_CALLBACK(on_downscale), NULL);
    RROW("Search on a reduced image (factor)", ds,
         "The coarse search runs on a smaller image; every hit is then checked at full resolution.");
    RROW("Compared part of the circle (%)", spin(50, 100, &cfg.core_pct),
         "Only the inner part of the circle is compared, so the background around the pattern does not count.");
    RROW("Color tolerance", spin(0, 100, &cfg.color_tol),
         "Hits with a different color are ignored (0 = same color only). Independent of brightness.");
    RROW("Lock radius (px)", spin(0, 200, &cfg.lock_radius),
         "Known patterns that wobble within this radius stay known; after a click this area is locked.");
    RROW("Lock time after a click (ms)", spin(0, 10000, &cfg.lock_ms),
         "No second click at the same place within this time.");
    GtkWidget *ie = gtk_check_button_new_with_label("Ignore patterns that are visible at the start");
    gtk_widget_set_tooltip_text(ie, "Except at the place where the pattern was taken.");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ie), cfg.ignore_existing);
    g_signal_connect(ie, "toggled", G_CALLBACK(on_check), &cfg.ignore_existing);
    gtk_grid_attach(GTK_GRID(rg), ie, 0, row++, 2, 1);
    gtk_grid_attach(GTK_GRID(rg), dim_note("Changes apply at the next start."), 0, row++, 2, 1);
    gtk_box_pack_start(GTK_BOX(box3), section("Detection", rg), FALSE, FALSE, 0);

    GtkWidget *sg = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(sg), 6);
    gtk_grid_set_column_spacing(GTK_GRID(sg), 10);
    S.shot_btn = gtk_button_new_with_label("Change");
    S.shot_entry = gtk_entry_new();
    gtk_editable_set_editable(GTK_EDITABLE(S.shot_entry), FALSE);
    gtk_widget_set_can_focus(S.shot_entry, FALSE);
    keys_text(kb, sizeof kb, -1, cfg.shot_key);
    gtk_entry_set_text(GTK_ENTRY(S.shot_entry), kb);
    g_signal_connect(S.shot_btn, "clicked", G_CALLBACK(on_change_hotkey), GINT_TO_POINTER(1));
    GtkWidget *sl = gtk_label_new("Screenshot key (while the game is in front)");
    gtk_label_set_xalign(GTK_LABEL(sl), 0.0f);
    gtk_widget_set_hexpand(sl, TRUE);
    gtk_grid_attach(GTK_GRID(sg), sl, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(sg), S.shot_btn, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(sg), S.shot_entry, 2, 0, 1, 1);
    GtkWidget *cl = gtk_label_new("Countdown of the screenshot button (s)");
    gtk_label_set_xalign(GTK_LABEL(cl), 0.0f);
    GtkWidget *cs = gtk_spin_button_new_with_range(0, 30, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(cs), cfg.shot_countdown_s);
    g_signal_connect(cs, "value-changed", G_CALLBACK(on_countdown), NULL);
    gtk_grid_attach(GTK_GRID(sg), cl, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(sg), cs, 1, 1, 2, 1);
    gtk_box_pack_start(GTK_BOX(box3), section("Screenshot", sg), FALSE, FALSE, 0);

    /* start menu */
    S.menu_switch = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(S.menu_switch), cfg.start_menu_entry && integ_installed());
    gtk_widget_set_sensitive(S.menu_switch, integ_available());
    g_signal_connect(S.menu_switch, "state-set", G_CALLBACK(on_menu), NULL);
    GtkWidget *mrow = text_row("", S.menu_switch);
    GList *kids = gtk_container_get_children(GTK_CONTAINER(mrow));
    S.menu_label = kids->data;
    g_list_free(kids);
    menu_label_update();
    gtk_box_pack_start(GTK_BOX(box), section("Start menu entry", mrow), FALSE, FALSE, 0);

    /* reset */
    GtkWidget *rb = gtk_button_new_with_label("Reset preset options");
    gtk_widget_set_tooltip_text(rb, "Resets the lines and the options. Settings, pattern and frames stay.");
    g_signal_connect(rb, "clicked", G_CALLBACK(on_reset), NULL);
    gtk_box_pack_start(GTK_BOX(box), section("Reset", rb), FALSE, FALSE, 0);

    /* footer: name, license, warranty, origin */
    GtkWidget *home = gtk_link_button_new_with_label(XCA_HOMEPAGE, XCA_NAME " v" XCA_VERSION);
    GtkWidget *lic = gtk_label_new("Free software under the GNU GPL v3.0, without any warranty.\n"
                                   "Unofficial modified version, not affiliated with the XClicker project.");
    gtk_label_set_justify(GTK_LABEL(lic), GTK_JUSTIFY_CENTER);
    GtkWidget *link = gtk_link_button_new_with_label("https://github.com/robiot/xclicker",
                                                     "Based on " XCA_UPSTREAM " by robiot");
    gtk_box_pack_start(GTK_BOX(box), home, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), lic, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), link, FALSE, FALSE, 0);

    gtk_widget_show_all(S.dlg);
    gtk_dialog_run(GTK_DIALOG(S.dlg));
    hk_capture_cancel();
    mw_set_capturing(FALSE);
    gtk_widget_destroy(S.dlg);
    S.dlg = NULL;
}
