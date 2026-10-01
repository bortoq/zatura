/* SPDX-License-Identifier: Zlib */

#ifndef ZATURA_DATABASE_NULL_H
#define ZATURA_DATABASE_NULL_H

#include "database.h"

#define ZATURA_TYPE_NULLDATABASE (zatura_nulldatabase_get_type())
#define ZATURA_NULLDATABASE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATURA_TYPE_NULLDATABASE, ZaturaNullDatabase))
#define ZATURA_IS_NULLDATABASE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATURA_TYPE_NULLDATABASE))
#define ZATURA_NULLDATABASE_CLASS(klass)                                                                              \
  (G_TYPE_CHECK_CLASS_CAST((klass), ZATURA_TYPE_NULLDATABASE, ZaturaNullDatabaseClass))
#define ZATURA_IS_NULLDATABASE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ZATURA_TYPE_NULLDATABASE))
#define ZATURA_NULLDATABASE_GET_CLASS(obj)                                                                            \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATURA_TYPE_NULLDATABASE, ZaturaNullDatabaseClass))

typedef struct _ZaturaNullDatabase ZaturaNullDatabase;
typedef struct _ZaturaNullDatabaseClass ZaturaNullDatabaseClass;

struct _ZaturaNullDatabase {
  GObject parent_instance;
};

struct _ZaturaNullDatabaseClass {
  GObjectClass parent_class;
};

GType zatura_nulldatabase_get_type(void);

/**
 * Initialize database system.
 *
 * @return A valid zatura_database_t instance or NULL on failure
 */
zatura_database_t* zatura_nulldatabase_new(void);

#endif
