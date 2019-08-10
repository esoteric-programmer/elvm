#include <target/util.h>
#include <target/hellutil.h>

// helper functions
static void decrement_immediate(HellImmediate* imm);
static void insert_preceeding_rnop(HellBlock* hb);

// process HeLL program
static void compute_referenced_by(HellProgram* hp); // go through data section, resolve references and add to referenced_by list
static void add_rnops_for_u_prefix(HellProgram* hp); // go through code section. if a label is referenced U_-prefixed, add enough RNop instructions before label


// generate Malbolge Unshackled code
void target_mu(Module* module) {
  HellProgram* hp = NULL;
  make_hell_object(module, &hp);
  if (!hp) {
    error("oops");
  }
  compute_referenced_by(hp);
  add_rnops_for_u_prefix(hp);
  sort_offsets(hp); // order: begin with fixed offsets (ascending), afterwards put all variable offsets
  fix_variable_offsets(hp); // assign ascending offsets, starting behind greatest fixed offset value
  convert_to_immediates(hp); // convert references and xlat cycles into fixed HellImmediate values
  emit_initialization_code(hp);
  emit_entry_point_code(hp);
  free_hell_program(&hp);
}

static void compute_referenced_by(HellProgram* hp) {
  for (HellBlock* hb = hp->blocks; hb; hb=hb->next) {
    for (HellDataAtom* data = hb->data; data; data=data->next) {
      if (data->reference) {
        LabelTree* tree_item = find_label(hp->labels, data->reference->label);
        if (!tree_item) {
          error("oops");
        }
        HellReferencedBy* ref_item = (HellReferencedBy*)malloc(sizeof(HellReferencedBy));
        if (!ref_item) {
          error("out of mem");
        }
        ref_item->data = data;
        ref_item->next = tree_item->referenced_by;
        tree_item->referenced_by = ref_item;
      }
    }
  }
}

static void decrement_immediate(HellImmediate* imm) {
  // TODO
  error("not implemented yet");
}

static void insert_preceeding_rnop(HellBlock* hb) {
  // insert rnop at beginning of block
  XlatCycle* cyc = (XlatCycle*)malloc(sizeof(XlatCycle));
  if (!cyc) {
    error("out of mem");
  }
  cyc->command = MALBOLGE_COMMAND_NOP;
  cyc->next = (XlatCycle*)malloc(sizeof(XlatCycle));
  if (!cyc->next) {
    error("out of mem");
  }
  cyc->next->command = MALBOLGE_COMMAND_NOP;
  cyc->next->next = NULL;
  HellCodeAtom* nop = (HellCodeAtom*)malloc(sizeof(HellCodeAtom));
  nop->command = cyc;
  nop->labels = NULL;
  nop->next = hb->code;
  hb->code = nop;
  decrement_immediate(hb->offset);
}

static void add_rnops_for_u_prefix(HellProgram* hp) {
  for (HellBlock* hb = hp->blocks; hb; hb=hb->next) {
    int code_items = 0;
    int non_rnop_item_exists_before = 0;
    for (HellCodeAtom* code = hb->code; code; code=code->next) {
      // look for U_ commands refering here
      for (HellReferencedBy* ref = code->referenced_by; ref; ref=ref->next) {
        int ref_offset = ref->data->reference->offset;
        // if (ref_offset < 0) -- hack for 8cc
        if ((unsigned int)ref_offset >= ((unsigned int)-1)/2) {
          if (non_rnop_item_exists_before) {
            error("oops");
          }
          int insert_rnops = -(code_items + ref_offset);
          for (int i = 0; i < insert_rnops; i++) {
            insert_preceeding_rnop(hb);
            code_items++;
          }
        }
      }
      // update non_rnop_item_exists_before and code_items
      int cnt_cmd = 0;
      for (XlatCycle* command = code->command; command; command=command->next) {
        if (command->command != MALBOLGE_COMMAND_NOP) {
          non_rnop_item_exists_before = 1;
        }
        cnt_cmd++;
      }
      if (cnt_cmd < 2) {
        non_rnop_item_exists_before = 1;
      }
      code_items++;
    }
  }
}
