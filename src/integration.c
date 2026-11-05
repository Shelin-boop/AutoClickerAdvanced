/*
 * AutoClicker Advanced - start menu entry for the AppImage
 * Copyright (C) 2026 AutoClicker Advanced contributors, GPL-3.0.
 *
 * Writes a desktop file and an icon below ~/.local/share (no root rights).
 * The Exec line points to the AppImage file itself; if the AppImage was moved,
 * the next start rewrites the entry.
 * Version 0.3.1 (new name) removes the entry of version 0.3.0 and earlier.
 */
#include "integration.h"

#include <glib/gstdio.h>
#include <string.h>

#include "version.h"

static char *icon_path_for(const char *id)
{
    char *file = g_strconcat(id, ".png", NULL);
    char *p = g_build_filename(g_get_user_data_dir(), "icons", "hicolor", "256x256", "apps", file, NULL);
    g_free(file);
    return p;
}

static char *desktop_path_for(const char *id)
{
    char *file = g_strconcat(id, ".desktop", NULL);
    char *p = g_build_filename(g_get_user_data_dir(), "applications", file, NULL);
    g_free(file);
    return p;
}

static char *icon_path(void)
{
    return icon_path_for(XCA_APP_ID);
}

char *integ_desktop_path(void)
{
    return desktop_path_for(XCA_APP_ID);
}

/* Removes the start menu entry written by version 0.3.0 and earlier (old
 * name). Only an entry that carries the marker of those versions is touched. */
static gboolean remove_old_entry(void)
{
    char *dp = desktop_path_for(XCA_OLD_APP_ID), *ip = icon_path_for(XCA_OLD_APP_ID);
    char *data = NULL;
    gboolean removed = FALSE;
    if (g_file_get_contents(dp, &data, NULL, NULL) && strstr(data, "X-XClickerAdvanced-Version=")) {
        g_unlink(dp);
        g_unlink(ip);
        removed = TRUE;
    }
    g_free(data);
    g_free(dp);
    g_free(ip);
    return removed;
}

gboolean integ_available(void)
{
    const char *a = g_getenv("APPIMAGE"), *d = g_getenv("APPDIR");
    return a && *a && d && *d;
}

gboolean integ_installed(void)
{
    char *p = integ_desktop_path();
    gboolean r = g_file_test(p, G_FILE_TEST_EXISTS);
    g_free(p);
    return r;
}

/* quoting for the Exec key (Desktop Entry Specification) */
static char *exec_quote(const char *path)
{
    GString *s = g_string_new("\"");
    for (const char *p = path; *p; p++) {
        switch (*p) {
        case '"':
            g_string_append(s, "\\\\\"");
            break;
        case '`':
            g_string_append(s, "\\\\`");
            break;
        case '$':
            g_string_append(s, "\\\\$");
            break;
        case '\\':
            g_string_append(s, "\\\\\\\\");
            break;
        case '%':
            g_string_append(s, "%%");
            break;
        default:
            g_string_append_c(s, *p);
        }
    }
    g_string_append_c(s, '"');
    return g_string_free(s, FALSE);
}

static gboolean write_if_changed(const char *path, const char *data, gsize len)
{
    char *old = NULL;
    gsize oldlen = 0;
    if (g_file_get_contents(path, &old, &oldlen, NULL) && oldlen == len && memcmp(old, data, len) == 0) {
        g_free(old);
        return FALSE;
    }
    g_free(old);
    char *dir = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);
    GError *e = NULL;
    if (!g_file_set_contents(path, data, (gssize)len, &e)) {
        g_printerr("autoclicker-advanced: cannot write %s: %s\n", path, e ? e->message : "?");
        if (e)
            g_error_free(e);
        return FALSE;
    }
    return TRUE;
}

static void refresh_menu(void)
{
    char *apps = g_build_filename(g_get_user_data_dir(), "applications", NULL);
    char *argv1[] = {"update-desktop-database", "-q", apps, NULL};
    char *argv2[] = {"kbuildsycoca6", NULL};
    g_spawn_async(NULL, argv1, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                  NULL, NULL, NULL, NULL);
    g_spawn_async(NULL, argv2, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                  NULL, NULL, NULL, NULL);
    g_free(apps);
}

gboolean integ_install(void)
{
    if (!integ_available())
        return FALSE;
    gboolean changed = remove_old_entry();

    char *src = g_build_filename(g_getenv("APPDIR"), XCA_APP_ID ".png", NULL);
    char *icon = NULL;
    gsize ilen = 0;
    if (g_file_get_contents(src, &icon, &ilen, NULL)) {
        char *ip = icon_path();
        changed |= write_if_changed(ip, icon, ilen);
        g_free(ip);
        g_free(icon);
    }
    g_free(src);

    char *exec = exec_quote(g_getenv("APPIMAGE"));
    char *desktop = g_strdup_printf("[Desktop Entry]\n"
                                    "Type=Application\n"
                                    "Name=" XCA_NAME "\n"
                                    "GenericName=Autoclicker\n"
                                    "Comment=Autoclicker with position steps\n"
                                    "Exec=%s\n"
                                    "Icon=" XCA_APP_ID "\n"
                                    "Terminal=false\n"
                                    "Categories=Utility;\n"
                                    "StartupWMClass=autoclicker-advanced\n"
                                    "X-AutoClickerAdvanced-Version=" XCA_VERSION "\n",
                                    exec);
    char *dp = integ_desktop_path();
    changed |= write_if_changed(dp, desktop, strlen(desktop));
    g_free(dp);
    g_free(desktop);
    g_free(exec);
    if (changed)
        refresh_menu();
    return changed;
}

void integ_remove(void)
{
    char *dp = integ_desktop_path(), *ip = icon_path();
    g_unlink(dp);
    g_unlink(ip);
    g_free(dp);
    g_free(ip);
    remove_old_entry();
    refresh_menu();
}
