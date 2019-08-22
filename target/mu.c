#include <string.h>
#include <target/util.h>
#include <target/hellutil.h>

void* malloc(size_t);
void free(void*);

static const char* xlat2 = "5z]&gqtyfr$(we4{WP)H-Zn,[%\\3dL+Q;>U!pJS72FhOA1C" \
		"B6v^=I_0/8|jsb9m<.TVac`uY*MK'X~xDl}REokN:#?G\"i@";

// helper functions
static void decrement_immediate(HellImmediate* imm);
static int compare_hell_immediate(const HellImmediate* imm1, const HellImmediate* imm2);
static void insert_preceeding_rnop(HellBlock* hb);
static void insert_block_sorted(HellProgram* hp, HellBlock* block);
static HellImmediate* compute_offset(HellImmediate* base, int offset);
static int immediate_mod_94(HellImmediate* imm);
static char code_value(XlatCycle* cyc, int offset);
static int compute_additional_code_offset(HellCodeAtom* code, HellImmediate* base_offset, int offset);
static void replace_code_by_immediates(HellBlock* hb);
static void resolve_reference(LabelTree* tree, HellDataAtom* data);
static void print_offset_and_label(HellImmediate* offset);
static int is_entrypoint(LabelList* labels);

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
  if (!module) {
    error("oops");
  }
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
  if (!hb) {
    error("oops");
  }
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
  nop->pos.father = NULL;
  nop->pos.position = 0;
  nop->labels = NULL;
  nop->next = hb->code;
  hb->code = nop;
  decrement_immediate(hb->offset);
}

static void add_rnops_for_u_prefix(HellProgram* hp) {
  if (!hp) {
    error("oops");
  }
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
          if (ref_offset < 0) {
          // if ((unsigned int)ref_offset >= ((unsigned int)-1)/2) { // -- hack for 8cc
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
  int s1len = strlen(s1);
  int s2len = strlen(s2);
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
  HellBlock* it = hp->blocks;
  if (!it) {
    return;
  }
  while (it->next) {
    int cmp = compare_hell_immediate(it->offset, it->next->offset);
    if (cmp > 0) { // TODO: fix 8cc
      HellBlock* cut = it->next;
      it->next = it->next->next;
      insert_block_sorted(hp, cut);
      continue;
    }
    it = it->next;
  }
}

static HellImmediate* compute_offset(HellImmediate* base, int offset) {
  if (!base) {
    error("oops");
  }
  if (!base->suffix) {
    error("oops");
  }
  HellImmediate* ret = (HellImmediate*)malloc(sizeof(HellImmediate));
  if (!ret) {
    error("out of mem");
  }
  ret->praefix_1t = base->praefix_1t;
  int lolen = strlen(base->suffix);
  int bs_ter_len = 0;
  int sign = 1;
  if (offset < 0) { // TODO: fix 8cc
    sign = -1;
  }
  for (int tmp = offset*sign; tmp; tmp=tmp/3) {
    bs_ter_len++;
  }
  int nolen = lolen+bs_ter_len+2; // upper bound for number of new digits
  char* new_suffix = (char*)malloc(nolen); // TODO: memory leak: this string must be freed somewhere!
  ret->suffix = new_suffix;
  memset(new_suffix,'0'+ret->praefix_1t,nolen-1);
  memcpy(new_suffix+nolen-1-lolen,base->suffix,lolen+1);
  // increment by offset
  int pos = nolen-2;
  while (offset) {
    if (sign == 1) {
      int add = offset % 3;
      offset /= 3;
      add += new_suffix[pos]-'0';
      new_suffix[pos] = '0'+add%3;
      offset += add/3;
    }else{
      int sub = (-offset)%3;
      offset /= 3;
      int old_val = new_suffix[pos]-'0';
      if (old_val < sub) {
        offset--;
        old_val += 3;
      }
      new_suffix[pos] = '0'+old_val-sub;
    }
    if (!pos && offset) {
      error("oops"); // overflow
    }
    pos--;
  }
  while (*ret->suffix == '0'+ret->praefix_1t) {
    ret->suffix++;
  }
  return ret;
}

static int immediate_mod_94(HellImmediate* imm) {
  if (!imm) {
    error("oops");
  }
  int ret = 8*imm->praefix_1t;
  int len = strlen(imm->suffix);
  int pos_value = 1;
  for (int i=1;i<=len;i++) {
    ret += (imm->suffix[len-i]-'0' + 94-imm->praefix_1t) * pos_value;
    ret %= 94;
    pos_value *= 3;
    pos_value %= 94;
  }
  return ret;
}

static char code_value(XlatCycle* cyc, int offset) {
  if (!cyc) {
    return 0;
  }
  for (char code = 33; code < 127; code++) {
    // test whether this code matches the XlatCycle *cyc
    if (!cyc->next) {
      // handle non-resistant command
      unsigned char normalized = (unsigned char)((code+offset)%94);
      if (normalized != cyc->command && (cyc->command != MALBOLGE_COMMAND_NOP || is_malbolge_cmd(normalized))) {
        continue;
      }
      return code;
    }
    // handle xlat cycle
    int match = 1;
    char cur_code = code;
    for (XlatCycle* cur_cyc = cyc; cur_cyc || cur_code != code; cur_cyc = cur_cyc->next) {
      if (!cur_cyc) {
        cur_cyc = cyc;
      }
      // test match!
      unsigned char normalized = (unsigned char)((cur_code+offset)%94);
      if (normalized != cur_cyc->command && (cur_cyc->command != MALBOLGE_COMMAND_NOP || is_malbolge_cmd(normalized))) {
        match = 0;
        break;
      }
      cur_code = xlat2[cur_code-33];
    }
    if (match) {
      return code;
    }
  }
  return 0; // error: no matching code value found
}

static int compute_additional_code_offset(HellCodeAtom* code, HellImmediate* base_offset, int offset) {
  int start_offset = (immediate_mod_94(base_offset) + offset)%94;
  for (int i=0; i<94; i++) {
    int current_offset = (start_offset+i) % 94;
    int match = 1;
    for (HellCodeAtom* it = code; it; it = it->next) {
      if (!code_value(it->command, current_offset)) {
        match = 0;
        break;
      }
      current_offset = (current_offset+1)%94;
    }
    if (match) {
      return i;
    }
  }
  return -1;
}

static void assign_memory_cells(HellProgram* hp) {
  HellImmediate* last_offset = NULL;
  int last_blocksize = 0;
  for (HellBlock* it = hp->blocks; it; it=it->next) {
    // compute current block's size
    int cnt = 0;
    if (it->code && !it->data) {
      // insert one Nop at beginning... to prevent xlat2 crashes
      // Any valid Malbolge command is sufficient; RNop is not necessary;
      // however, a function to insert RNop already exists for the U_-prefixes
      insert_preceeding_rnop(it);
      for (HellCodeAtom* cit = it->code; cit; cit = cit->next) {
        cit->pos.father = it;
        cit->pos.position = cnt;
        cnt++;
      }
    }else if (it->data && !it->code) {
      for (HellDataAtom* dit = it->data; dit; dit = dit->next){
        dit->pos.father = it;
        dit->pos.position = cnt;
        cnt++;
      }
    }else{
      error("oops");
    }
    if (it->offset) {
      last_offset = it->offset;
      last_blocksize = cnt;
      continue;
    }
    int additional_offset = 0;
    if (it->code) {
      // move the code block according to xlat2 position constraints
      additional_offset = compute_additional_code_offset(it->code,last_offset,last_blocksize);
      if (additional_offset == -1) {
        error("oops");
      }
    }
    // assign offset: add last_blocksize to last_offset
    it->offset = compute_offset(last_offset, last_blocksize + additional_offset);

    // update last_offset and last_blocksize
    last_blocksize = cnt;
    last_offset = it->offset;
  }
}


static void replace_code_by_immediates(HellBlock* it) {
  HellDataAtom* last_data = NULL;
  HellCodeAtom* last_code = NULL;
  int base_offset = immediate_mod_94(it->offset);
  int i=0;
  for (HellCodeAtom* code = it->code; code; code=code->next) {
    char imm = code_value(code->command, (base_offset+i)%94);
    if (!imm) {
      error("oops");
    }
    // generate HellImmediate...
    char* ternary = (char*)malloc(6);
    if (!ternary) {
      error("out of mem");
    }
    ternary[5] = 0;
    for (int j=0; j<5; j++) {
      ternary[4-j] = imm%3 + '0';
      imm/=3;
    }
    if (imm) {
      error("oops");
    }
    HellImmediate* hi = (HellImmediate*)malloc(sizeof(HellImmediate));
    if (!hi) {
      error("out of mem");
    }
    hi->praefix_1t = 0;
    hi->suffix = ternary;
    HellDataAtom* hd = (HellDataAtom*)malloc(sizeof(HellDataAtom));
    if (!hd) {
      error("our of mem");
    }
    hd->value = hi;
    hd->reference = NULL;
    hd->labels = code->labels;
    hd->pos.father = it;
    hd->pos.position = i;
    hd->next = NULL;
    if (last_code) {
      free(last_code); // TODO: memory leak!! free XlatCycle-list before
    }
    last_code = code;
    if (last_data) {
      last_data->next = hd;
    }else{
      it->data = hd;
      it->code = NULL;
    }
    last_data = hd;

    i++;
  }
  if (last_code) {
    free(last_code); // TODO: memory leak!! free XlatCycle-list before
  }
}

static void resolve_reference(LabelTree* tree, HellDataAtom* data) {
  if (!data || !tree) {
    error("oops");
  }
  if (!data->reference || data->value) {
    error("oops");
  }
  LabelTree* ref = find_label(tree, data->reference->label);
  if (!ref) {
    error("oops");
  }
  HellImmediate* base_offset = NULL;
  int additional_offset = 0;
  if (ref->code && !ref->data) {
    base_offset = ref->code->pos.father->offset;
    additional_offset = ref->code->pos.position;
  }else if (ref->data && !ref->code) {
    base_offset = ref->data->pos.father->offset;
    additional_offset = ref->data->pos.position;
  }else{
    error("oops");
  }
  additional_offset += data->reference->offset;
  data->value = compute_offset(base_offset, additional_offset-1);
  free(data->reference);
  data->reference = NULL;
}

static void convert_to_immediates(HellProgram* hp) {
  // maybe one subprocedure for data (reference -> immediate) and one for code (replace with data)
  if (!hp) {
    error("oops");
  }
  for (HellBlock* it = hp->blocks; it; it=it->next) {
    if (it->code && !it->data) {
      // HINT: this invalidates referenced_by pointer of LabelTree,
      //       but they are no longer needed U_-prefix has been processed already.
      //       so we do not fix this issue here!
      replace_code_by_immediates(it);
    }else if (it->data && !it->code) {
      for (HellDataAtom* data = it->data; data; data=data->next) {
        if (data->reference && !data->value) {
          resolve_reference(hp->labels, data);
        }
      }
    }else{
      error("oops");
    }
  }
}

static void print_offset_and_label(HellImmediate* offset) {
  if (!offset) {
    error("oops");
  }
  if (!offset->suffix) {
    error("oops");
  }
  printf("@%ct",offset->praefix_1t+'0');
  if (offset->suffix[0]) {
    printf("%s",offset->suffix);
  }else{
    printf("%c",offset->praefix_1t+'0');
  }
  printf("\n");
  printf("l%ct",offset->praefix_1t+'0');
  if (offset->suffix[0]) {
    printf("%s",offset->suffix);
  }else{
    printf("%c",offset->praefix_1t+'0');
  }
  printf(":\n");
}

static int is_entrypoint(LabelList* labels) {
  LabelList* it = labels;
  while (it) {
    if (it->item) {
      if (it->item->label) {
        if (strcmp(it->item->label,"ENTRY")==0) {
          return 1;
        }
      }
    }
    it = it->next;
  }
  return 0;
}

static void emit_initialization_code(HellProgram* hp) {
  if (!hp) {
    error("oops");
  }
  printf(".DATA\n");
  for (HellBlock* hb = hp->blocks; hb; hb=hb->next) {
    if (hb->code) {
      error("oops");
    }
    if (!hb->data) {
      error("oops");
    }
    print_offset_and_label(hb->offset);
    for (HellDataAtom* data = hb->data; data; data=data->next) {
      if (is_entrypoint(data->labels)) {
        printf("ENTRY:\n");
      }
      if (!data->value) {
        printf("  0t0\n"); // standard value for unassiged cell
      }else if (data->value->suffix[0]) {
        printf("  %ct%s\n", data->value->praefix_1t+'0',data->value->suffix);
      }else{
        printf("  %ct%c\n", data->value->praefix_1t+'0',data->value->praefix_1t+'0');
      }
    }
    printf("\n");
  }
  // TODO: copy weird code from LMFAO here; dont print out hell program, but malbolge program
}

static void emit_entry_point_code(HellProgram* hp) {
  // copy weird code from LMFAO here
  // error("not implemented yet");
  if (hp) { // avoid not-used warning/error
  }
}
