#include "gsplugin.h"

static const gchar *
gs_plugin_appstream_convert_component_kind (const gchar *kind)
{
	if (g_strcmp0 (kind, "webapp") == 0)
		return "web-application";
	return kind;
}

static gboolean
gs_plugin_appstream_upgrade_cb (XbBuilderFixup *self,
				XbBuilderNode *bn,
				gpointer user_data,
				GError **error)
{
	if (g_strcmp0 (xb_builder_node_get_element (bn), "application") == 0) {
		g_autoptr(XbBuilderNode) id = xb_builder_node_get_child (bn, "id", NULL);
		g_autofree gchar *kind = NULL;
		if (id != NULL) {
			kind = g_strdup (xb_builder_node_get_attr (id, "type"));
			xb_builder_node_remove_attr (id, "type");
		}
		if (kind != NULL)
			xb_builder_node_set_attr (bn, "type", kind);
		xb_builder_node_set_element (bn, "component");
	} else if (g_strcmp0 (xb_builder_node_get_element (bn), "metadata") == 0) {
		xb_builder_node_set_element (bn, "custom");
	} else if (g_strcmp0 (xb_builder_node_get_element (bn), "component") == 0) {
		const gchar *type_old = xb_builder_node_get_attr (bn, "type");
		const gchar *type_new = gs_plugin_appstream_convert_component_kind (type_old);
		if (type_old != type_new)
			xb_builder_node_set_attr (bn, "type", type_new);
	}
	return TRUE;
}

static gboolean
gs_plugin_appstream_load_appdata_fn (gpointer plugin,
				     XbBuilder *builder,
				     const gchar *filename,
				     GCancellable *cancellable,
				     GError **error)
{
	g_autoptr(GFile) file = g_file_new_for_path (filename);
	g_autoptr(XbBuilderFixup) fixup = NULL;
	g_autoptr(XbBuilderNode) info = NULL;
	g_autoptr(XbBuilderSource) source = xb_builder_source_new ();

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

	/* fix up any legacy installed files */
	fixup = xb_builder_fixup_new ("AppStreamUpgrade2",
				      gs_plugin_appstream_upgrade_cb,
				      plugin, NULL);
	xb_builder_fixup_set_max_depth (fixup, 3);
	xb_builder_source_add_fixup (source, fixup);

	/* add metadata */
	info = xb_builder_node_insert (NULL, "info", NULL);
	xb_builder_node_insert_text (info, "filename", filename, NULL);
	xb_builder_source_set_info (source, info);

	/* success */
	xb_builder_import_source (builder, source);
	return TRUE;
}

gboolean
gs_plugin_appstream_load_appdata (gpointer plugin,
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
		if (g_str_has_suffix (fn, ".appdata.xml") ||
		    g_str_has_suffix (fn, ".metainfo.xml")) {
			g_autofree gchar *filename = g_build_filename (path, fn, NULL);
			g_autoptr(GError) error_local = NULL;
			if (!gs_plugin_appstream_load_appdata_fn (plugin,
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
