#include "gsplugin.h"

static const gchar *
gs_plugin_appstream_convert_component_kind (const gchar *kind)
{
	if (g_strcmp0 (kind, "webapp") == 0)
		return "web-application";
	return kind;
}

#if LIBXMLB_CHECK_VERSION(0,3,1)
static gboolean
gs_plugin_appstream_tokenize_cb (XbBuilderFixup *self,
				 XbBuilderNode *bn,
				 gpointer user_data,
				 GError **error)
{
	const gchar * const elements_to_tokenize[] = {
		"id",
		"keyword",
		"launchable",
		"mimetype",
		"name",
		"pkgname",
		"summary",
		NULL };
	if (xb_builder_node_get_element (bn) != NULL &&
	    g_strv_contains (elements_to_tokenize, xb_builder_node_get_element (bn)))
		xb_builder_node_tokenize_text (bn);
	return TRUE;
}
#endif

static gboolean
gs_plugin_appstream_add_origin_keyword_cb (XbBuilderFixup *self,
					   XbBuilderNode *bn,
					   gpointer user_data,
					   GError **error)
{
	if (g_strcmp0 (xb_builder_node_get_element (bn), "components") == 0) {
		const gchar *origin = xb_builder_node_get_attr (bn, "origin");
		GPtrArray *components = xb_builder_node_get_children (bn);
		if (origin == NULL || origin[0] == '\0')
			return TRUE;
		g_debug ("origin %s has %u components", origin, components->len);
		if (components->len < 200) {
			for (guint i = 0; i < components->len; i++) {
				XbBuilderNode *component = g_ptr_array_index (components, i);
				gs_appstream_component_add_keyword (component, origin);
			}
		}
	}
	return TRUE;
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
gs_plugin_appstream_add_icons_cb (XbBuilderFixup *self,
				  XbBuilderNode *bn,
				  gpointer user_data,
				  GError **error)
{
	if (g_strcmp0 (xb_builder_node_get_element (bn), "component") != 0)
		return TRUE;
	gs_appstream_component_add_extra_info (bn);
	return TRUE;
}

static GInputStream *
gs_plugin_appstream_load_dep11_cb (XbBuilderSource *self,
				   XbBuilderSourceCtx *ctx,
				   gpointer user_data,
				   GCancellable *cancellable,
				   GError **error)
{
	g_autoptr(AsMetadata) mdata = as_metadata_new ();
	g_autoptr(GBytes) bytes = NULL;
	g_autoptr(GError) tmp_error = NULL;
	g_autofree gchar *xml = NULL;

	bytes = xb_builder_source_ctx_get_bytes (ctx, cancellable, error);
	if (bytes == NULL)
		return NULL;

	as_metadata_set_format_style (mdata, AS_FORMAT_STYLE_COLLECTION);
	as_metadata_parse_bytes (mdata,
				 bytes,
				 AS_FORMAT_KIND_YAML,
				 &tmp_error);
	if (tmp_error != NULL) {
		g_propagate_error (error, g_steal_pointer (&tmp_error));
		return NULL;
	}

	xml = as_metadata_components_to_collection (mdata, AS_FORMAT_KIND_XML, &tmp_error);
	if (xml == NULL) {
		// This API currently returns NULL if there is nothing to serialize, so we
		// have to test if this is an error or not.
		// See https://gitlab.gnome.org/GNOME/gnome-software/-/merge_requests/763
		// for discussion about changing this API.
		if (tmp_error != NULL) {
			g_propagate_error (error, g_steal_pointer (&tmp_error));
			return NULL;
		}

		xml = g_strdup("");
	}

	return g_memory_input_stream_new_from_data (g_steal_pointer (&xml), -1, g_free);
}

static gboolean
gs_plugin_appstream_load_appstream_fn (gpointer plugin,
				       XbBuilder *builder,
				       const gchar *filename,
				       GCancellable *cancellable,
				       GError **error)
{
	g_autoptr(GFile) file = g_file_new_for_path (filename);
	g_autoptr(XbBuilderNode) info = NULL;
	g_autoptr(XbBuilderFixup) fixup1 = NULL;
	g_autoptr(XbBuilderFixup) fixup2 = NULL;
	g_autoptr(XbBuilderFixup) fixup3 = NULL;
#if LIBXMLB_CHECK_VERSION(0,3,1)
	g_autoptr(XbBuilderFixup) fixup4 = NULL;
#endif
	g_autoptr(XbBuilderSource) source = xb_builder_source_new ();

	/* add support for DEP-11 files */
	xb_builder_source_add_adapter (source,
				       "application/x-yaml",
				       gs_plugin_appstream_load_dep11_cb,
				       NULL, NULL);

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
	xb_builder_node_insert_text (info, "scope", "system", NULL);
	xb_builder_node_insert_text (info, "filename", filename, NULL);
	xb_builder_source_set_info (source, info);

	/* add missing icons as required */
	fixup1 = xb_builder_fixup_new ("AddIcons",
				       gs_plugin_appstream_add_icons_cb,
				       plugin, NULL);
	xb_builder_fixup_set_max_depth (fixup1, 2);
	xb_builder_source_add_fixup (source, fixup1);

	/* fix up any legacy installed files */
	fixup2 = xb_builder_fixup_new ("AppStreamUpgrade2",
				       gs_plugin_appstream_upgrade_cb,
				       plugin, NULL);
	xb_builder_fixup_set_max_depth (fixup2, 3);
	xb_builder_source_add_fixup (source, fixup2);

	/* add the origin as a search keyword for small repos */
	fixup3 = xb_builder_fixup_new ("AddOriginKeyword",
				       gs_plugin_appstream_add_origin_keyword_cb,
				       plugin, NULL);
	xb_builder_fixup_set_max_depth (fixup3, 1);
	xb_builder_source_add_fixup (source, fixup3);

#if LIBXMLB_CHECK_VERSION(0,3,1)
	fixup4 = xb_builder_fixup_new ("TextTokenize",
				       gs_plugin_appstream_tokenize_cb,
				       NULL, NULL);
	xb_builder_fixup_set_max_depth (fixup4, 2);
	xb_builder_source_add_fixup (source, fixup4);
#endif

	/* success */
	xb_builder_import_source (builder, source);
	return TRUE;
}

gboolean
gs_plugin_appstream_load_appstream (gpointer plugin,
				    XbBuilder *builder,
				    const gchar *path,
				    GCancellable *cancellable,
				    GError **error)
{
	const gchar *fn;
	g_autoptr(GDir) dir = NULL;
	g_autoptr(GFile) parent = g_file_new_for_path (path);

	/* parent patch does not exist */
	if (!g_file_query_exists (parent, cancellable))
		return TRUE;
	dir = g_dir_open (path, 0, error);
	if (dir == NULL)
		return FALSE;
	while ((fn = g_dir_read_name (dir)) != NULL) {
		if (g_str_has_suffix (fn, ".xml") ||
		    g_str_has_suffix (fn, ".yml") ||
		    g_str_has_suffix (fn, ".yml.gz") ||
		    g_str_has_suffix (fn, ".xml.gz")) {
			g_autofree gchar *filename = g_build_filename (path, fn, NULL);
			g_autoptr(GError) error_local = NULL;
			if (!gs_plugin_appstream_load_appstream_fn (plugin,
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
