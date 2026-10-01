/* SPDX-License-Identifier: Zlib */

#include "database-null.h"

#include <girara/datastructures.h>
#include <girara/input-history.h>

#include "utils.h"

static bool add_bookmark(zatura_database_t* GIRARA_UNUSED(db), const char* GIRARA_UNUSED(file),
                         zatura_bookmark_t* GIRARA_UNUSED(bookmark)) {
  return true;
}

static bool remove_bookmark(zatura_database_t* GIRARA_UNUSED(db), const char* GIRARA_UNUSED(file),
                            const char* GIRARA_UNUSED(id)) {
  return true;
}

static bool load_bookmarks(zatura_database_t* GIRARA_UNUSED(db), const char* GIRARA_UNUSED(file),
                           girara_list_t* GIRARA_UNUSED(target_list)) {
  return true;
}

static girara_list_t* load_list(zatura_database_t* GIRARA_UNUSED(db), const char* GIRARA_UNUSED(file)) {
  return girara_list_new();
}

static bool save_list(zatura_database_t* GIRARA_UNUSED(db), const char* GIRARA_UNUSED(file),
                      girara_list_t* GIRARA_UNUSED(jumplist)) {
  return true;
}

static bool set_fileinfo(zatura_database_t* GIRARA_UNUSED(db), const char* GIRARA_UNUSED(file),
                         const uint8_t* GIRARA_UNUSED(hash), zatura_fileinfo_t* GIRARA_UNUSED(file_info)) {
  return true;
}

static bool get_fileinfo(zatura_database_t* GIRARA_UNUSED(db), const char* GIRARA_UNUSED(file),
                         const uint8_t* GIRARA_UNUSED(hash), zatura_fileinfo_t* GIRARA_UNUSED(file_info)) {
  return false;
}

static bool supports_hash_queries(zatura_database_t* GIRARA_UNUSED(db)) {
  return false;
}

static void io_append(GiraraInputHistoryIO* GIRARA_UNUSED(db), const char* GIRARA_UNUSED(input)) {}

static girara_list_t* io_read(GiraraInputHistoryIO* GIRARA_UNUSED(db)) {
  return girara_list_new_with_free(g_free);
}

static girara_list_t* get_recent_files(zatura_database_t* GIRARA_UNUSED(db), int GIRARA_UNUSED(max),
                                       const char* GIRARA_UNUSED(basepath)) {
  return girara_list_new();
}

static void db_interface_init(ZaturaDatabaseInterface* iface) {
  /* initialize interface */
  iface->add_bookmark          = add_bookmark;
  iface->remove_bookmark       = remove_bookmark;
  iface->load_bookmarks        = load_bookmarks;
  iface->load_jumplist         = load_list;
  iface->save_jumplist         = save_list;
  iface->set_fileinfo          = set_fileinfo;
  iface->get_fileinfo          = get_fileinfo;
  iface->get_recent_files      = get_recent_files;
  iface->load_quickmarks       = load_list;
  iface->save_quickmarks       = save_list;
  iface->supports_hash_queries = supports_hash_queries;
}

static void io_interface_init(GiraraInputHistoryIOInterface* iface) {
  /* initialize interface */
  iface->append = io_append;
  iface->read   = io_read;
}

static void zatura_nulldatabase_class_init(ZaturaNullDatabaseClass* GIRARA_UNUSED(class)) {}

static void zatura_nulldatabase_init(ZaturaNullDatabase* GIRARA_UNUSED(db)) {}

G_DEFINE_TYPE_WITH_CODE(ZaturaNullDatabase, zatura_nulldatabase, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(ZATURA_TYPE_DATABASE, db_interface_init)
                            G_IMPLEMENT_INTERFACE(GIRARA_TYPE_INPUT_HISTORY_IO, io_interface_init))

zatura_database_t* zatura_nulldatabase_new(void) {
  zatura_database_t* db = g_object_new(ZATURA_TYPE_NULLDATABASE, NULL);
  return db;
}
