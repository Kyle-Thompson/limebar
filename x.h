#pragma once

#include <xcb/xcb.h>
#include <xcb/xcb_xrm.h>

class X {
 public:
  static X* Instance();

  void get_string_resource(const char* query, char **out) {
    xcb_xrm_resource_get_string(database, query, nullptr, out);
  }
  void change_property(uint8_t mode, xcb_window_t window, xcb_atom_t property,
      xcb_atom_t type, uint8_t format, uint32_t data_len, const void *data) {
    xcb_change_property(connection, mode, window, property, type, format, data_len, data);
  }
  uint32_t generate_id() { return xcb_generate_id(connection); }
  void flush() { xcb_flush(connection); }

  // temporary
  xcb_connection_t *get_connection() { return connection; }
  xcb_xrm_database_t *get_database() { return database; }
  
 private:
  X();
  ~X();

  xcb_connection_t *connection { nullptr };
  xcb_xrm_database_t *database { nullptr };
  static X* instance;
};
