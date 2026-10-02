/* SPDX-License-Identifier: Zlib */
#include <girara-gtk/internal.h>
#include <glib/gstdio.h>
#include <girara-gtk/settings.h>
#include "zatura/zatura.h"
int main(int argc, char** argv) {
  if (argc != 2 && argc != 3) { return 1; }
  gtk_init();
  zathura_t* app = zathura_create();
  char* dir = g_dir_make_tmp("zatura-default-config-XXXXXX", NULL);
  zathura_set_config_dir(app, argc == 3 ? argv[2] : dir);
  if (!zathura_init(app)) { return 1; }
  girara_list_t* args = girara_list_new();
  girara_list_append(args, argv[1]);
  const bool ok = girara_cmd_dump_config(app->ui.session, args);
  g_print("Settings: %zu; keyboard: %zu; mouse: %zu; inputbar: %zu\n",
      girara_list_size(app->ui.session->private_data->settings),
      girara_list_size(app->ui.session->bindings.shortcuts),
      girara_list_size(app->ui.session->bindings.mouse_events),
      girara_list_size(app->ui.session->bindings.inputbar_shortcuts));
  girara_list_free(args);
  zathura_free(app);
  g_rmdir(dir);
  g_free(dir);
  return ok ? 0 : 1;
}
