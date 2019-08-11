#include <string.h>
#include <target/util.h>
#include <target/hellutil.h>

void* malloc(size_t);

// helper functions
static void decrement_immediate(HellImmediate* imm);
static int compare_hell_immediate(const HellImmediate* imm1, const HellImmediate* imm2);
static void insert_preceeding_rnop(HellBlock* hb);

// process HeLL program
static void compute_referenced_by(HellProgram* hp); // go through data section, resolve references and add to referenced_by list
static void add_rnops_for_u_prefix(HellProgram* hp); // go through code section. if a label is referenced U_-prefixed, add enough RNop instructions before label
static void sort_offsets(HellProgram* hp); // order: begin with fixed offsets (ascending), afterwards put all variable offsets
static void assign_memory_cells(HellProgram* hp); // assign ascending offsets, starting behind greatest fixed offset value
static void convert_to_immediates(HellProgram* hp); // convert references and xlat cycles into fixed HellImmediate values
static void emit_initialization_code(HellProgram* hp);
static void emit_entry_point_code(HellProgram* hp);

// generate Malbolge Unshackled code
void target_mu(Module* module) {
  HellProgram* hp = NULL;
  make_hell_object(module, &hp);
  if (!hp) {
    error("oops");
  }
  compute_referenced_by(hp);
  add_rnops_for_u_prefix(hp);
  sort_offsets(hp);
  assign_memory_cells(hp);
  convert_to_immediates(hp);
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
  // TODO: FIX memory leaks!!
  // old suffix must be freed, but it is not clear
  // whether the old suffix is located in heap
  if (!imm) {
    return;
  }
  int len = strlen(imm->suffix);
  char* new_suffix = (char*)malloc(len+1);
  memcpy(new_suffix+1,imm->suffix,len);
  new_suffix[0] = '0' + imm->praefix_1t;
  int dec = 1;
  len++;
  while (dec && len > 0) {
    len--;
    new_suffix[len]--;
    if (new_suffix[len]<'0') {
      new_suffix[len] = '2';
    }
  }
  while (new_suffix[0] == imm->praefix_1t + '0')
    new_suffix++;
  imm->suffix = new_suffix;
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
      for (LabelList* label = code->labels; label; label = label->next) {
        if (!label->item) {
          continue;
        }
        // look for U_ commands refering here
        for (HellReferencedBy* ref = label->item->referenced_by; ref; ref=ref->next) {
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
}

static int compare_hell_immediate(const HellImmediate* imm1, const HellImmediate* imm2) {
  if (!imm1 && !imm2) {
    return 0;
  }
  if (!imm1) {
    return 1;
  }
  if (!imm2) {
    return -1;
  }
  if (imm1->praefix_1t != imm2->praefix_1t) {
    return imm1->praefix_1t - imm2->praefix_1t;
  }
  const char* s1 = imm1->suffix;
  while (*s1 == '0' + imm1->praefix_1t) {
    s1++;
  }
  const char* s2 = imm2->suffix;
  while (*s2 == '0' + imm2->praefix_1t) {
    s2++;
  }
  int s1len = strlen(imm1->suffix);
  int s2len = strlen(imm2->suffix);
  if (s1len > s2len) {
    return *s1 - '0' - imm2->praefix_1t;
  }else if (s2len > s1len) {
    return '0' + imm1->praefix_1t - *s2;
  }else{
    while (s1 != NULL && s2 != NULL) {
      if (*s1 != *s2) {
        return *s1 - *s2;
      }
      s1++;
      s2++;
    }
    return 0; // equal
  }
}

static void insert_block_sorted(HellProgram* hp, HellBlock* block) {
  for (HellBlock** it = &hp->blocks; *it; it = &(*it)->next) {
    int cmp = compare_hell_immediate((*it)->offset, block->offset);
    if (cmp >= 0) { // TODO: fix 8cc
      block->next = *it;
      *it = block;
      return;
    }
  }
  error("oops");
}

static void sort_offsets(HellProgram* hp) {
  for (HellBlock** it = &hp->blocks; *it; it = &(*it)->next) {
    if ((*it)->next) {
      int cmp = compare_hell_immediate((*it)->offset, (*it)->next->offset);
      if (cmp < 0) { // TODO: fix 8cc
        HellBlock* cut = *it;
        (*it) = (*it)->next;
        insert_block_sorted(hp, cut);
      }
    }
  }
}

static void assign_memory_cells(HellProgram* hp) {
  HellImmediate* last_offset = NULL;
  int last_blocksize = 0;
  for (HellBlock* it = hp->blocks; it; it=it->next) {
    if (it->offset) {
      last_offset = it->offset;
      // TODO: count block size... -> last_blocksize
      continue;
    }
    if (it->code) {
      // insert one Nop at beginning... to prevent xlat2 crashes
      // Any valid Malbolge command is sufficient; RNop is not necessary;
      // however, a function to insert RNop already exists for the U_-prefixes
      insert_preceeding_rnop(it);
    }
    // TODO: assign offset: add last_blocksize to last_offset

    // TODO: update last_offset and last_blocksize

    // prevent not-used warning/error
    if (last_offset && last_blocksize) { }
  }
  // I think it will be no fun to implement this function
  error("not implemented yet");
}

static void convert_to_immediates(HellProgram* hp) {
  // maybe one subprocedure for data (reference -> immediate) and one for code (replace with data)
  error("not implemented yet");
  if (hp) { // avoid not-used warning/error
  }
}

static void emit_initialization_code(HellProgram* hp) {
  // copy weird code from LMFAO here
  error("not implemented yet");
  if (hp) { // avoid not-used warning/error
  }
}

static void emit_entry_point_code(HellProgram* hp) {
  // copy weird code from LMFAO here
  error("not implemented yet");
  if (hp) { // avoid not-used warning/error
  }
}
