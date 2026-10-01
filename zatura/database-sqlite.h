/* SPDX-License-Identifier: Zlib */

#ifndef ZATURA_DATABASE_SQLITE_H
#define ZATURA_DATABASE_SQLITE_H

#include "database.h"

#define ZATURA_TYPE_SQLDATABASE (zatura_sqldatabase_get_type())
#define ZATURA_SQLDATABASE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATURA_TYPE_SQLDATABASE, ZaturaSQLDatabase))
#define ZATURA_IS_SQLDATABASE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATURA_TYPE_SQLDATABASE))
#define ZATURA_SQLDATABASE_CLASS(klass)                                                                               \
  (G_TYPE_CHECK_CLASS_CAST((klass), ZATURA_TYPE_SQLDATABASE, ZaturaSQLDatabaseClass))
#define ZATURA_IS_SQLDATABASE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ZATURA_TYPE_SQLDATABASE))
#define ZATURA_SQLDATABASE_GET_CLASS(obj)                                                                             \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATURA_TYPE_SQLDATABASE, ZaturaSQLDatabaseClass))

typedef struct _ZaturaSQLDatabase ZaturaSQLDatabase;
typedef struct _ZaturaSQLDatabaseClass ZaturaSQLDatabaseClass;

struct _ZaturaSQLDatabase {
  GObject parent_instance;
};

struct _ZaturaSQLDatabaseClass {
  GObjectClass parent_class;
};

GType zatura_sqldatabase_get_type(void);

/**
 * Initialize database system.
 *
 * @param path Path to the sqlite database.
 * @return A valid zatura_database_t instance or NULL on failure
 */
zatura_database_t* zatura_sqldatabase_new(const char* path);

#endif
