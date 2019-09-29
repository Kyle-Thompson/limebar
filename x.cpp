#include "x.h"

#include "DisplayManager.h"

X* X::instance = nullptr;

X::X() {
  if ((connection = DisplayManager::Instance()->get_xcb_connection()) == nullptr) {
    fprintf (stderr, "Couldnt connect to X\n");
    exit (EXIT_FAILURE);
  }

  DisplayManager::Instance()->set_event_queue_order(XCBOwnsEventQueue);

  if (xcb_connection_has_error(connection)) {
    fprintf(stderr, "X connection has error.\n");
    exit(EXIT_FAILURE);
  }

  if (!(database = xcb_xrm_database_from_default(connection))) {
    fprintf(stderr, "Could not connect to database\n");
    exit(EXIT_FAILURE);
  }
}

X::~X() {
  if (connection) {
    xcb_disconnect(connection);
  }
  if (database) {
    xcb_xrm_database_free(database);
  }
}

X* X::Instance() {
  if (!instance) {
    instance = new X();
  }
  return instance;
}
