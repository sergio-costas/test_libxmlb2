#include "gsplugin.h"

/*
Data folders:

Path: /snap/snap-store/current/usr/share/app-info/xmls     -> xmls1
Path: /snap/snap-store/current/usr/share/app-info/yaml     -> ----
Path: /snap/snap-store/current/usr/var/cache/app-info/xmls -> ----
Path: /snap/snap-store/current/usr/var/cache/app-info/yaml -> ----
Path: /snap/snap-store/current/usr/var/lib/app-info/xmls   -> ----
Path: /snap/snap-store/current/usr/var/lib/app-info/yaml   -> ----
Path: /usr/share/app-info/xmls                             -> xmls4
Path: /usr/share/app-info/yaml                             -> ----
Path: /var/cache/app-info/xmls                             -> xmls5
Path: /var/cache/app-info/yaml                             -> ----
Path: /var/lib/app-info/xmls                               -> ----
Path: /var/lib/app-info/yaml                               -> yaml6
Path2: /snap/snap-store/current/usr/share/appdata          -> ----
Path2: /snap/snap-store/current/usr/share/metainfo         -> metainfo1
Path2: /usr/share/appdata                                  -> appdata2
Path2: /usr/share/metainfo                                 -> metainfo2
/var/lib/snapd/hostfs/usr/share/applications               -> hostfsapplications
*/

gboolean test() {
	const gchar *test_xml;
	g_autofree gchar *blobfn = NULL;
	g_autoptr(XbBuilder) builder = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GRWLockReaderLocker) reader_locker = NULL;
	g_autoptr(GRWLockWriterLocker) writer_locker = NULL;
	g_autoptr(GPtrArray) parent_appdata = g_ptr_array_new_with_free_func (g_free);
	g_autoptr(GPtrArray) parent_appstream = g_ptr_array_new_with_free_func (g_free);
	const gchar *const *locales = g_get_language_names ();
	g_autoptr(GMainContext) old_thread_default = NULL;

	/* FIXME: https://gitlab.gnome.org/GNOME/gnome-software/-/issues/1422 */
	old_thread_default = g_main_context_ref_thread_default ();
	if (old_thread_default == g_main_context_default ())
		g_clear_pointer (&old_thread_default, g_main_context_unref);
	if (old_thread_default != NULL)
		g_main_context_pop_thread_default (old_thread_default);
	builder = xb_builder_new ();
	if (old_thread_default != NULL)
		g_main_context_push_thread_default (old_thread_default);
	g_clear_pointer (&old_thread_default, g_main_context_unref);

	/* verbose profiling */
	if (g_getenv ("GS_XMLB_VERBOSE") != NULL) {
		xb_builder_set_profile_flags (builder,
					      XB_SILO_PROFILE_FLAG_XPATH |
					      XB_SILO_PROFILE_FLAG_DEBUG);
	}

	/* add current locales */
	for (guint i = 0; locales[i] != NULL; i++)
		xb_builder_add_locale (builder, locales[i]);


    g_ptr_array_add (parent_appstream, "test_files/xmls1");
    g_ptr_array_add (parent_appstream, "test_files/xmls4");
    g_ptr_array_add (parent_appstream, "test_files/xmls5");
    g_ptr_array_add (parent_appstream, "test_files/yaml6");
    g_ptr_array_add (parent_appdata, "test_files/metainfo1");
    g_ptr_array_add (parent_appdata, "test_files/metainfo2");
    g_ptr_array_add (parent_appdata, "test_files/appdata2");

    /* import all files */
    for (guint i = 0; i < parent_appstream->len; i++) {
        const gchar *fn = g_ptr_array_index (parent_appstream, i);
        g_print("Loading %s\n", fn);
        if (!gs_plugin_appstream_load_appstream (NULL, builder, fn,
                                NULL, NULL))
            return FALSE;
    }
    for (guint i = 0; i < parent_appdata->len; i++) {
        const gchar *fn = g_ptr_array_index (parent_appdata, i);
        g_print("Loading2 %s\n", fn);
        if (!gs_plugin_appstream_load_appdata (NULL, builder, fn,
                                NULL, NULL))
            return FALSE;
    }
    if (!gs_plugin_appstream_load_desktop (NULL, builder,
                            "test_files/hostfsapplications",
                            NULL, NULL)) {
        return FALSE;
    }
    g_print("\nLoading3 test_files/hostfsapplications\n\n");
    /*if (g_strcmp0 (DATADIR, "/usr/share") != 0 &&
        !gs_plugin_appstream_load_desktop (NULL, builder,
                            "/var/lib/snapd/hostfs/usr/share/applications",
                            NULL, NULL)) {
        return FALSE;
    }*/

	/* regenerate with each minor release */
	xb_builder_append_guid (builder, "1");

	/* create per-user cache */
	blobfn = ("/tmp/components.xmlb");
	file = g_file_new_for_path (blobfn);

	g_print("\nProcessing\n\n");
	xb_builder_ensure (builder, file,
					XB_BUILDER_COMPILE_FLAG_IGNORE_INVALID |
					XB_BUILDER_COMPILE_FLAG_SINGLE_LANG,
					NULL, NULL);
	// remove the cache file to ensure that the tree is rebuilt from scratch
    system("rm -f /tmp/components.xmlb");
    g_print("Loaded and processed everything\n");
}


int main() {
    test();
    return 0;
}
