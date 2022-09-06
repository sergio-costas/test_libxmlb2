#define I_KNOW_THE_GNOME_SOFTWARE_API_IS_SUBJECT_TO_CHANGE
#include <glib.h>
#include <xmlb.h>
#include <gobject/gobject.h>
#include "gs-appstream.h"

gboolean
gs_plugin_appstream_load_appstream (gpointer plugin,
				    XbBuilder *builder,
				    const gchar *path,
				    GCancellable *cancellable,
				    GError **error);

gboolean
gs_plugin_appstream_load_appdata (gpointer plugin,
				  XbBuilder *builder,
				  const gchar *path,
				  GCancellable *cancellable,
				  GError **error);

gboolean
gs_plugin_appstream_load_desktop (gpointer plugin,
				  XbBuilder *builder,
				  const gchar *path,
				  GCancellable *cancellable,
				  GError **error);
