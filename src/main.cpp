#include <webkit/webkit.h>
#include <JavaScriptCore/JavaScript.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cstring>
#include <cstdlib>

using namespace std;

#include <db_cxx.h>

Db *db;

#if __DEBUG
static void DBPUT(DbTxn *txn, Dbt *k, Dbt *v, unsigned int flags)
{
	cout << "DBPUT ";
	for (int i = 0; i != k->get_size(); ++ i)
	{
		cout << (int)((unsigned char *)k->get_data())[i] << ' ';
	}
	cout << endl;
	db->put(txn, k, v, flags);
}
#else
#define DBPUT db->put
#endif

#define KEY_IDCOUNT      0
#define KEY_ID2TAGSTRING 1
#define KEY_PLAYLINK2ID  2
#define KEY_ID2PLAYLINK  3

#define START_FILE   "ui/main.html"
#define WINDOW_TITLE "Music Play Tree GUI"

typedef long long handle_id;

struct View
{
	View   *mParent;
	string mName;
	string mFilter;
	map<string, View *> mSubviews;
	set<handle_id>      mItems;
};

View root;

static void
FreeView(View *view)
{
	map<string, View *>::iterator it;
	for(it = view->mSubviews.begin(); it != view->mSubviews.end(); ++ it)
	{
		FreeView(it->second);
	}
	if (view != &root) delete view;
}

static void
WriteView(ostream &o, View *view)
{
	o << "{ \"name\":\"" + view->mName + "\", \"filter\":\"" + view->mFilter + "\", \"items\":[";
	set<handle_id>::iterator item_it;
	bool nonfirst = false;
	for (item_it = view->mItems.begin(); item_it != view->mItems.end(); ++ item_it)
	{
		handle_id id = *item_it;
		struct
		{
			char      header;
			handle_id id;
		} __attribute__((packed)) __key;

		__key.id = id;
		__key.header = KEY_ID2TAGSTRING;
		
		Dbt value;
		Dbt key((void *)&__key, sizeof(__key));
		if (db->get(NULL, &key, &value, 0) == DB_NOTFOUND)
		{
			continue;
		}
		
		if (nonfirst)
		{
			o << ',';
		}
		else nonfirst = true;

		string tagsstring((const char *)value.get_data(), value.get_size());
		istringstream tags(tagsstring);
		string tagname;
		ostringstream tagvalue_is;
		
		o << "{ \"id\": " << id;
		while (!(tags >> tagname).fail())
		{
			tagvalue_is.str("");
			char c;
			while (!(tags.get(c)).fail())
			{
				if (c == '"') break;
				else if (c != ' ') cout << tagsstring << " "  << c << endl;
			}
			while (!(tags.get(c)).fail())
			{
				if (c == '\\')
				{
					tagvalue_is << c;
					tags >> c;
				} else if (c == '"') break;
				tagvalue_is << c;
			}
			o << ", \"" << tagname << "\":\"" << tagvalue_is.str() << "\"";
		}
		o << " }";
	}
	
	o << "], \"subviews\":[";
	map<string, View *>::iterator it;
	for(it = view->mSubviews.begin(); it != view->mSubviews.end(); ++ it)
	{
		if (it != view->mSubviews.begin())
		{
			o << ',';
		}
		WriteView(o, it->second);
	}
	o << "] }";
}

#include <taglib/taglib.h>
#include <taglib/fileref.h>
#include <taglib/tstring.h>
#include <taglib/tag.h>

static ostream &
Escape(ostream &o, const string &s)
{
	for (int i = 0; i < s.length(); ++ i)
	{
		if (s[i] == '\\' || s[i] == '"')
			o << '\\';
		o << s[i];
	}
	return o;
}

static int
GetTagLine(const string &file, ostream &line)
{
	if (file.length() == 0) return -1;

	TagLib::FileRef f(file.c_str());
	if (f.tag() == NULL) return -1;
	
	TagLib::String  title  = f.tag()->title();
	TagLib::String  artist = f.tag()->artist();
	TagLib::String  album  = f.tag()->album();
	TagLib::String  genre  = f.tag()->genre();
	int             year   = f.tag()->year();
	
	line << "Title \""  ; Escape(line, title.to8Bit(true))  << "\" ";
	line << "Artist \"" ; Escape(line, artist.to8Bit(true)) << "\" ";
	line << "Album \""  ; Escape(line, album.to8Bit(true))  << "\" ";
	line << "Genre \""  ; Escape(line, genre.to8Bit(true))  << "\" ";
	
	line << "Year \""   <<  year << "\" ";

	return 0;
}

static void
UpdateDb(istream& in, ostream& out)
{
	string line;
	while (!(getline(in, line)).eof())
	{
		ostringstream os;
		if (GetTagLine(line, os) != 0) continue;

		bool is_new = false;
		
		string tagline;
		tagline = os.str();

		struct
		{
			char header;
			handle_id id;
		} __attribute__((packed)) __key;
		
		line.insert(line.begin(), KEY_PLAYLINK2ID);
		Dbt key((void *)line.c_str(), line.length());
		Dbt pvalue;
		handle_id id;

		if (db->get(NULL, &key, &pvalue, 0) == DB_NOTFOUND)
		{
			handle_id count;
			
			char __count_key = KEY_IDCOUNT;
			key.set_data((void *)&__count_key);
			key.set_size(1);
			Dbt value;
			if (db->get(NULL, &key, &value, 0) == DB_NOTFOUND)
				count = 0;
			else memcpy(&count, value.get_data(), sizeof(count));

			++ count;

			// Set new count
			value.set_data(&count);
			value.set_size(sizeof(count));
			DBPUT(NULL, &key, &value, 0);

			-- count;

			// Set playlink2id
			key.set_data((void *)line.c_str());
			key.set_size(line.length());
			DBPUT(NULL, &key, &value, 0);

			id = count ++;
			
			// Set id2playlink
			struct
			{
				char header;
				handle_id id;
			} __attribute__((packed)) __key;

			__key.header = KEY_ID2PLAYLINK;
			__key.id = id;

			key.set_data((void *)&__key);
			key.set_size(sizeof(__key));

			value.set_data((void *)(line.c_str() + 1));
			value.set_size(line.length() - 1);
			DBPUT(NULL, &key, &value, 0);

			is_new = true;
		}
		else
		{
			memcpy(&id, pvalue.get_data(), sizeof(id));
		}

		__key.header = KEY_ID2TAGSTRING;
		__key.id = id;
		
		key.set_data((void *)&__key);
		key.set_size(sizeof(__key));

		Dbt value((void *)tagline.c_str(), tagline.length());
		DBPUT(NULL, &key, &value, 0);

		out << (is_new ? "+ " : "* ") << id << ' ' << tagline << endl;
	}
	db->sync(0);
}

static JSValueRef
native_play_tree_insert(JSContextRef     context,
						JSObjectRef      function,
						JSObjectRef      self,
						size_t           argc,
						const JSValueRef argv[],
						JSValueRef*      exception)
{
	if (argc != 2)
		return JSValueMakeNull(context);
	
	JSStringRef js_filename;
	js_filename = JSValueToStringCopy(context, argv[0], NULL);
#define BUF_SIZE 256
	char filename_buf[BUF_SIZE];
	int size = JSStringGetUTF8CString(js_filename, filename_buf, BUF_SIZE);
	JSStringRelease(js_filename);

	ifstream in(filename_buf);

	JSStringRef js_name = JSStringCreateWithUTF8CString("length");
	JSObjectRef arr = JSValueToObject(context, argv[1], NULL);
	JSValueRef js_length = JSObjectGetProperty(context, arr, js_name, NULL);
	JSStringRelease(js_name);

	ofstream cs(".changeset");
	unsigned length = JSValueToNumber(context, js_length, NULL);
	unsigned i;
	for (i = 0; i < length; ++ i)
	{
		JSValueRef js_id = JSObjectGetPropertyAtIndex(context, arr, i, NULL);
		handle_id id = JSValueToNumber(context, js_id, NULL);
		Dbt value;
		struct
		{
			char      header;
			handle_id id;
		} __attribute__((packed)) __key;

		__key.id = id;
		__key.header = KEY_ID2TAGSTRING;

		Dbt key((void *)&__key, sizeof(__key));
		if (db->get(NULL, &key, &value, 0) == DB_NOTFOUND)
		{
			continue;
		}
		cs << "+ " << id  << ' ' << string((const char *)value.get_data(), value.get_size()) << endl;
	}
	cs.close();

	ostringstream cmd;
	cmd << "./library run_mpt .changeset +";

	if (strcmp(filename_buf, "[LIBRARY]") == 0)
		cmd << ".library";
	else cmd << "managed/" << filename_buf;

	int ret;
	GError *err;
	g_spawn_command_line_sync(cmd.str().c_str(), NULL, NULL, &ret, &err);
	
	return JSValueMakeNull(context);
}

static JSValueRef
native_send_to_player(JSContextRef     context,
					  JSObjectRef      function,
					  JSObjectRef      self,
					  size_t           argc,
					  const JSValueRef argv[],
					  JSValueRef*      exception)
{
	if (argc != 1)
		return JSValueMakeNull(context);
	
	JSStringRef js_name = JSStringCreateWithUTF8CString("length");
	JSObjectRef arr = JSValueToObject(context, argv[0], NULL);
	JSValueRef  js_length = JSObjectGetProperty(context, arr, js_name, NULL);
	JSStringRelease(js_name);

	ofstream cs(".playlist");
	unsigned length = JSValueToNumber(context, js_length, NULL);
	unsigned i;
	for (i = 0; i < length; ++ i)
	{
		JSValueRef js_id = JSObjectGetPropertyAtIndex(context, arr, i, NULL);
		handle_id id = JSValueToNumber(context, js_id, NULL);
		Dbt value;
		struct
		{
			char      header;
			handle_id id;
		} __attribute__((packed)) __key;

		__key.id = id;
		__key.header = KEY_ID2PLAYLINK;

		Dbt key((void *)&__key, sizeof(__key));
		if (db->get(NULL, &key, &value, 0) == DB_NOTFOUND)
		{
			continue;
		}
		cs << string((const char *)value.get_data(), value.get_size()) << endl;
	}
	cs.close();

	ostringstream cmd;
	cmd << "./library play .playlist";

	int ret;
	GError *err;
	g_spawn_command_line_sync(cmd.str().c_str(), NULL, NULL, &ret, &err);
	
	return JSValueMakeNull(context);
}

static JSValueRef
native_reset_play_tree(JSContextRef     context,
					   JSObjectRef      function,
					   JSObjectRef      self,
					   size_t           argc,
					   const JSValueRef argv[],
					   JSValueRef*      exception)
{
	if (argc != 2)
		return JSValueMakeNull(context);
	
	JSStringRef js_string;
	int size;
#define BUF_SIZE 256
	char string_buf[BUF_SIZE];
	
	js_string = JSValueToStringCopy(context, argv[0], NULL);
	size = JSStringGetUTF8CString(js_string, string_buf, BUF_SIZE);
	string name(string_buf, size);
	JSStringRelease(js_string);


	js_string = JSValueToStringCopy(context, argv[0], NULL);
	size = JSStringGetUTF8CString(js_string, string_buf, BUF_SIZE);
	string filter(string_buf, size);
	JSStringRelease(js_string);

	ostringstream fns;
	if (name != "[LIBRARY]")
		fns << "managed/" << name;
	else return JSValueMakeNull(context);

	ofstream out(fns.str().c_str());
	out << filter << endl;
	out.close();
}

static JSValueRef
native_remove_play_tree(JSContextRef     context,
						JSObjectRef      function,
						JSObjectRef      self,
						size_t           argc,
						const JSValueRef argv[],
						JSValueRef*      exception)
{
	if (argc != 1)
		return JSValueMakeNull(context);
	
	JSStringRef js_string;
	int size;
#define BUF_SIZE 256
	char string_buf[BUF_SIZE];
	
	js_string = JSValueToStringCopy(context, argv[0], NULL);
	size = JSStringGetUTF8CString(js_string, string_buf, BUF_SIZE);
	string name(string_buf, size);
	JSStringRelease(js_string);

	g_remove(name.c_str());
	return JSValueMakeNull(context);
}

static JSValueRef
native_open_play_tree(JSContextRef     context,
					  JSObjectRef      function,
					  JSObjectRef      self,
					  size_t           argc,
					  const JSValueRef argv[],
					  JSValueRef*      exception)
{
	if (argc != 1)
		return JSValueMakeNull(context);
	
	JSStringRef js_filename;
	js_filename = JSValueToStringCopy(context, argv[0], NULL);
#define BUF_SIZE 256
	char filename_buf[BUF_SIZE];
	int size = JSStringGetUTF8CString(js_filename, filename_buf, BUF_SIZE);
	JSStringRelease(js_filename);

	ostringstream fns;
	if (strcmp(filename_buf, "[LIBRARY]") != 0)
		fns << "managed/" << filename_buf;
	else fns << ".library";

	ifstream in(fns.str().c_str());
	
	string line;
	View *cur = &root;
	root.mParent = NULL;
	root.mName = "root";
	std::getline(in, root.mFilter);
	root.mSubviews.clear();
	root.mItems.clear();

	while (!std::getline(in, line).eof())
	{
		if (line.length() == 0) continue;
		
		if (line == "}")
		{
			if (cur->mParent == NULL) break;
			else cur = cur->mParent;
		}
		else if (line.at(line.length() - 1) == '{')
		{
			const char *cstr = line.c_str();
			const char *brk  = strchr(cstr, ':');

			string filter;
			string name;

			if (brk == NULL)
			{
				filter = "";
				name = string(cstr, line.length() - 1);
			}
			else
			{
				filter = string(cstr, brk - cstr);
				name = string(brk + 1, line.length() - (brk - cstr) - 2);
			}
			
			View *next;
			if (cur->mSubviews.find(name) == cur->mSubviews.end())
			{
				next = new View;
				next->mName = name;
				next->mParent = cur;
				next->mFilter = filter;
				cur->mSubviews[name] = next;
			}
			else next = cur->mSubviews[name];
			cur = next;
		}
		else
		{
			handle_id id = atoi(line.c_str());
			cur->mItems.insert(id);
		}
	}

	ostringstream(out);
	WriteView(out, &root);

	JSStringRef str = JSStringCreateWithUTF8CString(out.str().c_str());
	JSValueRef result = JSValueMakeFromJSONString(context, str);
	JSStringRelease(str);

	if (result == NULL)
	{
		return JSValueMakeNull(context);
	}
	else return result;
}

static JSValueRef
native_close_play_tree(JSContextRef     context,
					   JSObjectRef      function,
					   JSObjectRef      self,
					   size_t           argc,
					   const JSValueRef argv[],
					   JSValueRef*      exception)
{
	FreeView(&root);
	return JSValueMakeNull(context);
}

static JSValueRef
native_get_play_tree_list(JSContextRef     context,
						  JSObjectRef      function,
						  JSObjectRef      self,
						  size_t           argc,
						  const JSValueRef argv[],
						  JSValueRef*      exception)
{
	GDir *dir = g_dir_open("managed", 0, NULL);
	const char *name;
	ostringstream out;
	
	out << "{ \"list\" : [ \"[LIBRARY]\"";
	while ((name = g_dir_read_name(dir)))
	{
		out << ", \"" << name << "\"";
	}
	out << "]}";
	
	JSStringRef str = JSStringCreateWithUTF8CString(out.str().c_str());
	JSValueRef result = JSValueMakeFromJSONString(context, str);
	JSStringRelease(str);

	if (result == NULL)
	{
		g_print("ERROR %s\n", out.str().c_str());
		return JSValueMakeNull(context);
	}
	else return result;
}

struct NativeCallRegEntry
{
	const char *name;	
	JSObjectCallAsFunctionCallback func;
};

const struct NativeCallRegEntry native_calls_list[] = {
	{ "GetPlayTreeList", native_get_play_tree_list },
	{ "OpenPlayTree", native_open_play_tree },
	{ "ResetPlayTree", native_reset_play_tree },
	{ "RemovePlayTree", native_remove_play_tree },
	{ "ClosePlayTree", native_close_play_tree },
	{ "PlayTreeInsert", native_play_tree_insert },
	{ "SendToPlayer", native_send_to_player },
};

/* ======================================== */

#include "wkgui.hpp"

WKGUI gui;

int
main(int argc, char *argv[])
{
	// Initail the BDB connection
	db = new Db(NULL, DB_CXX_NO_EXCEPTIONS);
	db->set_error_stream(&cerr);
	db->open(NULL, ".mpt_lib.db", NULL, DB_BTREE, DB_CREATE, 0);

	if (argc > 1 &&
		(strcmp(argv[1], "--update") == 0 ||
		 strcmp(argv[1], "-u") == 0))
	{
		istream *in = &cin;
		ostream *out = &cout;
		if (argc > 2)
			in = new ifstream(argv[2]);
		
		if (argc > 3)
			out = new ofstream(argv[3]);

		UpdateDb(*in, *out);
		
		if (in != &cin) delete in;
		if (out != &cout) delete out;
	}
	else
	{
		int i;
		int count = sizeof(native_calls_list) / sizeof(native_calls_list[0]);
		for (i = 0; i != count; ++ i)
		{
			gui.RegisterJSCallback(native_calls_list[i].name,
								   native_calls_list[i].func);
		}

		gui.Start(&argc, &argv, START_FILE);
	}
		
	return 0;
}
