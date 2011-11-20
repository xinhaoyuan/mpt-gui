#ifndef __WKGUI_HPP__
#define __WKGUI_HPP__

#include <webkit/webkit.h>
#include <JavaScriptCore/JavaScript.h>

#include <string>
#include <map>

class WKGUI
{	
	typedef JSValueRef(*WKJSCallback_f)(JSContextRef     context,
										JSObjectRef      function,
										JSObjectRef      self,
										size_t           argc,
										const JSValueRef argv[],
										JSValueRef*      exception);

	std::string mStartFile;
	std::map<std::string, WKJSCallback_f> mJSCallbacks;

	GtkWidget     *mWindow;
	WebKitWebView *mView;
	JSGlobalContextRef mJSNativeContext;
	JSObjectRef    mJSNativeObject;

public:

	int RegisterJSCallback(const char *name, WKJSCallback_f func);
	int Start(int *argc, char ***argv, const char *file);

	void SetWindowSize(int w, int h);
	void SetWindowTitle(const char *title);
};

#endif
