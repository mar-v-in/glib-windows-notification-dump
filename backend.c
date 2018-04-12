#include <gio/gio.h>
#include <gio/gnotificationbackend.h>
#include <gio/gnotification-private.h>
#include <gtk/gtk.h>
#include <gdk/gdkwin32.h>

#define WINOTIFY_EXPORT
#include <winotify.h>

#include <windows.h>
#include <strsafe.h>

#define g_notification_backend_get_type() g_type_from_name("GNotificationBackend")

#define G_TYPE_G_WIN32_NOTIFICATION_BACKEND  (g_win32_notification_backend_get_type ())
#define G_WIN32_NOTIFICATION_BACKEND(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_G_WIN32_NOTIFICATION_BACKEND, GWin32NotificationBackend))

typedef struct _GWin32NotificationBackend GWin32NotificationBackend;
typedef GNotificationBackendClass GWin32NotificationBackendClass;

struct _GWin32NotificationBackend {
    GNotificationBackend parent;
};

G_DEFINE_DYNAMIC_TYPE (GWin32NotificationBackend, g_win32_notification_backend, G_TYPE_NOTIFICATION_BACKEND);

static gboolean
g_win32_notification_backend_is_supported (void)
{
  g_print ("g_win32_notification_backend_is_supported\n");
  return TRUE; // TODO winotify_is_supported ();
}

static gboolean g_win32_notification_backend_invoke_action (gpointer detailed_action)
{
  gchar *action;
  GVariant *target;

  g_action_parse_detailed_name (detailed_action, &action, &target, NULL);

  if (strlen (action) > 4)
    g_action_group_activate_action (G_ACTION_GROUP (g_application_get_default ()), action + 4, target);

  g_free (action);
  g_free (detailed_action);
  g_variant_unref (target);
  return FALSE;
}

static void g_win32_notification_backend_default_action (GNotification *notification)
{
  gchar *action;
  GVariant *target;
  g_notification_get_default_action (notification, &action, &target);
  g_print ("default_action: %s\n", action + 4);

  g_idle_add (g_win32_notification_backend_invoke_action, g_action_print_detailed_name (action, target));

  g_free (action);
  g_variant_unref (target);
}

static gchar *G_WIN32_NOTIFICATION_BACKEND_DEFAULT_IMAGE = "";

static void
g_win32_notification_backend_send_notification (GNotificationBackend *self,
                                                      const gchar *id,
                                                      GNotification *notification)
{
  g_print ("g_win32_notification_backend_send_notification: %s = %s\n", id, g_notification_get_title (notification));
  WinotifyToast *toast = winotify_toast_new ();
  if (!toast)
    {
      g_print ("winotify failed\n");
      return;
    }
  toast->app_id = "CN44E2EAD8-8680-4DCF-AE68.DinoJabberXMPP_qnez0jxdbn5z0!im.dino"; //g_strdup (g_application_get_application_id (g_application_get_default ()));
  GIcon *icon = g_notification_get_icon (notification);
  gchar *image = G_WIN32_NOTIFICATION_BACKEND_DEFAULT_IMAGE;
  if (G_IS_FILE_ICON (icon))
    {
      GFile *file = g_file_icon_get_file (G_FILE_ICON (icon));
      char *file_path = g_file_get_path (file);
      image = g_strdup_printf ("<image placement=\"appLogoOverride\" src=\"file://%s\" />", file_path);
      g_free (file_path);
    }
  /*else if (G_IS_BYTES_ICON (icon))
    {
      GBytes *bytes = g_bytes_icon_get_bytes (G_BYTES_ICON (icon));
      gsize size;
      const guint8 *data = g_bytes_get_data (bytes, &size);
      gchar *b64 = g_base64_encode (data, size);
      image = g_strdup_printf ("<image placement=\"appLogoOverride\" src=\"data:image/png;base64,%s\"/>", b64);
      g_free (b64);
    }*/

// This is just some ugly experiments with Windows, trying to push the Dino window to foreground on notification click

  gchar self_name[MAX_PATH + 1];
  GetModuleFileName (NULL, self_name, MAX_PATH + 1);
  DWORD self_process_id = GetCurrentProcessId ();
  void *window_ptr = gdk_win32_window_get_handle (gtk_widget_get_window (GTK_WIDGET (gtk_application_get_active_window (GTK_APPLICATION (g_application_get_default ())))));

  toast->xml = g_strdup_printf ("<toast launch=\"--winotify-process-name=%s --winotify-process-id=%ld --winotify-window-id=%ld\">"
                                    "<visual>"
                                    "<binding template=\"ToastGeneric\">"
                                    "<text hint-maxLines=\"1\">%s</text>"
                                    "<text>%s</text>"
                                    "%s"
                                    "</binding>"
                                    "</visual>"
                                    "</toast>",
                                self_name,
                                self_process_id,
                                (long) window_ptr,
                                g_notification_get_title (notification),
                                g_notification_get_body (notification),
                                image);
  toast->on_click = (WinotifyToastClickListener) g_win32_notification_backend_default_action;
  toast->on_click_target = g_object_ref (notification);
  toast->on_click_target_destroy_notify = g_object_unref;

  g_print ("Winotify: %s\n", toast->xml);
  winotify_toast_display (toast);

  winotify_toast_unref (toast);
  if (image != G_WIN32_NOTIFICATION_BACKEND_DEFAULT_IMAGE) g_free (image);
}

static void
g_win32_notification_backend_withdraw_notification (GNotificationBackend *self,
                                                          const gchar *id)
{
  g_print ("g_win32_notification_backend_withdraw_notification: %s\n", id);
}

static void
g_win32_notification_backend_init (GWin32NotificationBackend *self)
{
  g_print ("g_win32_notification_backend_init\n");

}

static void
g_win32_notification_backend_class_init (GWin32NotificationBackendClass *klass)
{
  GNotificationBackendClass *backend_class = G_NOTIFICATION_BACKEND_CLASS(klass);
  g_print ("g_win32_notification_backend_class_init\n");

  backend_class->is_supported = g_win32_notification_backend_is_supported;
  backend_class->send_notification = g_win32_notification_backend_send_notification;
  backend_class->withdraw_notification = g_win32_notification_backend_withdraw_notification;
}

static void g_win32_notification_backend_class_finalize (GWin32NotificationBackendClass *klass)
{

}

G_MODULE_EXPORT char *g_module_check_init ()
{
  return 0;
}

G_MODULE_EXPORT void g_io_module_load (GIOModule *module)
{
  g_win32_notification_backend_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (G_NOTIFICATION_BACKEND_EXTENSION_POINT_NAME, g_win32_notification_backend_get_type (), "win32_toast", 1000);

  gchar **command_line = g_win32_get_command_line ();
  gchar *process_name = NULL;
  DWORD process_id = 0;
  gchar self_name[MAX_PATH + 1];
  GetModuleFileName (NULL, self_name, MAX_PATH + 1);
  void *window_ptr = 0;

  for (int i = 0; command_line[i] != NULL; ++i)
    {
      g_print ("Command: %s\n", command_line[i]);
      if (g_strrstr (command_line[i], "--winotify-process-name"))
        process_name = command_line[i] + 24;
      if (g_strrstr (command_line[i], "--winotify-process-id"))
        process_id = (DWORD) atol (command_line[i] + 22);
      if (g_strrstr (command_line[i], "--winotify-window-id"))
        window_ptr = (void *) atol (command_line[i] + 21);
    }

// Technically we should be able to use AllowSetForegroundWindow or SetForegroundWindow here to bring Dino to foreground,
// but it seems to not work (but does not return failure code)
// maybe an issue with GTK window management - should experiment with disabling CSD

  g_strfreev (command_line);
}

G_MODULE_EXPORT char **g_io_module_query ()
{
  g_print ("g_io_module_query\n");
  return g_strsplit_set (G_NOTIFICATION_BACKEND_EXTENSION_POINT_NAME, ":", 0);
}

G_MODULE_EXPORT void g_io_module_unload (GIOModule *module)
{
  g_print ("g_io_module_unload\n");
}
