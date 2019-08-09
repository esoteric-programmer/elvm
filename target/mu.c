#include <target/util.h>
#include <target/hellutil.h>

void target_mu(Module* module) {
  HellProgram* hp = NULL;
  make_hell_object(module, &hp);
  if (!hp) {
    error("oops");
  }
  compute_referenced_by(hp); // go through data section, resolve references and add to referenced_by list
  add_rnops_for_u_prefix(hp); // go through code section. if a label is referenced U_-prefixed, add enough RNop instructions before label
  sort_offsets(hp); // order: begin with fixed offsets (ascending), afterwards put all variable offsets
  fix_variable_offsets(hp); // assign ascending offsets, starting behind greatest fixed offset value
  convert_to_immediates(hp); // convert references and xlat cycles into fixed HellImmediate values
  emit_initialization_code(hp);
  emit_entry_point_code(hp);
  free_hell_program(&hp);
}
