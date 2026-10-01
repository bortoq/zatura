/* SPDX-License-Identifier: Zlib */

#include "database.h"

G_DEFINE_INTERFACE(ZaturaDatabase, zatura_database, G_TYPE_OBJECT)

static void zatura_database_default_init(ZaturaDatabaseInterface* GIRARA_UNUSED(iface)) {}

bool zatura_db_add_bookmark(zatura_database_t* db, const char* file, zatura_bookmark_t* bookmark) {
  g_return_val_if_fail(ZATURA_IS_DATABASE(db) && file != NULL && bookmark != NULL, false);

  return ZATURA_DATABASE_GET_INTERFACE(db)->add_bookmark(db, file, bookmark);
}

bool zatura_db_remove_bookmark(zatura_database_t* db, const char* file, const char* id) {
  g_return_val_if_fail(ZATURA_IS_DATABASE(db) && file != NULL && id != NULL, false);

  return ZATURA_DATABASE_GET_INTERFACE(db)->remove_bookmark(db, file, id);
}

bool zatura_db_load_bookmarks(zatura_database_t* db, const char* file, girara_list_t* target_list) {
  g_return_val_if_fail(ZATURA_IS_DATABASE(db) && file && target_list, false);

  return ZATURA_DATABASE_GET_INTERFACE(db)->load_bookmarks(db, file, target_list);
}

girara_list_t* zatura_db_load_jumplist(zatura_database_t* db, const char* file) {
  g_return_val_if_fail(ZATURA_IS_DATABASE(db) && file != NULL, NULL);

  return ZATURA_DATABASE_GET_INTERFACE(db)->load_jumplist(db, file);
}

bool zatura_db_save_jumplist(zatura_database_t* db, const char* file, girara_list_t* jumplist) {
  g_return_val_if_fail(ZATURA_IS_DATABASE(db) && file != NULL && jumplist != NULL, NULL);

  return ZATURA_DATABASE_GET_INTERFACE(db)->save_jumplist(db, file, jumplist);
}

bool zatura_db_set_fileinfo(zatura_database_t* db, const char* file, const uint8_t* hash,
                             zatura_fileinfo_t* file_info) {
  g_return_val_if_fail(ZATURA_IS_DATABASE(db) && file != NULL && file_info != NULL, false);

  return ZATURA_DATABASE_GET_INTERFACE(db)->set_fileinfo(db, file, hash, file_info);
}

bool zatura_db_get_fileinfo(zatura_database_t* db, const char* file, const uint8_t* hash,
                             zatura_fileinfo_t* file_info) {
  g_return_val_if_fail(ZATURA_IS_DATABASE(db) && file != NULL && file_info != NULL, false);

  return ZATURA_DATABASE_GET_INTERFACE(db)->get_fileinfo(db, file, hash, file_info);
}

girara_list_t* zatura_db_get_recent_files(zatura_database_t* db, int max, const char* basepath) {
  g_return_val_if_fail(ZATURA_IS_DATABASE(db), NULL);

  return ZATURA_DATABASE_GET_INTERFACE(db)->get_recent_files(db, max, basepath);
}

girara_list_t* zatura_db_load_quickmarks(ZaturaDatabase* db, const char* file) {
  g_return_val_if_fail(ZATURA_IS_DATABASE(db) && file, NULL);

  return ZATURA_DATABASE_GET_INTERFACE(db)->load_quickmarks(db, file);
}

bool zatura_db_save_quickmarks(ZaturaDatabase* db, const char* file, girara_list_t* quickmarks) {
  g_return_val_if_fail(ZATURA_IS_DATABASE(db) && file && quickmarks, false);

  return ZATURA_DATABASE_GET_INTERFACE(db)->save_quickmarks(db, file, quickmarks);
}

bool zatura_db_supports_hash_queries(zatura_database_t* db) {
  g_return_val_if_fail(ZATURA_IS_DATABASE(db), false);

  return ZATURA_DATABASE_GET_INTERFACE(db)->supports_hash_queries(db);
}
