/*
Simple battery status indicator for system tray,
via D-Bus interface of "org.freedesktop.UPower" object.
Thanks to everyone for their help and code examples.
Andrey Shashanov (2019)
Dependences: UPower https://upower.freedesktop.org/
Set your: BATTERY
gcc -O2 -s `pkg-config --cflags --libs gtk+-3.0` -o batti batti.c
*/

#define GDK_VERSION_MIN_REQUIRED (G_ENCODE_VERSION(3, 0))
#define GDK_VERSION_MAX_ALLOWED (G_ENCODE_VERSION(3, 12))

#include <glib/gprintf.h>
#include <gtk/gtk.h>

#define BATTERY "BAT0"
#define NOT_SIMBOLIC_ICON "simbolic"

#define DBUS_NAME "org.freedesktop.UPower"
#define DBUS_OBJECT_PATH "/org/freedesktop/UPower/devices/battery_" BATTERY
#define DBUS_INTERFACE_NAME "org.freedesktop.UPower.Device"
#define PROPERTY_ICONNAME "IconName"
#define PROPERTY_PERCENTAGE "Percentage"
#define PROPERTY_STATE "State"
#define TOOLTIP_TEXT_SIZE 64

typedef struct _applet
{
    GtkStatusIcon *status_icon;
    const gchar *state_string;
    guint percentage;
} applet;

static void child_watch_cb(GPid pid, gint status, GPid *ppid)
{
    (void)status;

    g_spawn_close_pid(pid);
    *ppid = 0;
}

static void activate_cb(GPid *pid)
{
    gchar *argv[] = {"/usr/bin/x-terminal-emulator",
                     "-title",
                     "upower",
                     "-hold",
                     "-e",
                     "/usr/bin/upower",
                     "-i",
                     DBUS_OBJECT_PATH,
                     NULL};

    if (*pid == 0 &&
        g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
                                 NULL, NULL, pid, NULL, NULL, NULL, NULL))
        g_child_watch_add(*pid, (GChildWatchFunc)child_watch_cb, pid);
}

static void popup_cb(GPid *pid)
{
    gchar *argv[] = {"/usr/bin/x-terminal-emulator",
                     "-title",
                     "upower",
                     "-e",
                     "/usr/bin/upower",
                     "--monitor-detail",
                     NULL};

    if (*pid == 0 &&
        g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
                                 NULL, NULL, pid, NULL, NULL, NULL, NULL))
        g_child_watch_add(*pid, (GChildWatchFunc)child_watch_cb, pid);
}

static const gchar *state_to_string(guint32 state)
{
    switch (state)
    {
    case 1:
        return "Charging";
    case 2:
        return "Discharging";
    case 3:
        return "Empty";
    case 4:
        return "Fully charged";
    case 5:
        return "Pending charge";
    case 6:
        return "Pending discharge";
    case 0:
    default:
        return "Unknown";
    }
}

static void on_properties_changed(GDBusProxy *proxy,
                                  GVariant *changed_properties,
                                  GStrv invalidated_properties,
                                  applet *data)
{
    (void)proxy;
    (void)invalidated_properties;

    GVariantIter *iter;
    const gchar *key;
    GVariant *value;
#if defined(NOT_SIMBOLIC_ICON)
    gchar *iconname;
    gsize length;
#else
    const gchar *iconname;
#endif
    guint32 state;
    gchar tooltip_text[TOOLTIP_TEXT_SIZE];

    g_variant_get(changed_properties, "a{sv}", &iter);
    while (g_variant_iter_loop(iter, "{&sv}", &key, &value))
    {
        if (g_strcmp0(key, PROPERTY_ICONNAME) == 0)
        {
#if defined(NOT_SIMBOLIC_ICON)
            iconname = g_variant_dup_string(value, &length);
            iconname[length - sizeof NOT_SIMBOLIC_ICON] = '\0';
            gtk_status_icon_set_from_icon_name(data->status_icon, iconname);
            g_free(iconname);
#else
            iconname = g_variant_get_string(value, NULL);
            gtk_status_icon_set_from_icon_name(data->status_icon, iconname);
#endif
        }
        else if (g_strcmp0(key, PROPERTY_PERCENTAGE) == 0)
        {
            data->percentage = (guint)g_variant_get_double(value);
            g_sprintf(tooltip_text, "%u%% (%s)", data->percentage,
                      data->state_string);
            gtk_status_icon_set_tooltip_text(data->status_icon, tooltip_text);
        }
        else if (g_strcmp0(key, PROPERTY_STATE) == 0)
        {
            state = g_variant_get_uint32(value);
            data->state_string = state_to_string(state);
            g_sprintf(tooltip_text, "%u%% (%s)", data->percentage,
                      data->state_string);
            gtk_status_icon_set_tooltip_text(data->status_icon, tooltip_text);
        }
    }
    g_variant_iter_free(iter);
}

static GDBusProxy *init(applet *data)
{
    GDBusProxy *proxy;
    GVariant *value;
#if defined(NOT_SIMBOLIC_ICON)
    gchar *iconname;
    gsize length;
#else
    const gchar *iconname;
#endif
    guint32 state;
    gchar tooltip_text[TOOLTIP_TEXT_SIZE];

    proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          DBUS_NAME,
                                          DBUS_OBJECT_PATH,
                                          DBUS_INTERFACE_NAME,
                                          NULL,
                                          NULL);
    if (proxy == NULL)
        return NULL;

    value = g_dbus_proxy_get_cached_property(proxy, PROPERTY_ICONNAME);
#if defined(NOT_SIMBOLIC_ICON)
    iconname = g_variant_dup_string(value, &length);
    g_variant_unref(value);
    iconname[length - sizeof NOT_SIMBOLIC_ICON] = '\0';
    gtk_status_icon_set_from_icon_name(data->status_icon, iconname);
    g_free(iconname);
#else
    iconname = g_variant_get_string(value, NULL);
    gtk_status_icon_set_from_icon_name(data->status_icon, iconname);
    g_variant_unref(value);
#endif

    value = g_dbus_proxy_get_cached_property(proxy, PROPERTY_PERCENTAGE);
    data->percentage = (guint)g_variant_get_double(value);
    g_variant_unref(value);

    value = g_dbus_proxy_get_cached_property(proxy, PROPERTY_STATE);
    state = g_variant_get_uint32(value);
    g_variant_unref(value);

    data->state_string = state_to_string(state);
    g_sprintf(tooltip_text, "%u%% (%s)", data->percentage, data->state_string);
    gtk_status_icon_set_tooltip_text(data->status_icon, tooltip_text);

    return proxy;
}

int main(void)
{
    applet data;
    GDBusProxy *proxy;
    GPid activate_child_pid, popup_child_pid;

    gtk_init(NULL, NULL);

    data.status_icon = gtk_status_icon_new();

    proxy = init(&data);
    if (proxy == NULL)
        return 1;

    g_signal_connect(proxy, "g-properties-changed",
                     G_CALLBACK(on_properties_changed), &data);

    activate_child_pid = 0;
    popup_child_pid = 0;

    g_signal_connect_swapped(data.status_icon, "activate",
                             G_CALLBACK(activate_cb), &activate_child_pid);
    g_signal_connect_swapped(data.status_icon, "popup-menu",
                             G_CALLBACK(popup_cb), &popup_child_pid);

    gtk_main();

    /* the code should never be executed */
    g_object_unref(proxy);
    return 0;
}
