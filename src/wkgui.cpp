#include "wkgui.hpp"

#define NATIVE_OBJECT_NAME "sys"

static gboolean
on_delete_event(GtkWidget *widget,
				GdkEvent  *event,
				gpointer   data)
{
	return FALSE;
}

static void
load_status_notify(WebKitWebFrame *frame,
				   gboolean   arg1,
				   gpointer   data)
{
	if (webkit_web_frame_get_load_status(frame) == WEBKIT_LOAD_COMMITTED)
	{
	 	JSStringRef name;
		name = JSStringCreateWithUTF8CString(NATIVE_OBJECT_NAME);
		JSGlobalContextRef js_global_context =
			webkit_web_frame_get_global_context(frame);

		JSObjectSetProperty(js_global_context,
							JSContextGetGlobalObject(js_global_context),
							name,
							*(JSObjectRef *)data,
							0, NULL);
		JSStringRelease(name);
	}
}

void
WKGUI::SetWindowTitle(const char *title)
{
	gtk_window_set_title(GTK_WINDOW(mWindow), title);
}

void
WKGUI::SetWindowSize(int w, int h)
{
	gtk_window_resize(GTK_WINDOW(mWindow), w, h);
}

int
WKGUI::RegisterJSCallback(const char *name, WKJSCallback_f func)
{
	mJSCallbacks[name] = func;
}

int
WKGUI::Start(int *argc, char ***argv, const char *file)
{
	GtkWidget *scrolled;

	if (!g_thread_supported())
		g_thread_init(NULL);
	gtk_init_check(argc, argv);
	
	mView = WEBKIT_WEB_VIEW(webkit_web_view_new());

	char *cwd = g_get_current_dir();
	char *path = g_build_filename(cwd, file, NULL);
	char *start = g_filename_to_uri(path, NULL, NULL);
	
	webkit_web_view_load_uri(mView, start);
	
	g_free(cwd);
	g_free(path);
	g_free(start);

  	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(mView));

	mWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(mWindow), 900, 600);
	
	g_signal_connect(mWindow, "delete-event", G_CALLBACK(on_delete_event), this);
	g_signal_connect(mWindow, "destroy", G_CALLBACK(gtk_main_quit), this);

	gtk_container_add(GTK_CONTAINER(mWindow), scrolled);

	g_signal_connect(webkit_web_view_get_main_frame(mView), "notify::load-status", G_CALLBACK(load_status_notify), &mJSNativeObject);

	/* Create the native object with local callback properties */
	JSGlobalContextRef js_global_context =
		webkit_web_frame_get_global_context(webkit_web_view_get_main_frame(mView));

	JSStringRef name;

	mJSNativeContext = JSGlobalContextCreateInGroup(
		JSContextGetGroup(webkit_web_frame_get_global_context(webkit_web_view_get_main_frame(mView))),
		NULL);

	mJSNativeObject = JSObjectMake(mJSNativeContext, NULL, NULL);
	name = JSStringCreateWithUTF8CString(NATIVE_OBJECT_NAME);
	JSObjectSetProperty(mJSNativeContext,
						JSContextGetGlobalObject(mJSNativeContext),
						name,
						mJSNativeObject,
						0, NULL);
	JSStringRelease(name);

	std::map<std::string, WKJSCallback_f>::iterator i = mJSCallbacks.begin();
	for (; i != mJSCallbacks.end(); ++ i)
	{
		name = JSStringCreateWithUTF8CString(i->first.c_str());
		JSObjectSetProperty(mJSNativeContext,
							mJSNativeObject,
							name,
							JSObjectMakeFunctionWithCallback(
								mJSNativeContext, NULL,
								i->second),
							0, NULL);
		JSStringRelease(name);
	}

	gtk_widget_show_all(mWindow);
	gtk_main();

	return 0;
}
