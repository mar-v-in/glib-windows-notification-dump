#include <gio/gio.h>
#include <gio/gnotificationbackend.h>
#include <gio/gnotification-private.h>
#include <gtk/gtk.h>
#include <gdk/gdkwin32.h>

#define g_notification_backend_get_type() g_type_from_name("GNotificationBackend")

#define G_TYPE_G_WIN32_NOTIFICATION_BACKEND  (g_win32_notification_backend_get_type ())
#define G_WIN32_NOTIFICATION_BACKEND(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_G_WIN32_NOTIFICATION_BACKEND, GWin32ToastNotificationBackend))

typedef struct _GWin32ToastNotificationBackend GWin32ToastNotificationBackend;
typedef GNotificationBackendClass GWin32ToastNotificationBackendClass;

struct _GWin32ToastNotificationBackend {
    GNotificationBackend parent;
};

G_DEFINE_DYNAMIC_TYPE (GWin32ToastNotificationBackend, g_win32_notification_backend, G_TYPE_NOTIFICATION_BACKEND);

static gboolean
g_win32_notification_backend_is_supported (void)
{
  g_print ("g_win32_notification_backend_is_supported\n");
  return TRUE;
}

static void
g_win32_notification_backend_send_notification (GNotificationBackend *self,
                                                      const gchar *id,
                                                      GNotification *notification)
{
  g_print ("g_win32_notification_backend_send_notification: %s = %s\n", id, g_notification_get_title (notification));
}

static void
g_win32_notification_backend_withdraw_notification (GNotificationBackend *self,
                                                          const gchar *id)
{
  g_print ("g_win32_notification_backend_withdraw_notification: %s\n", id);
}

static void
g_win32_notification_backend_init (GWin32ToastNotificationBackend *self)
{

}

static void
g_win32_notification_backend_class_init (GWin32ToastNotificationBackendClass *klass)
{
  GNotificationBackendClass *backend_class = G_NOTIFICATION_BACKEND_CLASS(klass);

  backend_class->is_supported = g_win32_notification_backend_is_supported;
  backend_class->send_notification = g_win32_notification_backend_send_notification;
  backend_class->withdraw_notification = g_win32_notification_backend_withdraw_notification;
}

static void g_win32_notification_backend_class_finalize (GWin32ToastNotificationBackendClass *klass)
{

}

G_MODULE_EXPORT char *g_module_check_init ()
{
  return 0;
}

G_MODULE_EXPORT void g_io_module_load (GIOModule *module)
{
  g_win32_notification_backend_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (G_NOTIFICATION_BACKEND_EXTENSION_POINT_NAME, g_win32_notification_backend_get_type (), "win32", 1);
}

G_MODULE_EXPORT char **g_io_module_query ()
{
  return g_strsplit_set (G_NOTIFICATION_BACKEND_EXTENSION_POINT_NAME, ":", 0);
}

G_MODULE_EXPORT void g_io_module_unload (GIOModule *module)
{
}
