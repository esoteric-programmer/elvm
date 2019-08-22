#include <stdarg.h>
#include <string.h>
#include <target/util.h>
#include <target/hellutil.h>

void* malloc(size_t);
void free(void*);

// Helper functions for HeLL program generation

static StringList* strlist;
static StringList* strlist_tail;

static HellProgram* current_program;
static HellBlock* current_block;
static HellCodeAtom* current_code;
static HellDataAtom* current_data;
static HellImmediate* fixed_offset;
static LabelList* current_labels;
static LabelList* current_labels_tail;

static void emit_code_atom(XlatCycle* cyc);
static void emit_data_atom(HellImmediate* hi, HellReference* hr);
static void emit_hell_block(HellCodeAtom* code, HellDataAtom* data);
static void free_labeltree(LabelTree* tree);
static void free_xlat(XlatCycle* cyc);
static void	free_code(HellCodeAtom* code);
static void	free_data(HellDataAtom* data);
static void clear_string_memory();


// call the following functions to generate HeLL program

static void emit_xlat_cycle(unsigned char command, ...); // terminated by invalid command, e.g. 0
static void emit_immediate(int praefix_1t, const char* suffix);
static void emit_label_reference(const char* label, int offset);
static void emit_unused_cell();
static void emit_finalize_block();
static void emit_offset(int praefix_1t, const char* suffix); // assign fixed offset to next code/data atom
static void emit_label(const char* name);
static char* make_string(const char* format, ...);

int is_malbolge_cmd(unsigned char cmd) {
  switch (cmd) {
    case MALBOLGE_COMMAND_OPR:
    case MALBOLGE_COMMAND_ROT:
    case MALBOLGE_COMMAND_MOVD:
    case MALBOLGE_COMMAND_JMP:
    case MALBOLGE_COMMAND_IN:
    case MALBOLGE_COMMAND_OUT:
    case MALBOLGE_COMMAND_HALT:
    case MALBOLGE_COMMAND_NOP:
      return 1;
    default:
      return 0;
  }
}

static const char* xlat1 = "+b(29e*j1VMEKLyC})8&m#~W>qxdRp0wkrUo[D7,XT"
		"cA\"lI.v%{gJh4G\\-=O@5`_3i<?Z';FNQuY]szf$!BS/|t:Pn6^Ha";

/*
static unsigned char normalize(unsigned char cmd) {
  return xlat1[(61+(int)cmd)%94];
}
*/

static unsigned char denormalize(unsigned char cmd) {
  if (cmd >= 33 && cmd < 127) {
    unsigned char j;
    for (j=0;j<94;j++) {
      if (cmd == xlat1[j]) {
        return (j+33)%94;
      }
    }
  }
  return 0;
}

static void emit_xlat_cycle(unsigned char command, ...) {
    va_list ap;
    unsigned char c = denormalize(command);
    if (!is_malbolge_cmd(c)) {
      error("oops",command, command, c);
    }
    XlatCycle* cyc = (XlatCycle*)malloc(sizeof(XlatCycle));
    if (!cyc) {
      error("out of mem");
    }
  cyc->command = c;
  cyc->next = NULL;
  XlatCycle* it = cyc;

    va_start(ap, command);
    while (1) {
      int cmd = va_arg(ap, int);
      if (cmd < 0 || cmd > 0xFF) {
        break;
      }
      unsigned char cmd_u8 = (unsigned char)cmd;
      c = denormalize(cmd_u8);
      if (!is_malbolge_cmd(c)) {
        break;
      }
      it->next = (XlatCycle*)malloc(sizeof(XlatCycle));
      if (!it->next) {
        error("out of mem");
      }
      it = it->next;
        it->command = c;
        it->next = NULL;
    }
    va_end(ap);
    emit_code_atom(cyc);
}

static void emit_code_atom(XlatCycle* cyc) {
  if (!cyc) {
    error("oops");
  }
  HellCodeAtom* atom = (HellCodeAtom*)malloc(sizeof(HellCodeAtom));
  if (!atom) {
    error("out of mem");
  }
  atom->command = cyc;
  atom->pos.father = NULL;
  atom->pos.position = 0;
  atom->next = NULL;
  LabelList* it = current_labels;
  while (it) {
    it->item->code = atom;
    it = it->next;
  }
  atom->labels = current_labels;
  current_labels = NULL;
  current_labels_tail = NULL;
  if (current_code) {
    if (fixed_offset) {
      error("oops");
    }
    current_code->next = atom;
    current_code = atom;
  }else{
    emit_hell_block(atom, NULL);
  }
}

static void emit_immediate(int praefix_1t, const char* suffix) {
  if (!suffix) {
    error("oops");
  }
  HellImmediate* hi = (HellImmediate*)malloc(sizeof(HellImmediate));
  if (!hi) {
    error("out of mem");
  }
  hi->praefix_1t = praefix_1t;
  hi->suffix = suffix;
  emit_data_atom(hi, NULL);
}


static void emit_label_reference(const char* label, int offset) {
  if (!label) {
    error("oops");
  }
  HellReference* hr = (HellReference*)malloc(sizeof(HellReference));
  if (!hr) {
    error("out of mem");
  }
  hr->label = label;
  hr->offset = offset;
  emit_data_atom(NULL, hr);
}

static void emit_unused_cell() {
  emit_data_atom(NULL, NULL);
}

static void emit_data_atom(HellImmediate* hi, HellReference* hr) {
  if (hi && hr) {
    error("oops");
  }
  HellDataAtom* atom = (HellDataAtom*)malloc(sizeof(HellDataAtom));
  if (!atom) {
    error("out of mem");
  }
  atom->value = hi;
  atom->reference = hr;
  atom->pos.father = NULL;
  atom->pos.position = 0;
  atom->next = NULL;
  LabelList* it = current_labels;
  while (it) {
    it->item->data = atom;
    it = it->next;
  }
  atom->labels = current_labels;
  current_labels = NULL;
  current_labels_tail = NULL;
  if (current_data) {
    if (fixed_offset) {
      error("oops");
    }
    current_data->next = atom;
    current_data = atom;
  }else{
    emit_hell_block(NULL, atom);
  }
}

static void emit_finalize_block() {
  if (current_labels) {
    // bad HeLL code!! however, assign some dummy data
    emit_unused_cell();
  }
  current_code = NULL;
  current_data = NULL;
  if (fixed_offset) {
    error("oops");
  }
}

static void emit_offset(int praefix_1t, const char* suffix) {
  if (!suffix) {
    error("oops");
  }
  emit_finalize_block();
  HellImmediate* hi = (HellImmediate*)malloc(sizeof(HellImmediate));
  if (!hi) {
    error("out of mem");
  }
  hi->praefix_1t = praefix_1t;
  hi->suffix = suffix;
  fixed_offset = hi;
}

static void emit_hell_block(HellCodeAtom* code, HellDataAtom* data) {
  HellBlock* hb = (HellBlock*)malloc(sizeof(HellBlock));
  if (!hb) {
    error("out of mem");
  }
  if ((code == NULL && data == NULL) || (code != NULL && data != NULL)) {
    error("oops");
  }
  hb->offset = fixed_offset;
  fixed_offset = NULL;
  hb->next = NULL;
  if (code && !data) {
    hb->code = code;
    hb->data = NULL;
    current_code = code;
    current_data = NULL;
  }else if (data && !code) {
    hb->code = NULL;
    hb->data = data;
    current_code = NULL;
    current_data = data;
  }
  if (current_block) {
    current_block->next = hb;
    current_block = hb;
  }else{
    if (!current_program) {
      current_program = (HellProgram*)malloc(sizeof(HellProgram));
      if (!current_program) {
        error("out of mem");
      }
      current_program->labels = NULL;
      current_program->blocks = NULL;
    }
    if (current_program->blocks) {
      error("oops");
    }
    current_program->blocks = hb;
    current_block = hb;
  }
}

// insert label into current_labels list and LabelTree of of current_labels.
static void emit_label(const char* name) {
  if (!name) {
    error("oops");
  }
  LabelTree* label = (LabelTree*)malloc(sizeof(LabelTree));
  if (!label) {
    error("out of mem");
  }
  label->code = NULL;
  label->data = NULL;
  label->label = name;
  label->referenced_by = NULL;
  label->left = NULL;
  label->right = NULL;
  if (!current_program) {
    current_program = (HellProgram*)malloc(sizeof(HellProgram));
    if (!current_program) {
      error("out of mem");
    }
    current_program->labels = NULL;
    current_program->blocks = NULL;
  }
  if (!current_program->labels) {
    current_program->labels = label;
  }else{
    LabelTree* it = current_program->labels;
    while(1) {
      if (!it->label) {
        error("oops");
      }
      int cmp = strncmp(name,it->label,101);
      if (cmp > 0) {
      // if (cmp > 0 && (unsigned int)cmp < ((unsigned int)-1)/2) { // -- hack for 8cc
        if (it->left) {
          it = it->left;
        }else{
          it->left = label;
          break;
        }
      }else if (cmp < 0) {
      // }else if ((unsigned int)cmp >= ((unsigned int)-1)/2){ // -- hack for 8cc
        if (it->right) {
          it = it->right;
        }else{
          it->right = label;
          break;
        }
      }else{
        error("oops");
      }
    }
  }
  LabelList* list = (LabelList*)malloc(sizeof(LabelList));
  if (!list) {
    error("out of mem");
  }
  list->item = label;
  list->next = NULL;
  if (current_labels_tail) {
    if (!current_labels) {
      error("oops");
    }
    current_labels_tail->next = list;
    current_labels_tail = list;
  }else{
    if (current_labels) {
      error("oops");
    }
    current_labels = list;
    current_labels_tail = list;
  }
}

static void free_labeltree(LabelTree* tree) {
  if (!tree) {
    return;
  }
  free_labeltree(tree->left);
  free_labeltree(tree->right);
  free(tree);
}

static void free_xlat(XlatCycle* cyc) {
  XlatCycle* it = cyc;
  while (it) {
    XlatCycle* tmp = it;
    it = it->next;
    // clear tmp
    free(tmp);
  }
}

static void	free_code(HellCodeAtom* code) {
  HellCodeAtom* it = code;
  while (it) {
    HellCodeAtom* tmp = it;
    it = it->next;
    // clear tmp
    free_xlat(tmp->command);
    free(tmp);
  }
}

static void	free_data(HellDataAtom* data) {
  HellDataAtom* it = data;
  while (it) {
    HellDataAtom* tmp = it;
    it = it->next;
    // clear tmp
    if (tmp->value) {
      free(tmp->value);
    }
    if (tmp->reference) {
      free(tmp->reference);
    }
    free(tmp);
  }
}

static char* make_string(const char* format, ...) {
  if (!format || !format[0]) {
    error("oops");
  }
  char* buf = (char*)malloc(100);
  if (!buf) {
    error("out of mem");
  }
  StringList* list = (StringList*)malloc(sizeof(StringList));
  if (!list) {
    free(buf);
    error("out of mem");
  }
  list->next = NULL;
  list->str = buf;
  if (strlist_tail) {
    strlist_tail->next = list;
  }
  strlist_tail = list;
  va_list ap;
  va_start(ap, format);
  if (vsprintf(buf, format, ap)<0) {
    free(buf);
    error("oops");
  }
  va_end(ap);
  return buf;
}

// Generation of HeLL program from Module

static int current_pc_value = -1;

static void init_state_hell(Data* data);
static void hell_emit_inst(Inst* inst); // produce HeLL code for a given instruction
static void finalize_hell();
static void hell_emit_gen_1222();

void make_hell_object(Module* module, HellProgram** hell) {
  if (!hell) {
    error("oops");
  }
  current_program = NULL;
  current_block = NULL;
  current_code = NULL;
  current_data = NULL;
  fixed_offset = NULL;
  current_labels = NULL;
  current_labels_tail = NULL;
  strlist = NULL;
  strlist_tail = NULL;

  current_pc_value = -1;
  init_state_hell(module->data);

  emit_label("ENTRY");
  hell_emit_gen_1222();

  Inst* inst = module->text;
  for (; inst; inst = inst->next) {
    if (current_pc_value != inst->pc) {
      current_pc_value = inst->pc;
      emit_label(make_string("prepare_label_pc%u",current_pc_value)); // to fix issue with unused code
      emit_label_reference("MOVD",0);
      emit_label_reference(make_string("direct_jmp_label_pc%u",current_pc_value),0);
      emit_label(make_string("label_pc%u",current_pc_value));
      emit_label_reference("MOVDMOVD",+1);
      emit_unused_cell();
      emit_unused_cell();
      emit_unused_cell();
      emit_unused_cell();
      emit_label(make_string("direct_jmp_label_pc%u",current_pc_value));
      emit_label_reference("MOVD",+1);
    }
    hell_emit_inst(inst);
  }
  emit_label("end"); // fix LMFAO problem
  emit_label_reference("HALT",0);
  emit_finalize_block();

  finalize_hell();

  *hell = current_program;
  if (!*hell) {
    error("oops");
  }
  (*hell)->string_memory = strlist;
}

LabelTree* find_label(LabelTree* tree, const char* name) {
  if (!name) {
    return NULL;
  }
  if (!name[0]) {
    return NULL;
  }
  if (!tree) {
    return NULL;
  }
  int cmp = strncmp(name,tree->label,101);
  if (cmp > 0) {
  // if (cmp > 0 && (unsigned int)cmp < ((unsigned int)-1)/2){ // -- hack for 8cc
    return find_label(tree->left, name);
  }else if (cmp < 0) {
  // }else if ((unsigned int)cmp >= ((unsigned int)-1)/2){ // -- hack for 8cc
    return find_label(tree->right, name);
  }else{
    return tree;
  }
}

static void clear_string_memory(StringList* list) {
  StringList* it = list;
  while (it) {
    StringList* tmp = it;
    it = it->next;
    // clear tmp
    free(tmp->str);
    free(tmp);
  }
}

//// TODO: clear referenced_by-list of LabelTree AND labels-list of Code/Data-Atoms
void free_hell_program(HellProgram** hell) {
  if (!hell) {
    return;
  }
  if (!*hell) {
    return;
  }
  // free all memory
  free_labeltree((*hell)->labels);
  clear_string_memory((*hell)->string_memory);
  HellBlock* it = (*hell)->blocks;
  while (it) {
    HellBlock* tmp = it;
    it = it->next;
    // clear tmp
    free_code(tmp->code);
    free_data(tmp->data);
    free(tmp);
  }
  free(*hell);
  *hell = NULL;
}


//////////// HeLL program generation /////////////

// TODO: reduce size of generated HeLL code even more

/*

TODO: speed up:

speed up rotwidth-loop: do computations only for (at most) the first 20 (?) iterations;
  afterwards: switch to other inner call, which should only be used to adjust ROT variables.

speed up EQ and NEQ test by writing own comparison method (instead of calling SUB two times as of now).

speed up getc by more efficient testing for C21, C2 (using SUB multiple times as of now is the lazy, but slow way to implement this test)

maybe speed up: MODULO: double modulo every time until not successfull, then reset and begin doubling again
     ---> ensure that there must not occur any overflow in doubled mod. otherwise, start from original again...
*/

typedef struct {
  const char* name;
  int counter;
} HellVariable;

typedef enum {
  REG_A = 0, REG_B = 1, REG_C = 2, REG_D = 3,
  REG_BP = 4, REG_SP = 5,
  ALU_SRC = 6, ALU_DST = 7, TMP = 8, CARRY = 9, VAL_1222 = 10,
  TMP2 = 11, TMP3 = 12
} HellVariables;


static HellVariable HELL_VARIABLES[] = {
  {"reg_a", 0},
  {"reg_b", 0},
  {"reg_c", 0},
  {"reg_d", 0},
  {"reg_bp", 0},
  {"reg_sp", 0},
  {"alu_src", 0},
  {"alu_dst", 0},
  {"tmp", 0},
  {"carry", 0},
  {"val_1222", 0},
  {"tmp2", 0},
  {"tmp3", 0},
  {"", 0} // NULL instead of "" crashes 8cc
};
// --> labels: opr_reg_a, rot_reg_a, reg_a; opr_reg_b, ...; .....

static int num_flags = 0;
static int num_rotwidth_loop_calls = 0;
static int num_local_labels = 0;

typedef struct {
  const char* name;
  int counter;
} HellFlag;

typedef enum {
  FLAG_BASIS_ALU=0,
  FLAG_OPR_MEM=1,
  FLAG_MEM_COMPUTE=2,
  FLAG_MEM_ACCESS=3,
  FLAG_TEST_LT=4,
  FLAG_TEST_EQ=5,
  FLAG_MODULO=6,
  FLAG_ARITHMETIC_OR_IO=7
} HeLLFlags;

static HellFlag HELL_FLAGS[] = {
  {"BASIS_ALU", 0},
  {"OPR_MEM", 0},
  {"MEM_COMPUTE", 0},
  {"MEM_ACCESS", 0},
  {"TEST_LT", 0},
  {"TEST_EQ", 0},
  {"MODULO", 0},
  {"ARITHMETIC_OR_IO", 0},
  {"", 0}
};

typedef struct {
  const char* name;
  int flag;
  int counter;
} HeLLFunction;


typedef enum {
  HELL_GENERATE_1222=0,
  HELL_ADD=1,
  HELL_SUB=2,
  HELL_OPR_MEMPTR=3,
  HELL_OPR_MEMORY=4,
  HELL_COMPUTE_MEMPTR=5,
  HELL_READ_MEMORY=6,
  HELL_WRITE_MEMORY=7,
  HELL_TEST_LT=8,
  HELL_TEST_GE=9,
  HELL_TEST_EQ=10,
  HELL_TEST_NEQ=11,
  HELL_MODULO=12,
  HELL_SUB_UINT24=13,
  HELL_ADD_UINT24=14,
  HELL_GETC=15,
  HELL_PUTC=16,
  HELL_TEST_ALU_DST=17
} HeLLFunctions;

static HeLLFunction HELL_FUNCTIONS[] = {
  {"generate_1222", FLAG_BASIS_ALU, 0},
  {"add", FLAG_BASIS_ALU, 0},
  {"sub", FLAG_BASIS_ALU, 0},
  {"opr_memptr", FLAG_OPR_MEM, 0},
  {"opr_memory", FLAG_OPR_MEM, 0},
  {"compute_memptr", FLAG_MEM_COMPUTE, 0},
  {"read_memory", FLAG_MEM_ACCESS, 0},
  {"write_memory", FLAG_MEM_ACCESS, 0},
  {"test_lt", FLAG_TEST_LT, 0},
  {"test_ge", FLAG_TEST_LT, 0},
  {"test_eq", FLAG_TEST_EQ, 0},
  {"test_neq", FLAG_TEST_EQ, 0},
  {"modulo", FLAG_MODULO, 0},
  {"sub_uint24", FLAG_ARITHMETIC_OR_IO, 0},
  {"add_uint24", FLAG_ARITHMETIC_OR_IO, 0},
  {"getc", FLAG_ARITHMETIC_OR_IO, 0},
  {"putc", FLAG_ARITHMETIC_OR_IO, 0},
  {"test_alu_dst",FLAG_BASIS_ALU}
};

typedef enum {
  HELL_VAR_TO_ALU_DST = 0,
  HELL_VAR_TO_ALU_SRC = 1,
  HELL_VAR_READ_0t = 2,
  HELL_VAR_WRITE = 3,
  HELL_ALU_DST_TO_VAR = 4
} ModifyMode;

int modify_var_counter[5][8] = {
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0}
};


// initialization and finalization (almost finalization)
static void emit_jmp_alusrc_base();
static void emit_copy_var_aludst_base(int var);
static void emit_copy_var_alusrc_base(int var);
static void emit_read_var_0t_base(int var);
static void emit_write_var_base(int var);
static void emit_copy_aludst_to_var_base(int var);
static void emit_modify_var_footer(int var, ModifyMode access); // generate function footer for variable-modifying functions
static void emit_test_alu_dst_base(); // set CARRY flag if ALU_DST == 0; clear CARRY flag if ALU_DST == 1; otherwise: crash
static void emit_putc_base(); // write a character from ALU_DST to stdout (modulo 256) ;;; changes ALU_DST by mod 256 computation ;;; changes TMP2, TMP3
static void emit_getc_base(); // read a character from stdin into ALU_DST (modulo 256; revert newline to '\n'; convert EOF to '\0') ;;; changes ALU_SRC, TMP2, TMP3
static void emit_add_uint24_base(); // arithmetic: add ALU_SRC to ALU_DST; ALU_SRC gets destroyed! ;;; side effect: changes TMP2, TMP3
static void emit_sub_uint24_base(); // arithmetic: sub ALU_SRC from ALU_DST; ALU_SRC gets destroyed! ;;; side effect: changes TMP2, TMP3
static void emit_modulo_base(); // ALU_DST := ALU_DST % ALU_SRC ;;; side effect: changes TMP2, TMP3
static void emit_test_neq_base(); // ALU_DST := (ALU_DST != ALU_SRC)?1:0
static void emit_test_eq_base(); // ALU_DST := (ALU_DST == ALU_SRC)?1:0
static void emit_test_ge_base(); // ALU_DST := (ALU_DST >= ALU_SRC)?1:0
static void emit_test_lt_base(); // ALU_DST := (ALU_DST < ALU_SRC)?1:0
static void emit_write_memory_base(); // copy ALU_DST to memory[ALU_SRC] ;;; side effect: changes TMP2, TMP3
static void emit_read_memory_base(); // copy memory[ALU_SRC] to ALU_DST ;;; side effect: changes TMP2
static void emit_compute_memptr_base(); // MEMPTR := (MEMORY - 2) - 2* ALU_SRC ;;; side effect: changes TMP2
static void emit_memory_access_base(); // OPR memory // OPR memptr
static void emit_add_base(); // arithmetic: add ALU_SRC to ALU_DST; ALU_SRC gets destroyed!; tmp_IS_C1-flag: overflow
static void emit_sub_base(); // arithmetic: sub ALU_SRC from ALU_DST; ALU_SRC gets destroyed!; tmp_IS_C1-flag: overflow ;;;; side effect: ALU_DST changes from 0t.. to 1t..
static void emit_generate_val_1222_base(); // set VAL_1222 to 1t22.22
static void emit_rotwidth_loop_base();
static void emit_hell_variables_base(); // declare variables used in HeLL
static void emit_branch_lookup_table(); // lookup-table to perform jmp instruction by address stored in register
static void emit_function_footer(HeLLFunctions hf); // generate function footer
static void emit_flags();




// use
static void emit_call(HeLLFunctions hf); // call some function
static void emit_modify_var(int var, ModifyMode access); // read, write, or clear variable
static void emit_jmp(Value* jmp);
static void emit_read_0tvar(int var); // set VAL_1222 to 1t22..22, read a value starting with 0t from var
static void emit_read_1tvar(int var); // set VAL_1222 to 1t22..22, read a value starting with 1t from var
static void emit_clear_var(int var); // set var AND tmp to C1, prepare for writing...
static void emit_write_var(int var); // write to var; tmp and var must be set to C1 before
static void emit_opr_var(int var); // OPR var
static void emit_rot_var(int var); // ROT var
static void emit_test_var(int var); // MOVD var
static void emit_rotwidth_loop_begin(); // for (int i=0; i<rotwidth; i++) { if (i<20) { /* execute the following code at most 20 times */
static void emit_rotwidth_loop_always(); // } /* execute the following code rotwidth times */
static void emit_rotwidth_loop_end();   // }
static int hell_cmp_call(Inst* inst);
static void hell_read_value(Value* val);
static void emit_load_immediate(unsigned int immediate);
static void emit_load_immediate_ter(HellImmediate* immediate);
static void emit_load_expression(const char* expression, int preceeding_1t);

// helper
static void dec_ternary_string(char* str);


static void hell_emit_gen_1222() {
  emit_call(HELL_GENERATE_1222);
}

static void emit_modify_var(int var, ModifyMode access) {
  modify_var_counter[access][var]++;
  emit_label_reference(make_string("MODIFY_VAR_RETURN%u",modify_var_counter[access][var]),+1);
  switch (access) {
    case HELL_VAR_TO_ALU_DST:
      emit_label_reference("MOVD",0);
      emit_label_reference(make_string("copy_var_to_aludst_%s",HELL_VARIABLES[var].name),0);
      emit_finalize_block();
      emit_label(make_string("copy_var_to_aludst_%s_ret%u",HELL_VARIABLES[var].name,modify_var_counter[access][var]));
      break;
    case HELL_VAR_TO_ALU_SRC:
      emit_label_reference("MOVD",0);
      emit_label_reference(make_string("copy_var_to_alusrc_%s",HELL_VARIABLES[var].name),0);
      emit_finalize_block();
      emit_label(make_string("copy_var_to_alusrc_%s_ret%u",HELL_VARIABLES[var].name,modify_var_counter[access][var]));
      break;
    case HELL_VAR_READ_0t:
      emit_label_reference("MOVD",0);
      emit_label_reference(make_string("read_var_0t_%s",HELL_VARIABLES[var].name),0);
      emit_finalize_block();
      emit_label(make_string("read_var_0t_%s_ret%u",HELL_VARIABLES[var].name,modify_var_counter[access][var]));
      break;
    case HELL_VAR_WRITE:
      emit_label_reference("MOVD",0);
      emit_label_reference(make_string("WRITE_%s",HELL_VARIABLES[var].name),0);
      emit_finalize_block();
      emit_label(make_string("write_%s_ret%u",HELL_VARIABLES[var].name,modify_var_counter[access][var]));
      break;
    case HELL_ALU_DST_TO_VAR:
      emit_label_reference("MOVD",0);
      emit_label_reference(make_string("copy_aludst_to_var_%s",HELL_VARIABLES[var].name),0);
      emit_finalize_block();
      emit_label(make_string("copy_aludst_to_var_%s_ret%u",HELL_VARIABLES[var].name,modify_var_counter[access][var]));
      break;
  }
}

static void emit_modify_var_footer(int var, ModifyMode access) {
  for (int i=1; i<=modify_var_counter[access][var]; i++) {
    const char* ret;
    switch (access) {
      case HELL_VAR_TO_ALU_DST:
        ret = "copy_var_to_aludst";
        break;
      case HELL_VAR_TO_ALU_SRC:
        ret = "copy_var_to_alusrc";
        break;
      case HELL_VAR_READ_0t:
        ret = "read_var_0t";
        break;
      case HELL_VAR_WRITE:
        ret = "write";
        break;
      case HELL_ALU_DST_TO_VAR:
        ret = "copy_aludst_to_var";
        break;
      default:
        error("oops");
    }
    emit_label_reference(make_string("MODIFY_VAR_RETURN%u",i),0);
    emit_label_reference(make_string("%s_%s_ret%u", ret, HELL_VARIABLES[var].name, i),0);
    emit_label_reference(make_string("MODIFY_VAR_RETURN%u",i),+1);
  }
  emit_finalize_block();
}

static void emit_copy_var_aludst_base(int var) {
  emit_label(make_string("copy_var_to_aludst_%s",HELL_VARIABLES[var].name));
  emit_label_reference("MOVD",+1);
  emit_clear_var(ALU_DST);
  emit_read_0tvar(var);
  emit_write_var(ALU_DST);
  emit_modify_var_footer(var, HELL_VAR_TO_ALU_DST);

}

static void emit_copy_var_alusrc_base(int var) {
  emit_label(make_string("copy_var_to_alusrc_%s",HELL_VARIABLES[var].name));
  emit_label_reference("MOVD",+1);
  emit_clear_var(ALU_SRC);
  emit_read_0tvar(var);
  emit_write_var(ALU_SRC);
  emit_modify_var_footer(var, HELL_VAR_TO_ALU_SRC);

}

static void emit_read_var_0t_base(int var) {
  emit_label(make_string("read_var_0t_%s",HELL_VARIABLES[var].name));
  emit_label_reference("MOVD",+1);
  emit_read_0tvar(var);
  emit_modify_var_footer(var, HELL_VAR_READ_0t);

}

static void emit_write_var_base(int var) {
  int flag_base = num_flags;
  num_flags += 4;

  emit_label(make_string("WRITE_%s",HELL_VARIABLES[var].name));
  emit_label_reference("MOVD",+1);
  
  // SAVE A REGISTER

  // OPR into TMP VAR
  emit_label_reference(make_string("FLAG%u", flag_base+1),+1);
  emit_label_reference("MOVD",0);
  emit_label_reference(make_string("write_%s_opr_tmp",HELL_VARIABLES[var].name),0);
  emit_finalize_block();
  emit_label(make_string("write_%s_tmp_ret1",HELL_VARIABLES[var].name));
  // OPR into TMP2 VAR
  emit_label_reference(make_string("FLAG%u", flag_base+1),+1);
  emit_label_reference("MOVD",0);
  emit_label_reference(make_string("write_%s_opr_tmp2",HELL_VARIABLES[var].name),0);
  emit_finalize_block();

  emit_label(make_string("write_%s_tmp2_ret1",HELL_VARIABLES[var].name));
  // clear destination variable
  emit_clear_var(var);

  // restore A register
  num_local_labels++;
  emit_label(make_string("local_label_%u",num_local_labels));
  emit_label_reference("ROT",0);
  emit_immediate(0,"0");
  emit_label_reference("ROT",+1);
  emit_opr_var(VAL_1222);
  // OPR into TMP2 VAR
  emit_label_reference(make_string("FLAG%u", flag_base+2),+1);
  emit_label_reference("MOVD",0);
  emit_label_reference(make_string("write_%s_opr_tmp2",HELL_VARIABLES[var].name),0);
  emit_finalize_block();
  emit_label(make_string("write_%s_tmp2_ret2",HELL_VARIABLES[var].name));
  emit_label_reference("LOOP2",0);
  emit_label_reference(make_string("local_label_%u",num_local_labels),0);
  
  // WRITE A REGISTER
  emit_write_var(var);

  // RESET local TMP, TMP2 VARS
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label_reference(make_string("FLAG%u", flag_base+2),+1);
  emit_label_reference("MOVD",0);
  emit_label_reference(make_string("write_%s_opr_tmp",HELL_VARIABLES[var].name),0);
  emit_finalize_block();
  emit_label(make_string("write_%s_tmp_ret2",HELL_VARIABLES[var].name));
  emit_label_reference(make_string("FLAG%u", flag_base+3),+1);
  emit_label_reference("MOVD",0);
  emit_label_reference(make_string("write_%s_opr_tmp",HELL_VARIABLES[var].name),0);
  emit_finalize_block();
  emit_label(make_string("write_%s_tmp_ret3",HELL_VARIABLES[var].name));
  emit_label_reference(make_string("FLAG%u", flag_base+3),+1);
  emit_label_reference("MOVD",0);
  emit_label_reference(make_string("write_%s_opr_tmp2",HELL_VARIABLES[var].name),0);
  emit_finalize_block();
  emit_label(make_string("write_%s_tmp2_ret3",HELL_VARIABLES[var].name));
  emit_label_reference(make_string("FLAG%u", flag_base+4),+1);
  emit_label_reference("MOVD",0);
  emit_label_reference(make_string("write_%s_opr_tmp2",HELL_VARIABLES[var].name),0);
  emit_finalize_block();

  emit_label(make_string("write_%s_tmp2_ret4",HELL_VARIABLES[var].name));
  // return
  emit_modify_var_footer(var, HELL_VAR_WRITE);


  emit_label(make_string("write_%s_opr_tmp",HELL_VARIABLES[var].name));
  emit_label_reference("OPR",0);
  emit_immediate(1,"1");
  emit_label_reference("OPR",+1);
  emit_label_reference("MOVD",+1);
  for (int i=1;i<=3;i++) {
    emit_label_reference(make_string("FLAG%u",flag_base+i),0);
    emit_label_reference(make_string("write_%s_tmp_ret%u",HELL_VARIABLES[var].name,i),0);
    emit_label_reference(make_string("FLAG%u",flag_base+i),+1);
  }
  emit_finalize_block();
  emit_label(make_string("write_%s_opr_tmp2",HELL_VARIABLES[var].name));
  emit_label_reference("OPR",0);
  emit_immediate(1,"1");
  emit_label_reference("OPR",+1);
  emit_label_reference("MOVD",+1);
  for (int i=1;i<=4;i++) {
    emit_label_reference(make_string("FLAG%u",flag_base+i),0);
    emit_label_reference(make_string("write_%s_tmp2_ret%u",HELL_VARIABLES[var].name,i),0);
    emit_label_reference(make_string("FLAG%u",flag_base+i),+1);
  }
  emit_finalize_block();
}


static void emit_copy_aludst_to_var_base(int var) {
  emit_label(make_string("copy_aludst_to_var_%s",HELL_VARIABLES[var].name));
  emit_label_reference("MOVD",+1);
  emit_clear_var(var);
  emit_read_0tvar(ALU_DST);
  emit_write_var(var);
  emit_modify_var_footer(var, HELL_ALU_DST_TO_VAR);
}


static void emit_load_immediate_ter(HellImmediate* immediate) {
  if (immediate == 0) {
    error("oops");
  }else{
    // build ternary values
    char ternary1[21];
    char ternary2[21];
    char ternary3[21];
    ternary1[20] = 0;
    ternary2[20] = 0;
    ternary3[20] = 0;
    int i = 20;
    int j = 0;
    while (immediate->suffix[j]) {
      j++;
    }
    if (j>i) {
      error("oops");
    }
    int need_three_opr = immediate->praefix_1t;
    while (i) {
      i--;
      int modulus;
      if (j) {
        j--;
        modulus = immediate->suffix[j] - '0';
      }else{
        modulus = immediate->praefix_1t;
      }
      int odd = modulus%2;
      if (odd) {
        need_three_opr = 1;
      }
      ternary3[i] = '0' + modulus;
      ternary2[i] = '1' - odd;
      ternary1[i] = '0' + 2*odd;
    }
    emit_label_reference("ROT",0);
    emit_immediate(1,"1");
    emit_label_reference("ROT",+1);
    if (immediate->praefix_1t) {
      emit_label_reference("OPR",0);
      emit_immediate(0,"22222222222222222222");
      emit_label_reference("OPR",+1);
      emit_label_reference("OPR",0);
      emit_immediate(1,"00000000000000000000");
      emit_label_reference("OPR",+1);
      emit_label_reference("OPR",0);
      emit_immediate(0,"11111111111111111111");
      emit_label_reference("OPR",+1);
    }
    if (need_three_opr) {
      emit_label_reference("OPR",0);
      emit_immediate(immediate->praefix_1t,make_string("%s",ternary1));
      emit_label_reference("OPR",+1);
      emit_label_reference("OPR",0);
      emit_immediate(1-immediate->praefix_1t,make_string("%s",ternary2));
      emit_label_reference("OPR",+1);
    }
    emit_label_reference("OPR",0);
    emit_immediate(immediate->praefix_1t,make_string("%s",ternary3));
    emit_label_reference("OPR",+1);
  }
}



static void emit_load_immediate(unsigned int immediate) {
  if (immediate == 0) {
    emit_label_reference("ROT",0);
    emit_immediate(0,"0");
    emit_label_reference("ROT",+1);
  }else{
    // build ternary values
    char ternary1[20];
    char ternary2[20];
    char ternary3[20];
    ternary1[19] = 0;
    ternary2[19] = 0;
    ternary3[19] = 0;
    int i = 19;
    int need_three_opr = 0;
    while (immediate) {
      i--;
      int modulus = immediate%3;
      int odd = modulus%2;
      if (odd) {
        need_three_opr = 1;
      }
      ternary3[i] = '0' + modulus;
      ternary2[i] = '1' - odd;
      ternary1[i] = '0' + 2*odd;
      immediate /= 3;
    }
    emit_label_reference("ROT",0);
    emit_immediate(1,"1");
    emit_label_reference("ROT",+1);
    if (need_three_opr) {
      emit_label_reference("OPR",0);
      emit_immediate(0,make_string("%s",ternary1+i));
      emit_label_reference("OPR",+1);
      emit_label_reference("OPR",0);
      emit_immediate(1,make_string("%s",ternary2+i));
      emit_label_reference("OPR",+1);
    }
    emit_label_reference("OPR",0);
    emit_immediate(0,make_string("%s",ternary3+i));
    emit_label_reference("OPR",+1);
  }
}


static void emit_load_expression(const char* expression, int preceeding_1t) {
  num_local_labels++;
  emit_label(make_string("local_label_%u",num_local_labels));
  if (preceeding_1t) {
    emit_label_reference("ROT",0);
    emit_immediate(1,"1");
    emit_label_reference("ROT",+1);
  }else{
    emit_label_reference("ROT",0);
    emit_immediate(0,"0");
    emit_label_reference("ROT",+1);
  }
  emit_opr_var(VAL_1222);
  emit_label_reference("OPR",0);
  emit_label_reference(make_string("%s",expression),0);
  emit_label_reference("OPR",+1);
  emit_label_reference("LOOP2",0);
  emit_label_reference(make_string("local_label_%u",num_local_labels),0);
}


static int hell_cmp_call(Inst* inst) {
  int op = normalize_cond(inst->op, 0);
  switch (op) {
    case JEQ:
      return HELL_TEST_EQ;
    case JNE:
      return HELL_TEST_NEQ;
    case JLT:
      return HELL_TEST_LT;
    case JGT:
      return HELL_TEST_LT;
    case JLE:
      return HELL_TEST_GE;
    case JGE:
      return HELL_TEST_GE;
    default:
      error("oops");
  }
}

static void hell_read_value(Value* val) {
    if (val->type == REG) {
      emit_modify_var(val->reg, HELL_VAR_READ_0t);
    }else if (val->type == IMM) {
      emit_load_immediate(val->imm);
    }else{
      error("invalid value");
    }
}


static void hell_emit_inst(Inst* inst) {
  switch (inst->op) {
    case MOV:
    hell_read_value(&inst->src);
    emit_modify_var(inst->dst.reg, HELL_VAR_WRITE);
    break;

  case ADD:
    // copy to ALU
    emit_modify_var(inst->dst.reg, HELL_VAR_TO_ALU_DST);
    hell_read_value(&inst->src);
    emit_modify_var(ALU_SRC, HELL_VAR_WRITE);

    // compute
    emit_call(HELL_ADD_UINT24);

    // copy result back
    emit_modify_var(inst->dst.reg, HELL_ALU_DST_TO_VAR);
    break;

  case SUB:
    // copy to ALU
    emit_modify_var(inst->dst.reg, HELL_VAR_TO_ALU_DST);
    hell_read_value(&inst->src);
    emit_modify_var(ALU_SRC, HELL_VAR_WRITE);

    // compute
    emit_call(HELL_SUB_UINT24);

    // copy result back
    emit_modify_var(inst->dst.reg, HELL_ALU_DST_TO_VAR);
    break;

  case LOAD:
    // copy to ALU
    hell_read_value(&inst->src);
    emit_modify_var(ALU_SRC, HELL_VAR_WRITE);

    // compute
    emit_call(HELL_READ_MEMORY);

    // copy result back
    emit_modify_var(inst->dst.reg, HELL_ALU_DST_TO_VAR);
    break;

  case STORE:
    // copy to ALU
    hell_read_value(&inst->src);
    emit_modify_var(ALU_SRC, HELL_VAR_WRITE);
    emit_modify_var(inst->dst.reg, HELL_VAR_TO_ALU_DST);

    // compute
    emit_call(HELL_WRITE_MEMORY);
    break;

  case PUTC:
    // copy to ALU (NOTE: from inst->src to ALU_DST)
    hell_read_value(&inst->src);
    emit_modify_var(ALU_DST, HELL_VAR_WRITE);

    // compute
    emit_call(HELL_PUTC);
    break;

  case GETC:
    // compute
    emit_call(HELL_GETC);

    // copy result back
    emit_modify_var(inst->dst.reg, HELL_ALU_DST_TO_VAR);
    break;

  case EXIT:
    emit_label_reference("HALT",0);
    break;

  case DUMP:
    // emit_label_reference("NOP",0); // necessary? remove this line?
    break;

  case EQ:
  case NE:
  case LT:
  case GT:
  case LE:
  case GE:
    // copy to ALU
    // for GT and LE operation: toggle SRC and DST
    if (inst->op == GT || inst->op == LE) {
      emit_modify_var(inst->dst.reg, HELL_VAR_TO_ALU_SRC);
      hell_read_value(&inst->src);
      emit_modify_var(ALU_DST, HELL_VAR_WRITE);
    }else{
      emit_modify_var(inst->dst.reg, HELL_VAR_TO_ALU_DST);
      hell_read_value(&inst->src);
      emit_modify_var(ALU_SRC, HELL_VAR_WRITE);
    }

    // compute
    emit_call(hell_cmp_call(inst));

    // copy result back
    emit_modify_var(inst->dst.reg, HELL_ALU_DST_TO_VAR);
    break;


  case JEQ:
  case JNE:
  case JLT:
  case JGT:
  case JLE:
  case JGE:
    // copy to ALU
    if (inst->op == JGT || inst->op == JLE) {
      // for JGT and JLE operation: toggle SRC and DST
      emit_modify_var(inst->dst.reg, HELL_VAR_TO_ALU_SRC);
      hell_read_value(&inst->src);
      emit_modify_var(ALU_DST, HELL_VAR_WRITE);
    }else{
      emit_modify_var(inst->dst.reg, HELL_VAR_TO_ALU_DST);
      hell_read_value(&inst->src);
      emit_modify_var(ALU_SRC, HELL_VAR_WRITE);
    }

    // compare
    emit_call(hell_cmp_call(inst));

    // test result
    {
      emit_call(HELL_TEST_ALU_DST); // set carry_IS_C1 if ALU_DST is 0; unset if ALU_DST is 1
      num_local_labels++;
      int dntjmp = num_local_labels;
      emit_label_reference(make_string("%s_IS_C1", HELL_VARIABLES[CARRY].name),0);
      emit_label_reference(make_string("dont_jmp_%u", dntjmp),0);
      emit_jmp(&inst->jmp);
      emit_label(make_string("dont_jmp_%u", dntjmp));
      emit_label_reference("NOP",0);
    }
    break;

  case JMP:
    emit_jmp(&inst->jmp);
    break;

  default:
    error("oops");
  }
}

static void emit_jmp(Value* jmp) {
  if (jmp->type == REG) {
    emit_modify_var(jmp->reg, HELL_VAR_TO_ALU_SRC);
    emit_label_reference("MOVD",0);
    emit_label_reference("jmp_to_alu_src",0);
  }else if (jmp->type == IMM) {
    emit_label_reference("MOVD",0);
    emit_label_reference(make_string("direct_jmp_label_pc%u", jmp->imm),0);
  }else{
    error("invalid value");
  }
  emit_finalize_block();
}

static void emit_jmp_alusrc_base() {
    emit_label("jmp_to_alu_src");
    emit_label_reference("MOVD",+1);
    // save address
    emit_clear_var(TMP2);
    emit_read_0tvar(ALU_SRC);
    emit_write_var(TMP2);
    
    // pc_lookup_table
    emit_load_expression("pc_lookup_table", 1);
    emit_modify_var(ALU_DST, HELL_VAR_WRITE);

    // compute position in lookup table
    emit_call(HELL_ADD);
    emit_read_0tvar(TMP2);
    emit_modify_var(ALU_SRC,HELL_VAR_WRITE);
    emit_call(HELL_ADD);

    // change 0t.. prefix to 1t..
    emit_clear_var(ALU_SRC);
    emit_read_1tvar(ALU_DST);
    emit_write_var(ALU_SRC);

    // do the jmp using the lookup table
    emit_label_reference("MOVDMOVD",0);
    emit_label_reference(make_string("%s",HELL_VARIABLES[ALU_SRC].name),-3);
}


static void emit_call(HeLLFunctions hf) {
  HELL_FUNCTIONS[hf].counter++;
  if (HELL_FLAGS[HELL_FUNCTIONS[hf].flag].counter < HELL_FUNCTIONS[hf].counter) {
    HELL_FLAGS[HELL_FUNCTIONS[hf].flag].counter = HELL_FUNCTIONS[hf].counter;
  }
  emit_label_reference(make_string("%s%u",HELL_FLAGS[HELL_FUNCTIONS[hf].flag].name,HELL_FUNCTIONS[hf].counter),+1);
  emit_label_reference("MOVD",0);
  emit_label_reference(make_string("%s",HELL_FUNCTIONS[hf].name),0);

  emit_finalize_block();
  emit_label(make_string("%s_ret%u",HELL_FUNCTIONS[hf].name,HELL_FUNCTIONS[hf].counter));
}


static void emit_function_footer(HeLLFunctions hf) {
  for (int i=1; i<=HELL_FUNCTIONS[hf].counter; i++) {
    emit_label_reference(make_string("%s%u",HELL_FLAGS[HELL_FUNCTIONS[hf].flag].name,i),0);
    emit_label_reference(make_string("%s_ret%u",HELL_FUNCTIONS[hf].name,i),0);
    emit_label_reference(make_string("%s%u",HELL_FLAGS[HELL_FUNCTIONS[hf].flag].name,i),+1);
  }
  emit_finalize_block();
}

static void emit_flags() {
  for (int j=0; HELL_FLAGS[j].name[0]; j++) {
    for (int i=1; i<=HELL_FLAGS[j].counter; i++) {
      emit_label(make_string("%s%u",HELL_FLAGS[j].name,i));
      emit_xlat_cycle('o','j',0);
      emit_xlat_cycle('i',0);
      emit_finalize_block();
    }
  }
}



// set VAL_1222 to 1t22..22, read from var
static void emit_read_0tvar(int var) {
  if (var == TMP || var == CARRY || var == VAL_1222) {
    error("oops");
  }
  num_local_labels++;
  emit_label(make_string("local_label_%u",num_local_labels));
  emit_label_reference("ROT",0);
  emit_immediate(0,"0");
  emit_label_reference("ROT",+1);
  emit_opr_var(VAL_1222);
  emit_opr_var(var);
  emit_label_reference("LOOP2",0);
  emit_label_reference(make_string("local_label_%u",num_local_labels),0);
}

// set VAL_1222 to 1t22..22, read from var, but with preceeding 1t11..
static void emit_read_1tvar(int var) {
  if (var == TMP || var == CARRY || var == VAL_1222) {
    error("oops");
  }
  num_local_labels++;
  emit_label(make_string("local_label_%u",num_local_labels));
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_opr_var(VAL_1222);
  emit_opr_var(var);
  emit_label_reference("LOOP2",0);
  emit_label_reference(make_string("local_label_%u",num_local_labels),0);
}

// set var AND tmp to C1, prepare for writing...
static void emit_clear_var(int var) {
  if (var == TMP || var == VAL_1222) {
    error("oops");
  }
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  num_local_labels++;
  emit_label(make_string("local_label_%u",num_local_labels));
  emit_opr_var(var);
  emit_label_reference("LOOP2",0);
  emit_label_reference(make_string("local_label_%u",num_local_labels),0);
  num_local_labels++;
  emit_label(make_string("local_label_%u",num_local_labels));
  emit_opr_var(TMP);
  emit_label_reference("LOOP2",0);
  emit_label_reference(make_string("local_label_%u",num_local_labels),0);
}

// write to var; tmp and var must be set to C1 before
static void emit_write_var(int var) {
  if (var == TMP || var == CARRY || var == VAL_1222) {
    error("oops");
  }
  emit_opr_var(TMP);
  emit_opr_var(var);
}



static void emit_test_alu_dst_base() {
  if (!HELL_FUNCTIONS[HELL_TEST_ALU_DST].counter) {
    return;
  }
  emit_label("test_alu_dst");
  emit_label_reference("MOVD",+1);
  emit_clear_var(CARRY);
  emit_read_0tvar(ALU_DST);
  emit_opr_var(CARRY);
  emit_test_var(CARRY);
  emit_function_footer(HELL_TEST_ALU_DST);
}


static void emit_putc_base() {
  if (!HELL_FUNCTIONS[HELL_PUTC].counter) {
    return;
  }
  emit_label("putc");
  emit_label_reference("MOVD",+1);

  // do mod 256 computation
  // write 256 into ALU_SRC
  emit_clear_var(ALU_SRC);
  emit_load_immediate(256);
  emit_write_var(ALU_SRC);
  emit_call(HELL_MODULO);

  emit_read_0tvar(ALU_DST);
  emit_label_reference("OUT",0);
  emit_unused_cell();
  emit_label_reference("OUT",+1);

  emit_function_footer(HELL_PUTC);
}


static void emit_getc_base() {
  if (!HELL_FUNCTIONS[HELL_GETC].counter) {
    return;
  }
  emit_label("getc");
  emit_label_reference("MOVD",+1);

  emit_clear_var(ALU_DST);
  emit_label_reference("IN",0);
  emit_unused_cell();
  emit_label_reference("IN",+1);
  emit_write_var(ALU_DST);

  // save in TMP3
  emit_clear_var(TMP3);
  emit_read_0tvar(ALU_DST);
  emit_write_var(TMP3);

  // detect C21 and C2

  // DESTROY preceeding 2t... by moving the value back from TMP3
  emit_clear_var(ALU_DST);
  emit_read_0tvar(TMP3);
  emit_write_var(ALU_DST);

  // read UINT_MAX = 16777215 into ALU_SRC (which is much larger than largest Unicode code point,
  // but much smaller than modified special Malbolge Unshackled encoding 0t22..21 or 0t22..22
  emit_clear_var(ALU_SRC);
  HellImmediate UINT_MAX_TER = {0, "1011120101000100"};
  emit_load_immediate_ter(&UINT_MAX_TER);
  emit_write_var(ALU_SRC);

  // test if alu_dst is less than alu_src
  emit_call(HELL_SUB);
  emit_clear_var(ALU_DST);
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label_reference(make_string("%s_IS_C1", HELL_VARIABLES[TMP].name),0);
  emit_label_reference("handle_normal_input_character",0);

  // handle special input character:
  // only the last trit matters to determine whether newline or EOF has been read
  // restore last trit from TMP3
  emit_clear_var(ALU_DST);
  emit_label_reference("ROT",0);
  emit_immediate(0,"0");
  emit_label_reference("ROT",+1);
  emit_label_reference("OPR",0);
  emit_immediate(1,"2");
  emit_label_reference("OPR",+1);
  emit_opr_var(ALU_DST); // set ALU_DST to 0t2
  emit_read_1tvar(TMP3);
  emit_opr_var(ALU_DST);
  // if last trit of input has been '2', ALU_DST will now be '1'
  // if last trit of input has been '1', ALU_DST will now be '2'

  // now compare with 2:
  // read 2 into ALU_SRC
  emit_clear_var(ALU_SRC);
  emit_load_immediate(2);
  emit_write_var(ALU_SRC);

  // test if alu_dst is less than alu_src
  emit_call(HELL_SUB);
  emit_clear_var(ALU_DST);
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label_reference(make_string("%s_IS_C1", HELL_VARIABLES[TMP].name),0);
  emit_label_reference("handle_eof_input",0);


  // handle newline
  // write '\n' into ALU_DST
  emit_clear_var(ALU_DST);
  emit_load_immediate('\n');
  emit_write_var(ALU_DST);
  emit_label_reference("MOVD",0);
  emit_label_reference("finish_getc",0);


  // handle eof
  emit_label("handle_eof_input");
  // read '\0' into ALU_DST
  emit_clear_var(ALU_DST);
  emit_opr_var(ALU_DST);
  emit_label_reference("MOVD",0);
  emit_label_reference("finish_getc",0);


  // handle normal character
  emit_label("handle_normal_input_character");
  // restore from TMP3
  emit_clear_var(ALU_DST);
  emit_read_0tvar(TMP3);
  emit_write_var(ALU_DST);
  // do mod 256 computation
  // write 256 into ALU_SRC
  emit_clear_var(ALU_SRC);
  emit_load_immediate(256);
  emit_write_var(ALU_SRC);
  emit_call(HELL_MODULO);
  // done.

  emit_label_reference("MOVD",+1);
  emit_label("finish_getc");
  emit_label_reference("MOVD",+1);

  emit_function_footer(HELL_GETC);
}



static void emit_sub_uint24_base() {
  if (!HELL_FUNCTIONS[HELL_SUB_UINT24].counter) {
    return;
  }
  emit_label("sub_uint24");
  emit_label_reference("MOVD",+1);

  // backup ALU_SRC to TMP2
  emit_clear_var(TMP2);
  emit_read_0tvar(ALU_SRC);
  emit_write_var(TMP2);

  // load UINT_MAX+1 into ALU_SRC
  emit_clear_var(ALU_SRC);
  HellImmediate UINT_MAX_STR_PLUS_ONE = {0, "1011120101000101"};
  emit_load_immediate_ter(&UINT_MAX_STR_PLUS_ONE);
  emit_write_var(ALU_SRC);

  // add UINT_MAX+1 to ALU_DST
  emit_call(HELL_ADD);

  // restore ALU_SRC from TMP2
  emit_clear_var(ALU_SRC);
  emit_read_0tvar(TMP2);
  emit_write_var(ALU_SRC);

  // do actual subtraction
  emit_call(HELL_SUB);

  // compute result modulo (UINT_MAX+1)
  emit_clear_var(ALU_SRC);
  emit_load_immediate_ter(&UINT_MAX_STR_PLUS_ONE);
  emit_write_var(ALU_SRC);
  emit_call(HELL_MODULO);

  emit_function_footer(HELL_SUB_UINT24);
}




static void emit_add_uint24_base() {
  if (!HELL_FUNCTIONS[HELL_ADD_UINT24].counter) {
    return;
  }
  emit_label("add_uint24");
  emit_label_reference("MOVD",+1);

  // normal computation
  emit_call(HELL_ADD);

  // read (UINT_MAX+1) into ALU_SRC
  emit_clear_var(ALU_SRC);
  HellImmediate UINT_MAX_STR_PLUS_ONE = {0, "1011120101000101"};
  emit_load_immediate_ter(&UINT_MAX_STR_PLUS_ONE);
  emit_write_var(ALU_SRC);

  // now modulo can be applied
  emit_call(HELL_MODULO);

  emit_function_footer(HELL_ADD_UINT24);
}



static void emit_modulo_base() {
  if (!HELL_FUNCTIONS[HELL_MODULO].counter) {
    return;
  }
  emit_label("modulo");
  emit_label_reference("MOVD",+1);

  // save modulus
  emit_clear_var(TMP3);
  emit_read_0tvar(ALU_SRC);
  emit_write_var(TMP3);

  emit_label("continue_modulo");
  // save current remainder
  emit_clear_var(TMP2);
  emit_read_0tvar(ALU_DST);
  emit_write_var(TMP2);
  // subtract modulus
  emit_call(HELL_SUB);
  // restore modulus
  emit_clear_var(ALU_SRC);
  emit_read_0tvar(TMP3);
  emit_write_var(ALU_SRC);
  // test underflow
  emit_label_reference(make_string("%s_IS_C1", HELL_VARIABLES[TMP].name),+1);
  emit_label_reference(make_string("%s_IS_C1", HELL_VARIABLES[TMP].name),0); // try one more
  emit_label_reference("continue_modulo",0);

  // restore remainder

  emit_clear_var(ALU_DST);
  emit_read_0tvar(TMP2);
  emit_write_var(ALU_DST);

  emit_function_footer(HELL_MODULO);
}


static void emit_test_neq_base() {
  if (!HELL_FUNCTIONS[HELL_TEST_NEQ].counter) {
    return;
  }
  emit_label("test_neq");
  emit_label_reference("MOVD",+1);

  emit_call(HELL_SUB);
  emit_clear_var(ALU_SRC);
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label_reference("OPR",0);
  emit_immediate(0,"2");
  emit_label_reference("OPR",+1);
  emit_label_reference("OPR",0);
  emit_immediate(1,"0");
  emit_label_reference("OPR",+1);
  emit_opr_var(ALU_SRC);

  emit_call(HELL_SUB);
  emit_clear_var(ALU_DST);
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label_reference(make_string("%s_IS_C1", HELL_VARIABLES[TMP].name),0);
  emit_label_reference("is_eq",0);
  emit_label_reference("OPR",0);
  emit_immediate(0,"2");
  emit_label_reference("OPR",+1);
  emit_label_reference("OPR",0);
  emit_immediate(1,"0");
  emit_label_reference("OPR",+1);
  emit_label("is_eq");
  emit_opr_var(ALU_DST);

  emit_function_footer(HELL_TEST_NEQ);
}


static void emit_test_eq_base() {
  if (!HELL_FUNCTIONS[HELL_TEST_EQ].counter) {
    return;
  }
  emit_label("test_eq");
  emit_label_reference("MOVD",+1);

  emit_call(HELL_SUB);
  emit_clear_var(ALU_SRC);
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label_reference("OPR",0);
  emit_immediate(0,"2");
  emit_label_reference("OPR",+1);
  emit_label_reference("OPR",0);
  emit_immediate(1,"0");
  emit_label_reference("OPR",+1);
  emit_opr_var(ALU_SRC);

  emit_call(HELL_SUB);
  emit_clear_var(ALU_DST);
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label_reference(make_string("%s_IS_C1", HELL_VARIABLES[TMP].name),+1);
  emit_label_reference(make_string("%s_IS_C1", HELL_VARIABLES[TMP].name),0);
  emit_label_reference("is_neq",0);
  emit_label_reference("OPR",0);
  emit_immediate(0,"2");
  emit_label_reference("OPR",+1);
  emit_label_reference("OPR",0);
  emit_immediate(1,"0");
  emit_label_reference("OPR",+1);
  emit_label("is_neq");
  emit_opr_var(ALU_DST);

  emit_function_footer(HELL_TEST_EQ);
}


static void emit_test_ge_base() {
  if (!HELL_FUNCTIONS[HELL_TEST_GE].counter) {
    return;
  }
  emit_label("test_ge");
  emit_label_reference("MOVD",+1);

  emit_call(HELL_SUB);
  emit_clear_var(ALU_DST);
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label_reference(make_string("%s_IS_C1", HELL_VARIABLES[TMP].name),0);
  emit_label_reference("is_lt",0);
  emit_label_reference("OPR",0);
  emit_immediate(0,"2");
  emit_label_reference("OPR",+1);
  emit_label_reference("OPR",0);
  emit_immediate(1,"0");
  emit_label_reference("OPR",+1);
  emit_label("is_lt");
  emit_opr_var(ALU_DST);

  emit_function_footer(HELL_TEST_GE);
}


static void emit_test_lt_base() {
  if (!HELL_FUNCTIONS[HELL_TEST_LT].counter) {
    return;
  }
  emit_label("test_lt");
  emit_label_reference("MOVD",+1);

  emit_call(HELL_SUB);
  emit_clear_var(ALU_DST);
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label_reference(make_string("%s_IS_C1", HELL_VARIABLES[TMP].name),+1);
  emit_label_reference(make_string("%s_IS_C1", HELL_VARIABLES[TMP].name),0);
  emit_label_reference("is_ge",0);
  emit_label_reference("OPR",0);
  emit_immediate(0,"2");
  emit_label_reference("OPR",+1);
  emit_label_reference("OPR",0);
  emit_immediate(1,"0");
  emit_label_reference("OPR",+1);
  emit_label("is_ge");
  emit_opr_var(ALU_DST);

  emit_function_footer(HELL_TEST_LT);
}




static void emit_memory_access_base() {
  if (HELL_FUNCTIONS[HELL_OPR_MEMPTR].counter) {
    emit_finalize_block();
    emit_label("opr_memory");
    emit_label_reference("MOVDOPRMOVD",-1); // U_MOVDOPRMOVD memptr
    emit_label("opr_memptr");
    emit_label_reference("OPR",0);
    emit_label("memptr");
    emit_label_reference("MEMORY_0",-2);
    emit_label_reference("OPR",+1);
    emit_label_reference("MOVD",+1);
    emit_function_footer(HELL_OPR_MEMPTR);
  }

  if (HELL_FUNCTIONS[HELL_OPR_MEMORY].counter) {

    emit_finalize_block();
    // backjump
    emit_offset(1,"01112");
    emit_label("return_from_memory_cell");
    emit_label_reference("MOVD",+1);
    emit_label_reference("MOVD",0);
    emit_label_reference("restore_opr_memory",0);

    emit_finalize_block();
    emit_label("restore_opr_memory");
    emit_label_reference("MOVD",+1);
    emit_label("restore_opr_memory_no_r_moved");
    emit_label_reference("PARTIAL_MOVDOPRMOVD",0);
    emit_unused_cell();
    emit_label_reference("MOVDOPRMOVD",+1);
    emit_unused_cell();
    emit_unused_cell();
    emit_unused_cell();
    emit_unused_cell();
    emit_label_reference("LOOP4",0);
    emit_label_reference("half_of_restore_opr_memory_done",0);
    emit_label_reference("MOVD",0);
    emit_label_reference("restore_opr_memory",0);
    emit_finalize_block();
    emit_label("half_of_restore_opr_memory_done");
    emit_label_reference("LOOP2_2",0);
    emit_label_reference("restore_opr_memory_done",0);
    emit_label_reference("PARTIAL_MOVDOPRMOVD",0);
    emit_label_reference("restore_opr_memory_no_r_moved",0);
    emit_finalize_block();
    emit_label("restore_opr_memory_done");
    emit_function_footer(HELL_OPR_MEMORY);
  }
}


static void emit_compute_memptr_base() {
  if (!HELL_FUNCTIONS[HELL_COMPUTE_MEMPTR].counter) {
    return;
  }
  ; // ALU_DST := (MEMORY_0 - 2) - 2* ALU_SRC
  emit_label("compute_memptr");
  emit_label_reference("MOVD",+1);
  // set ALU_DST to MEMORY_0-2
  emit_clear_var(ALU_DST);
  HellImmediate MEMORy_0_MINUS_TWO = {1,"022212"};
  emit_load_immediate_ter(&MEMORy_0_MINUS_TWO);
  emit_write_var(ALU_DST);

  // save ALU_SRC to TMP2
  emit_clear_var(TMP2);
  emit_read_0tvar(ALU_SRC);
  emit_write_var(TMP2);

  // sub ALU_SRC from ALU_DST
  emit_call(HELL_SUB);

  // restore ALU_SRC
  emit_clear_var(ALU_SRC);
  emit_read_0tvar(TMP2);
  emit_write_var(ALU_SRC);

  // sub ALU_SRC from ALU_DST
  emit_call(HELL_SUB);

  // write result into memptr
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_call(HELL_OPR_MEMPTR);
  emit_call(HELL_OPR_MEMPTR);
  emit_opr_var(TMP);
  emit_opr_var(TMP);
  emit_read_1tvar(ALU_DST);
  emit_opr_var(TMP);
  emit_call(HELL_OPR_MEMPTR);

  emit_function_footer(HELL_COMPUTE_MEMPTR);
}


static void emit_write_memory_base() {
  if (!HELL_FUNCTIONS[HELL_WRITE_MEMORY].counter) {
    return;
  }
  emit_label("write_memory");
  emit_label_reference("MOVD",+1);

  // save ALU_DST to TMP3
  emit_clear_var(TMP3);
  emit_read_0tvar(ALU_DST);
  emit_write_var(TMP3);

  emit_call(HELL_COMPUTE_MEMPTR);

  // restore ALU_DST
  emit_clear_var(ALU_DST);
  emit_read_0tvar(TMP3);
  emit_write_var(ALU_DST);

  // add offset (such that uninitialized cells, which are initially 0t10000, equal 0t000)
  // read 0t10000 = 81 into ALU_SRC
  emit_clear_var(ALU_SRC);
  emit_load_immediate(81);
  emit_write_var(ALU_SRC);

  emit_call(HELL_ADD);

  // prepare memory for write (clean cell)
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_call(HELL_OPR_MEMORY);
  emit_call(HELL_OPR_MEMORY);
  emit_opr_var(TMP);
  emit_opr_var(TMP);

  // read value
  emit_read_0tvar(ALU_DST);

  // store value
  emit_opr_var(TMP);
  emit_call(HELL_OPR_MEMORY);

  emit_function_footer(HELL_WRITE_MEMORY);
}


static void emit_read_memory_base() {
  if (!HELL_FUNCTIONS[HELL_READ_MEMORY].counter) {
    return;
  }
  emit_label("read_memory");
  emit_label_reference("MOVD",+1);

  // compute address
  emit_call(HELL_COMPUTE_MEMPTR);

  emit_clear_var(ALU_DST);

  num_local_labels++;
  emit_label(make_string("local_label_%u",num_local_labels));
  emit_label_reference("ROT",0);
  emit_immediate(0,"0");
  emit_label_reference("ROT",+1);
  emit_opr_var(VAL_1222);
  emit_call(HELL_OPR_MEMORY);
  emit_label_reference("LOOP2",0);
  emit_label_reference(make_string("local_label_%u",num_local_labels),0);

  emit_write_var(ALU_DST);

  // make backup copy
  emit_clear_var(TMP2);
  emit_read_0tvar(ALU_DST);
  emit_write_var(TMP2);

  // subtract offset (such that uninitialized cells, which are initially 0t10000, equal 0t000)
  // read 0t10000 = 81 into ALU_SRC
  emit_clear_var(ALU_SRC);
  emit_load_immediate(81);
  emit_write_var(ALU_SRC);

  emit_call(HELL_SUB);
  emit_function_footer(HELL_READ_MEMORY);
}


static void emit_add_base() {
  if (!HELL_FUNCTIONS[HELL_ADD].counter) {
    return;
  }
  emit_label("add");
  emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[TMP].name),0); // set CARRY flag
  emit_label_reference("add",0);
  emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[TMP].name),+1); // clear CARRY flag
  emit_label_reference("MOVD",+1);
  emit_rotwidth_loop_begin();
  // LOOP:
  // if CARRY
  emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[TMP].name),+1);
  emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[TMP].name),0);
  emit_label_reference("no_increment_during_add",0);
  //  unset CARRY and increment ALU_DST
  emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[TMP].name),+1);
  emit_label("force_increment_during_add");
  emit_clear_var(CARRY);
  emit_opr_var(TMP); // set tmp to C0
  emit_opr_var(VAL_1222); // load 1t22..22
  emit_opr_var(CARRY); // set carry to 0t22..22
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label_reference("OPR",0);
  emit_immediate(0,"2");
  emit_label_reference("OPR",+1);
  emit_opr_var(CARRY); // set carry to 0t22..21
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_opr_var(VAL_1222); // load 0t22..22
  num_local_labels++;
  emit_label(make_string("local_label_%u",num_local_labels));
  emit_opr_var(ALU_DST); // opr dest
  emit_opr_var(TMP); // opr tmp
  emit_opr_var(CARRY); // opr carry
  emit_label_reference("LOOP2",0);
  emit_label_reference(make_string("local_label_%u",num_local_labels),0);
  num_local_labels++;
  emit_label(make_string("local_label_%u",num_local_labels));
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label_reference("OPR",0);
  emit_immediate(0,"2");
  emit_label_reference("OPR",+1); // load 0t2
  emit_opr_var(TMP); // opr tmp
  emit_label_reference("LOOP2",0);
  emit_label_reference(make_string("local_label_%u",num_local_labels),0);
  emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[TMP].name),0); // keep increment carry-flag
  emit_label_reference("keep_carry_during_add",0);
  emit_test_var(TMP); // MOVD tmp
  emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[TMP].name),+1);
  emit_label("keep_carry_during_add");
  emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[TMP].name),+1);
  emit_label("no_increment_during_add");
  // decrement ALU_SRC
  emit_clear_var(CARRY);
  emit_opr_var(TMP); // set tmp to C0
  emit_opr_var(VAL_1222); // load 1t22..22
  emit_opr_var(CARRY); // set carry to 0t22..22
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label_reference("OPR",0);
  emit_immediate(0,"2");
  emit_label_reference("OPR",+1);
  emit_opr_var(CARRY); // set carry to 1t22..21
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(ALU_SRC); // OPR src
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(ALU_SRC); //   OPR src
  emit_opr_var(TMP); //   OPR tmp
  emit_opr_var(CARRY); //   OPR carry
  emit_opr_var(ALU_SRC); //   OPR src
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(ALU_SRC); //   OPR src
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(CARRY); //   OPR carry
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label_reference("OPR",0);
  emit_immediate(0,"2");
  emit_label_reference("OPR",+1); // load 0t2
  emit_opr_var(CARRY); //   OPR carry
  emit_test_var(CARRY); // MOVD carry
  // IF NO BORROW OCURRED: increment ALU_DST
  emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[CARRY].name),0); //  GOTO force_increment if CARRY flag is set (=NO BORROW)
  emit_label_reference("force_increment_during_add",0);

  emit_rotwidth_loop_always();
  emit_rot_var(ALU_DST); // rot dest
  emit_rot_var(ALU_SRC); // rot src
  emit_rot_var(VAL_1222); // rot 1t22..22 value (22..22-tail must bé kept in operational range)
  emit_rotwidth_loop_end();

  emit_function_footer(HELL_ADD);
}



static void emit_sub_base() {
  if (!HELL_FUNCTIONS[HELL_SUB].counter) {
    return;
  }
  emit_label("sub");
  emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[TMP].name),0); // set CARRY flag
  emit_label_reference("sub",0);
  emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[TMP].name),+1); // clear CARRY flag
  emit_label_reference("MOVD",+1);
  emit_rotwidth_loop_begin();
  // LOOP:
  // if CARRY
  emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[TMP].name),+1);
  emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[TMP].name),0);
  emit_label_reference("no_decrement_during_sub",0);
  //  unset CARRY and increment ALU_DST
  emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[TMP].name),+1);
  emit_label("force_decrement_during_sub");
  // decrement ALU_DST
  emit_clear_var(CARRY);
  emit_opr_var(TMP); // set tmp to C0
  emit_opr_var(VAL_1222); // load 1t22..22
  emit_opr_var(CARRY); // set carry to 0t22..22
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label_reference("OPR",0);
  emit_immediate(0,"2");
  emit_label_reference("OPR",+1);
  emit_opr_var(CARRY); // set carry to 1t22..21
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(ALU_DST); // OPR src
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(ALU_DST); //   OPR src
  emit_opr_var(TMP); //   OPR tmp
  emit_opr_var(CARRY); //   OPR carry
  emit_opr_var(ALU_DST); //   OPR src
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(ALU_DST); //   OPR src
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(CARRY); //   OPR carry
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label_reference("OPR",0);
  emit_immediate(0,"2");
  emit_label_reference("OPR",+1); // load 0t2
  emit_opr_var(CARRY); //   OPR carry
  emit_test_var(CARRY); // MOVD carry

  emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[TMP].name),0); // keep increment carry-flag
  emit_label_reference("keep_carry_during_sub",0);
  emit_test_var(CARRY); // MOVD tmp
  emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[CARRY].name),0); // if CARRY_IS_C1 is set, TMP_IS_C1 should remain disabled
  emit_label_reference("keep_carry_during_sub",0);
  emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[TMP].name),+1);
  emit_label("keep_carry_during_sub");
  emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[TMP].name),+1);
  emit_label("no_decrement_during_sub");
  // decrement ALU_SRC
  emit_clear_var(CARRY);
  emit_opr_var(TMP); // set tmp to C0
  emit_opr_var(VAL_1222); // load 1t22..22
  emit_opr_var(CARRY); // set carry to 0t22..22
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label_reference("OPR",0);
  emit_immediate(0,"2");
  emit_label_reference("OPR",+1);
  emit_opr_var(CARRY); // set carry to 1t22..21
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(ALU_SRC); // OPR src
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(ALU_SRC); //   OPR src
  emit_opr_var(TMP); //   OPR tmp
  emit_opr_var(CARRY); //   OPR carry
  emit_opr_var(ALU_SRC); //   OPR src
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(ALU_SRC); //   OPR src
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_opr_var(VAL_1222); // load 0t22..22
  emit_opr_var(CARRY); //   OPR carry
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label_reference("OPR",0);
  emit_immediate(0,"2");
  emit_label_reference("OPR",+1); // load 0t2
  emit_opr_var(CARRY); //   OPR carry
  emit_test_var(CARRY); // MOVD carry
  // IF NO BORROW OCURRED: increment ALU_DST
  emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[CARRY].name),0); //  GOTO force_increment if CARRY flag is set (=NO BORROW)
  emit_label_reference("force_decrement_during_sub",0);

  emit_rotwidth_loop_always();
  emit_rot_var(ALU_DST); // rot dest
  emit_rot_var(ALU_SRC); // rot src
  emit_rot_var(VAL_1222); // rot 1t22..22 value (22..22-tail must bé kept in operational range)
  emit_rotwidth_loop_end();

  emit_function_footer(HELL_SUB);
}



static void emit_generate_val_1222_base() {
  if (!HELL_FUNCTIONS[HELL_GENERATE_1222].counter) {
    return;
  }
  emit_label("generate_1222");
  emit_label_reference("MOVD",+1);
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label("generate_1222_2loop");
  emit_opr_var(VAL_1222);
  emit_label_reference("LOOP2",0);
  emit_label_reference("generate_1222_2loop",0);
  emit_rotwidth_loop_begin();
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label_reference("OPR",0);
  emit_immediate(0,"2");
  emit_label_reference("OPR",+1);
  emit_opr_var(VAL_1222);

  emit_rotwidth_loop_always();
  emit_rot_var(VAL_1222); ////
  emit_rotwidth_loop_end();

  emit_function_footer(HELL_GENERATE_1222);
}



static void emit_opr_var(int var) {
  HELL_VARIABLES[var].counter++;
  emit_label_reference(make_string("VAR_RETURN%u",HELL_VARIABLES[var].counter),+1);
  emit_label_reference("MOVD",0);
  emit_label_reference(make_string("opr_%s",HELL_VARIABLES[var].name),0);
  emit_finalize_block();
  emit_label(make_string("%s_ret%u",HELL_VARIABLES[var].name,HELL_VARIABLES[var].counter));
}

static void emit_rot_var(int var) {
  HELL_VARIABLES[var].counter++;
  emit_label_reference(make_string("VAR_RETURN%u",HELL_VARIABLES[var].counter),+1);
  emit_label_reference("MOVD",0);
  emit_label_reference(make_string("rot_%s",HELL_VARIABLES[var].name),0);
  emit_finalize_block();
  emit_label(make_string("%s_ret%u",HELL_VARIABLES[var].name,HELL_VARIABLES[var].counter));
}

static void emit_test_var(int var) {
  HELL_VARIABLES[var].counter++;
  emit_label_reference(make_string("VAR_RETURN%u",HELL_VARIABLES[var].counter),+1);
  emit_label_reference("MOVD",0);
  emit_label_reference(make_string("%s",HELL_VARIABLES[var].name),0);
  emit_finalize_block();
  emit_label(make_string("%s_ret%u",HELL_VARIABLES[var].name,HELL_VARIABLES[var].counter));
}



static void emit_rotwidth_loop_begin() {
  num_rotwidth_loop_calls++;
  emit_label_reference(make_string("ROTWIDTH_LOOP%u",num_rotwidth_loop_calls),+1);
  emit_label_reference("MOVD",0);
  emit_label_reference("init_loop",0);
  emit_finalize_block();

  emit_label(make_string("rotwidth_loop_inner%u",num_rotwidth_loop_calls));
  emit_label_reference(make_string("ROTWIDTH_LOOP%u",num_rotwidth_loop_calls),+1);
}
static void emit_rotwidth_loop_always() {
  emit_label_reference("MOVD",0);
  emit_label_reference("end_of_inner_loop_body",0);
  emit_finalize_block();
  emit_label(make_string("rotwidth_loop_inner_always%u",num_rotwidth_loop_calls));
  emit_label_reference(make_string("ROTWIDTH_LOOP%u",num_rotwidth_loop_calls),+1);

}
static void emit_rotwidth_loop_end() {
  emit_label_reference("MOVD",0);
  emit_label_reference("end_of_loop_body",0);
  emit_finalize_block();

  emit_label(make_string("rotwidth_loop_ret%u",num_rotwidth_loop_calls));
}

static void emit_rotwidth_loop_base() {
  /** loop over rotwidth */
  int flag_base = num_flags;
  num_flags += 4;

  
  emit_label("opr_loop_tmp");
  emit_label_reference("MOVD",+1);
  emit_label_reference("OPR",0);

  emit_label("loop_tmp");
  emit_immediate(1,"1");
  emit_label_reference("NOP",-8); // U_NOP no_decision
  emit_label_reference("NOP",-1); // U_NOP was_C1
  emit_label_reference("NOP",-3); // U_NOP was_C10

  emit_label("was_C1");
  emit_label_reference("LOOP2",+1);
  emit_label_reference("MOVD",0);
  emit_label_reference("leave_loop",0);

  emit_label("was_C10");
  emit_label_reference("LOOP2",+1);
  emit_label_reference("MOVD",0);
  emit_label_reference("loop",0);

  emit_label("no_decision");
  emit_label_reference("OPR",+1);
  for (int i=1;i<=4;i++) {
    emit_label_reference(make_string("FLAG%u",flag_base+i),0);
    emit_label_reference(make_string("loop_tmp_ret%u",i),0);
    emit_label_reference(make_string("FLAG%u",flag_base+i),+1);
  }

  emit_finalize_block();
  emit_label("init_loop");
  // reset 20-iterations-counter
  emit_label_reference("MOVD",+1);
  emit_label_reference("LOOP5_rw",0);
  emit_label_reference("reset_loop5_successful",0);
  emit_label_reference("MOVD",0);
  emit_label_reference("init_loop",0);

  emit_label("restore_loop4");
  emit_label_reference("MOVD",+1);
  emit_label("reset_loop5_successful");
  emit_label_reference("LOOP4_rw",0);
  emit_label_reference("reset_loop4_successful",0);
  emit_label_reference("MOVD",0);
  emit_label_reference("restore_loop4",0);

  emit_label("reset_loop4_successful");
  emit_label_reference("SKIP_rw_FLAG",0);
  emit_label_reference("rw_flag_is_reset",0);
  emit_label_reference("SKIP_rw_FLAG",0);
  emit_label_reference("rw_flag_is_reset",0);

  emit_label("loop");
  emit_label_reference("MOVD",+1);
  emit_label("rw_flag_is_reset");
  emit_label_reference("SKIP_rw_FLAG",0);
  emit_label_reference("skip_inner_loop",0);
  emit_label_reference("SKIP_rw_FLAG",+1);

  /// IF 20 iterations have been executed already, skip to end_of_inner_loop_body
  for (int i=1;i<=num_rotwidth_loop_calls;i++){
    emit_label_reference(make_string("ROTWIDTH_LOOP%u",i),0);
    emit_label_reference(make_string("rotwidth_loop_inner%u",i),0);
    emit_label_reference(make_string("ROTWIDTH_LOOP%u",i),+1);
  }

  emit_finalize_block();
  emit_label("skip_inner_loop");
  emit_label_reference("SKIP_rw_FLAG",+1);
  emit_label_reference("MOVD",+1);
  emit_label("end_of_inner_loop_body");
  emit_label_reference("MOVD",+1);
  for (int i=1;i<=num_rotwidth_loop_calls;i++){
    emit_label_reference(make_string("ROTWIDTH_LOOP%u",i),0);
    emit_label_reference(make_string("rotwidth_loop_inner_always%u",i),0);
    emit_label_reference(make_string("ROTWIDTH_LOOP%u",i),+1);
  }

  emit_finalize_block();
  emit_label("end_of_loop_body");
  emit_label_reference("SKIP_rw_FLAG",0);
  emit_label_reference("set_skip_flag",0);
  emit_label_reference("SKIP_rw_FLAG",+1);
  emit_label_reference("LOOP5_rw",0);
  emit_label_reference("maybe_set_skipflag",0);
  emit_label("update_skip_flag_finished");
  emit_label_reference("MOVD",+1);

  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);
  emit_label("reset_loop_tmp_loop");
  emit_label_reference(make_string("FLAG%u",flag_base+1),+1);
  emit_label_reference("MOVD",0);
  emit_label_reference("opr_loop_tmp",0);

//
  emit_finalize_block();
  emit_label("maybe_set_skipflag");
  emit_label_reference("LOOP4_rw",0);
  emit_label_reference("set_skip_flag",0);
  emit_label_reference("MOVD",+1);
  emit_label_reference("MOVD",0);
  emit_label_reference("update_skip_flag_finished",0);
  emit_finalize_block();
  emit_label("set_skip_flag");
  emit_label_reference("SKIP_rw_FLAG",+1);
  emit_label_reference("MOVD",+1);
  emit_label_reference("MOVD",0);
  emit_label_reference("update_skip_flag_finished",0);

//

  emit_finalize_block();
  emit_label("loop_tmp_ret1");
  emit_label_reference("LOOP2",0);
  emit_label_reference("reset_loop_tmp_loop",0);

  emit_label("do_twice");
  emit_label_reference("ROT",0);
  emit_immediate(1,"1");
  emit_label_reference("ROT",+1);

  emit_label_reference("OPR",0);
  emit_immediate(0,"2");
  emit_label_reference("OPR",+1);
  emit_label_reference(make_string("FLAG%u",flag_base+2),+1);
  emit_label_reference("MOVD",0);
  emit_label_reference("opr_loop_tmp",0);
  emit_finalize_block();
  emit_label("loop_tmp_ret2");
  emit_label_reference("LOOP2",+1);
  emit_label_reference("LOOP2",0);
  emit_label_reference("loop_tmp",0);
  emit_label_reference(make_string("FLAG%u",flag_base+3),+1);
  emit_label_reference("MOVD",0);
  emit_label_reference("opr_loop_tmp",0);
  emit_finalize_block();

  emit_label("loop_tmp_ret3");
  emit_label_reference("ROT",0);
  emit_immediate(1,"2");
  emit_label_reference("ROT",+1);
  emit_label_reference(make_string("FLAG%u",flag_base+4),+1);
  emit_label_reference("MOVD",0);
  emit_label_reference("opr_loop_tmp",0);
  emit_finalize_block();

  emit_label("loop_tmp_ret4");
  emit_label_reference("LOOP2",0);
  emit_label_reference("do_twice",0);
  emit_finalize_block();

  emit_label("leave_loop");
  emit_label_reference("MOVD",+1);
  for (int i=1;i<=num_rotwidth_loop_calls;i++){
    emit_label_reference(make_string("ROTWIDTH_LOOP%u",i),0);
    emit_label_reference(make_string("rotwidth_loop_ret%u",i),0);
    emit_label_reference(make_string("ROTWIDTH_LOOP%u",i),+1);
  }

  emit_finalize_block();

  
  for (int i=1;i<=num_rotwidth_loop_calls;i++){
    emit_label(make_string("ROTWIDTH_LOOP%u",i));
    emit_xlat_cycle('o','j',0);
    emit_xlat_cycle('i',0);
    emit_finalize_block();
  }

  emit_label("LOOP4_rw");
  emit_xlat_cycle('o','o','o','j',0);
  emit_xlat_cycle('i',0);
  emit_finalize_block();

  emit_label("LOOP5_rw");
  emit_xlat_cycle('o','o','o','o','j',0);
  emit_xlat_cycle('i',0);
  emit_finalize_block();

  emit_label("SKIP_rw_FLAG");
  emit_xlat_cycle('o','j',0);
  emit_xlat_cycle('i',0);
  emit_finalize_block();

  emit_finalize_block();
  
/** END: LOOP over rotwidth */
}


static void emit_hell_variables_base() {
  int max_cnt = 0;
  for (int i=0; HELL_VARIABLES[i].name[0] != 0; i++) {
    if (!HELL_VARIABLES[i].counter) {
      // variable is not USED
      continue;
    }
    emit_label(make_string("opr_%s",HELL_VARIABLES[i].name));
    emit_label_reference("ROT",+1);
    emit_label_reference("OPR",-2); // U_OPR `HELL_VARIABLES[i].name`

    emit_label(make_string("rot_%s",HELL_VARIABLES[i].name));
    emit_label_reference("OPR",+1);
    emit_label_reference("ROT",0); // U_ROT `HELL_VARIABLES[i].name`

    emit_label(make_string("%s",HELL_VARIABLES[i].name));
    emit_immediate(0,"0"); // initialize all registers to zero

    emit_label_reference("NOP",-9); // U_NOP continue_`HELL_VARIABLES[i].name`
    emit_label_reference("NOP",-1); // U_NOP `HELL_VARIABLES[i].name`_was_c1
    emit_label_reference("NOP",-3); // U_NOP `HELL_VARIABLES[i].name`_was_c10

    emit_label(make_string("%s_was_c1",HELL_VARIABLES[i].name));
    emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[i].name),0);
    emit_label_reference(make_string("%s_was_c1",HELL_VARIABLES[i].name),0);
    emit_label_reference("NOP",-6); // U_NOP return_from_`HELL_VARIABLES[i].name`

    emit_label(make_string("%s_was_c10",HELL_VARIABLES[i].name));
    emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[i].name),0);
    emit_label_reference(make_string("%s_was_c10",HELL_VARIABLES[i].name),0);
    emit_label_reference(make_string("%s_IS_C1",HELL_VARIABLES[i].name),+1);
    emit_label_reference("NOP",-2); // U_NOP return_from_`HELL_VARIABLES[i].name`

    emit_label(make_string("continue_%s",HELL_VARIABLES[i].name)); /* U_-prefix destination */
    emit_label_reference("ROT",+1);
    emit_label_reference("OPR",+1);

    emit_label(make_string("return_from_%s",HELL_VARIABLES[i].name)); /* U_-prefix destination */
    emit_label_reference("MOVD",+1);
    for (int j=1;j<=HELL_VARIABLES[i].counter;j++) {
      emit_label_reference(make_string("VAR_RETURN%u",j),0);
      emit_label_reference(make_string("%s_ret%u",HELL_VARIABLES[i].name,j),0);
      emit_label_reference(make_string("VAR_RETURN%u",j),+1);
    }
    if (max_cnt < HELL_VARIABLES[i].counter) {
      max_cnt = HELL_VARIABLES[i].counter;
    }

    emit_finalize_block();
  }

  
  emit_finalize_block();

  for (int i=0; HELL_VARIABLES[i].name[0] != 0; i++) {
    if (!HELL_VARIABLES[i].counter) {
      // variable is not USED
      continue;
    }
    emit_label(make_string("%s_IS_C1",HELL_VARIABLES[i].name));
    emit_xlat_cycle('o','j',0);
    emit_xlat_cycle('i',0);
    emit_finalize_block();
  }
  for (int j=1;j<=max_cnt;j++) {
    emit_label(make_string("VAR_RETURN%u",j));
    emit_xlat_cycle('o','j',0);
    emit_xlat_cycle('i',0);
    emit_finalize_block();
  }
}

static void emit_branch_lookup_table() {
  
  emit_finalize_block();
  emit_offset(1,"200000"); // prevent growing of pointer too large, so that it can be computed within rotwidth of 20 (see rotwidth loop limit)
  emit_label("pc_lookup_table");
  for (int i=0;i<=current_pc_value;i++) {
    emit_label_reference("MOVD",0);
    emit_label_reference(make_string("label_pc%u",i),0);
  }
  emit_finalize_block();
}

static void finalize_hell() {

  emit_jmp_alusrc_base();
  emit_test_alu_dst_base();
  emit_putc_base();
  emit_getc_base();
  emit_add_uint24_base();
  emit_sub_uint24_base();
  emit_modulo_base();
  emit_test_neq_base();
  emit_test_eq_base();
  emit_test_ge_base();
  emit_test_lt_base();
  emit_write_memory_base();
  emit_read_memory_base();
  emit_compute_memptr_base();
  emit_memory_access_base();
  emit_add_base();
  emit_sub_base();
  emit_generate_val_1222_base();
  emit_rotwidth_loop_base();
  for (int i=0; i<TMP; i++) {
    if (modify_var_counter[HELL_VAR_TO_ALU_DST][i]) {
      emit_copy_var_aludst_base(i);
    }
    if (modify_var_counter[HELL_VAR_TO_ALU_SRC][i]) {
      emit_copy_var_alusrc_base(i);
    }
    if (modify_var_counter[HELL_VAR_READ_0t][i]) {
      emit_read_var_0t_base(i);
    }
    if (modify_var_counter[HELL_VAR_WRITE][i]) {
      emit_write_var_base(i);
    }
    if (modify_var_counter[HELL_ALU_DST_TO_VAR][i]) {
      emit_copy_aludst_to_var_base(i);
    }
  }
  emit_hell_variables_base();
  emit_branch_lookup_table();

  
  emit_finalize_block();

  emit_label("MOVD");
  emit_xlat_cycle('j','o',0);
  emit_xlat_cycle('i',0);
  emit_finalize_block();

  emit_label("ROT");
  emit_xlat_cycle('*','o',0);
  emit_xlat_cycle('i',0);
  emit_finalize_block();

  emit_label("OPR");
  emit_xlat_cycle('p','o',0);
  emit_xlat_cycle('i',0);
  emit_finalize_block();

  emit_label("IN");
  emit_xlat_cycle('/','o',0);
  emit_xlat_cycle('i',0);
  emit_finalize_block();

  emit_label("OUT");
  emit_xlat_cycle('<','o',0);
  emit_xlat_cycle('i',0);
  emit_finalize_block();

  emit_label("NOP");
  emit_xlat_cycle('i',0);
  emit_finalize_block();

  emit_label("HALT");
  emit_xlat_cycle('v',0);
  emit_finalize_block();

  emit_label("MOVDMOVD");
  emit_xlat_cycle('j','o',0);
  emit_xlat_cycle('o','o',0);
  emit_xlat_cycle('o','o',0);
  emit_xlat_cycle('o','o',0);
  emit_xlat_cycle('j','o',0);
  emit_xlat_cycle('i',0);
  emit_finalize_block();

  emit_label("LOOP2");
  emit_xlat_cycle('j','o',0);
  emit_xlat_cycle('i',0);
  emit_finalize_block();

  emit_label("LOOP4");
  emit_xlat_cycle('o','o','o','j',0);
  emit_xlat_cycle('i',0);
  emit_finalize_block();

  emit_label("LOOP2_2");
  emit_xlat_cycle('o','j',0);
  emit_xlat_cycle('i',0);
  emit_finalize_block();

  emit_label("MOVDOPRMOVD");
  emit_xlat_cycle('j','o','o','o','o','o','o','o','o',0);
  emit_xlat_cycle('o','o',0);
  emit_xlat_cycle('o','o',0);
  emit_xlat_cycle('p','o','o','o','o','*','o','o','o',0);
  emit_label("PARTIAL_MOVDOPRMOVD");
  emit_xlat_cycle('j','o','o','o','o','o','o','o','o',0);
  emit_xlat_cycle('i',0);
  emit_finalize_block();

  for (int i=1; i<=num_flags; i++) {
    emit_label(make_string("FLAG%u",i));
    emit_xlat_cycle('o','j',0);
    emit_xlat_cycle('i',0);
    emit_finalize_block();
  }
  
  int max_var = 0;
  for (int i=0; i<4; i++) {
    for (int j=0; j<8; j++) {
      if (max_var < modify_var_counter[i][j]) {
        max_var = modify_var_counter[i][j];
      }
    }
  }
  for (int i=1; i<=max_var; i++) {
    emit_label(make_string("MODIFY_VAR_RETURN%u",i));
    emit_xlat_cycle('o','j',0);
    emit_xlat_cycle('i',0);
    emit_finalize_block();
  }
  emit_flags();
}

static void dec_ternary_string(char* str) {
  size_t l = 0;
  while (str[l]) l++;
  while (l) {
    l--;
    str[l]--;
    if (str[l] < '0') {
      str[l] = '2';
    }else{
      break;
    }
  }
}

static void init_state_hell(Data* data) {
  
  emit_finalize_block();
  // memory array

  // create fixed offset of first memory cell
  char address[20];
  for (int i=0; i<13; i++) {
    address[i] = '1';
  }
  address[13] = '0';
  address[14] = '2';
  address[15] = '2';
  address[16] = '2';
  address[17] = '2';
  address[18] = '2';
  address[19] = 0;

  int mp = 0;
  for (; data; data = data->next, mp++) {
    emit_offset(1,make_string("%s",address));
    emit_label(make_string("MEMORY_%u", mp));
    char value[20];
    value[19] = 0;
    int tmp = data->v;
    int i = 19;
    do {
      i--;
      if (i==14){
        tmp++; // add 0t10000 offset (fixed cell init value)
      }
      value[i] = '0' + tmp%3;
      tmp = tmp/3;
    } while((tmp || i>14) && i);
    if (!i) {
      error("oops");
    }
    emit_immediate(0,make_string("%s",value+i));
    emit_immediate(1,"01111");
    emit_finalize_block();

    // compute offset of next memory cell
    dec_ternary_string(address);
    dec_ternary_string(address);
  }
  // the offset MEMORY_0 is needed even if no memory cells are initialized at startup
  if (!mp) {
    emit_offset(1,"022222");
    emit_label("MEMORY_0");
    emit_immediate(0,"10000");
    emit_immediate(1,"01111");
    emit_finalize_block();
  }

  emit_label("unused");
  // force rotation width to be large enough when 1t22...22-constant is generated at program start
  emit_immediate(0,"10000000000000000000");
  emit_finalize_block();
}

