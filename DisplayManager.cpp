#include "DisplayManager.h"

DisplayManager* DisplayManager::instance = nullptr;

DisplayManager::DisplayManager() {
  display = XOpenDisplay(nullptr);
  if (!display) {
    fprintf (stderr, "Couldnt open display\n");
    exit(EXIT_FAILURE);
  }
}
DisplayManager::~DisplayManager() {
  delete instance;
}

DisplayManager* DisplayManager::Instance() {
  if (!instance) {
    instance = new DisplayManager();
  }
  return instance;
}


// TODO: return string
char* DisplayManager::get_property(Window win, Atom xa_prop_type,
    const char *prop_name, unsigned long *size) {
  Atom xa_prop_name;
  Atom xa_ret_type;
  int ret_format;
  unsigned long ret_nitems;
  unsigned long ret_bytes_after;
  unsigned long tmp_size;
  unsigned char *ret_prop;
  char *ret;

  xa_prop_name = XInternAtom(display, prop_name, False);

  if (XGetWindowProperty(display, win, xa_prop_name, 0, 1024, False,
      xa_prop_type, &xa_ret_type, &ret_format,
      &ret_nitems, &ret_bytes_after, &ret_prop) != Success) {
    return NULL;
  }

  if (xa_ret_type != xa_prop_type) {
    XFree(ret_prop);
    return NULL;
  }

  /* null terminate the result to make string handling easier */
  tmp_size = (ret_format / (32 / sizeof(long))) * ret_nitems;
  ret = (char *) malloc(tmp_size + 1);
  memcpy(ret, ret_prop, tmp_size);
  ret[tmp_size] = '\0';

  if (size) {
    *size = tmp_size;
  }

  XFree(ret_prop);
  return ret;
}


// TODO: return string
char* DisplayManager::get_window_title(Window win) {
  char *title_utf8 = nullptr;
  char *wm_name = get_property(win, XA_STRING, "WM_NAME", NULL);
  char *net_wm_name = get_property(win,
      XInternAtom(display, "UTF8_STRING", False), "_NET_WM_NAME", NULL);

  if (net_wm_name) {
    title_utf8 = strdup(net_wm_name);
  }

  free(wm_name);
  free(net_wm_name);

  return title_utf8;
}


Window* DisplayManager::get_client_list(unsigned long *size) {
  Window *client_list;

  if ((client_list = (Window *)get_property(DefaultRootWindow(display),
      XA_WINDOW, "_NET_CLIENT_LIST", size)) == NULL) {
    if ((client_list = (Window *)get_property(DefaultRootWindow(display),
        XA_CARDINAL, "_WIN_CLIENT_LIST", size)) == NULL) {
      fprintf(stderr, "Cannot get client list properties. \n"
          "(_NET_CLIENT_LIST or _WIN_CLIENT_LIST)\n");
      return NULL;
    }
  }

  return client_list;
}


unsigned long DisplayManager::get_current_workspace() {
  unsigned long *cur_desktop = NULL;
  Window root = DefaultRootWindow(display);
  if (! (cur_desktop = (unsigned long *)get_property(root,
      XA_CARDINAL, "_NET_CURRENT_DESKTOP", NULL))) {
    if (! (cur_desktop = (unsigned long *)get_property(root,
        XA_CARDINAL, "_WIN_WORKSPACE", NULL))) {
      fprintf(stderr, "Cannot get current desktop properties. "
          "(_NET_CURRENT_DESKTOP or _WIN_WORKSPACE property)\n");
      free(cur_desktop);
      exit(EXIT_FAILURE);
    }
  }
  unsigned long ret = *cur_desktop;
  free(cur_desktop);
  return ret;
}
