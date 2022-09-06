#include "gsplugin.h"

static GInputStream *
gs_plugin_appstream_load_desktop_cb (XbBuilderSource *self,
				     XbBuilderSourceCtx *ctx,
				     gpointer user_data,
				     GCancellable *cancellable,
				     GError **error)
{
	g_autofree gchar *xml = NULL;
	g_autoptr(AsComponent) cpt = as_component_new ();
	g_autoptr(AsContext) actx = as_context_new ();
	g_autoptr(GBytes) bytes = NULL;
	gboolean ret;

	bytes = xb_builder_source_ctx_get_bytes (ctx, cancellable, error);
	if (bytes == NULL)
		return NULL;

	as_component_set_id (cpt, xb_builder_source_ctx_get_filename (ctx));
	ret = as_component_load_from_bytes (cpt,
					   actx,
					   AS_FORMAT_KIND_DESKTOP_ENTRY,
					   bytes,
					   error);
	if (!ret)
		return NULL;
	xml = as_component_to_xml_data (cpt, actx, error);
	if (xml == NULL)
		return NULL;
	return g_memory_input_stream_new_from_data (g_steal_pointer (&xml), -1, g_free);
}

static gboolean
gs_plugin_appstream_load_desktop_fn (gpointer plugin,
				     XbBuilder *builder,
				     const gchar *filename,
				     GCancellable *cancellable,
				     GError **error)
{
	g_autoptr(GFile) file = g_file_new_for_path (filename);
	g_autoptr(XbBuilderNode) info = NULL;
	g_autoptr(XbBuilderSource) source = xb_builder_source_new ();

	/* add support for desktop files */
	xb_builder_source_add_adapter (source, "application/x-desktop",
				       gs_plugin_appstream_load_desktop_cb, NULL, NULL);

	/* add source */
	if (!xb_builder_source_load_file (source, file,
#if LIBXMLB_CHECK_VERSION(0, 2, 0)
					  XB_BUILDER_SOURCE_FLAG_WATCH_DIRECTORY,
#else
					  XB_BUILDER_SOURCE_FLAG_WATCH_FILE,
#endif
					  cancellable,
					  error)) {
		return FALSE;
	}

	/* add metadata */
	info = xb_builder_node_insert (NULL, "info", NULL);
	xb_builder_node_insert_text (info, "filename", filename, NULL);
	xb_builder_source_set_info (source, info);

	/* success */
	xb_builder_import_source (builder, source);
	return TRUE;
}

gboolean
gs_plugin_appstream_load_desktop (gpointer plugin,
				  XbBuilder *builder,
				  const gchar *path,
				  GCancellable *cancellable,
				  GError **error)
{
	const gchar *fn;
	g_autoptr(GDir) dir = NULL;
	g_autoptr(GFile) parent = g_file_new_for_path (path);
	if (!g_file_query_exists (parent, cancellable))
		return TRUE;

	dir = g_dir_open (path, 0, error);
	if (dir == NULL)
		return FALSE;

	while ((fn = g_dir_read_name (dir)) != NULL) {
		if (g_str_has_suffix (fn, ".desktop")) {
			g_autofree gchar *filename = g_build_filename (path, fn, NULL);
			g_autoptr(GError) error_local = NULL;
			if (g_strcmp0 (fn, "mimeinfo.cache") == 0)
				continue;
			if (!gs_plugin_appstream_load_desktop_fn (plugin,
								  builder,
								  filename,
								  cancellable,
								  &error_local)) {
				g_debug ("ignoring %s: %s", filename, error_local->message);
				continue;
			}
		}
	}

	/* success */
	return TRUE;
}
