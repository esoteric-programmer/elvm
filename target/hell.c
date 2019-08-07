#include <target/hellutil.h>

void target_hell(Module* module) {
  HellProgram* hp = NULL;
  make_hell_object(module, &hp);
  // TODO: do something with generated hp
  free_hell_program(&hp);
}
