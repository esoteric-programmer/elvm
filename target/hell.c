#include <target/util.h>
#include <target/hellutil.h>

static int last_section_type = 0;

static void print_labels(HellReferencedBy* ref);
static void print_malbolge_command(unsigned char cmd);
static void print_hell_code(HellCodeAtom* code);
static void print_hell_data(HellDataAtom* data);
static void print_hell_program(HellProgram* hell);

void target_hell(Module* module) {
  HellProgram* hp = NULL;
  make_hell_object(module, &hp);
  print_hell_program(hp);
  free_hell_program(&hp);
}


static void print_hell_program(HellProgram* hell) {
  if (!hell) {
    return;
  }

  // TODO: fill referenced_by list

  last_section_type = 0;
  HellBlock* it = hell->blocks;
  while (it) {
    if (it->code && it->data) {
      error("oops");
    }
    if (it->code) {
      print_hell_code(it->code);
    }
    if (it->data) {
      print_hell_data(it->data);
    }
    it = it->next;
  }
}

static void print_labels(HellReferencedBy* ref) {
	// TODO
	if (ref) {
	  // TODO
	}
}

static void print_malbolge_command(unsigned char cmd) {
  switch (cmd) {
    case MALBOLGE_COMMAND_OPR:
      printf("Opr");
      break;
    case MALBOLGE_COMMAND_ROT:
      printf("Rot");
      break;
    case MALBOLGE_COMMAND_MOVD:
      printf("MovD");
      break;
    case MALBOLGE_COMMAND_JMP:
      printf("Jmp");
      break;
    case MALBOLGE_COMMAND_IN:
      printf("In");
      break;
    case MALBOLGE_COMMAND_OUT:
      printf("Out");
      break;
    case MALBOLGE_COMMAND_HALT:
      printf("Hlt");
      break;
    case MALBOLGE_COMMAND_NOP:
      printf("Nop");
      break;
    default:
      error("oops");
  }
}


static void print_hell_code(HellCodeAtom* code) {
  if (!code) {
    return;
  }
  if (last_section_type != 1) {
    printf(".CODE\n");
  }
  last_section_type = 1;
  HellCodeAtom* it = code;
  while (it) {
    if (!it->command) {
      error("oops");
    }
    print_labels(it->referenced_by);
    XlatCycle* cyc = it->command;
    int is_rnop = (cyc->command == MALBOLGE_COMMAND_NOP && cyc->next != NULL)?1:0;
    while (cyc->next && is_rnop) {
      cyc = cyc->next;
      if (cyc->command != MALBOLGE_COMMAND_NOP) {
        is_rnop = 0;
      }
    }
    if (is_rnop) {
      printf("  RNop\n");
    }else{
      cyc = it->command;
      printf("  ");
      while (cyc) {
        print_malbolge_command(cyc->command);
        cyc = cyc->next;
        if (cyc) {
          printf("/");
        }
      }
      printf("\n");
    }
    it = it->next;
  }
  printf("\n");
}

static void print_hell_data(HellDataAtom* data) {
  if (!data) {
    return;
  }
  if (last_section_type != 2) {
    printf(".DATA\n");
  }
  last_section_type = 2;
  HellDataAtom* it = data;
  while (it) {
    print_labels(it->referenced_by);
    if (it->value && it->reference) {
      error("oops");
    }else if (!it->value && !it->reference) {
      printf("  ?\n");
    }else if (it->value) {
      if (!it->value->suffix) {
        error("oops");
      }
      if (!it->value->suffix[0]) {
        printf("  %ct%c\n",'0'+it->value->praefix_1t,'0'+it->value->praefix_1t);
      }else{
        printf("  %ct%s\n",'0'+it->value->praefix_1t,it->value->suffix);
      }
    }else if (it->reference) {
      printf("  ");
      if (it->reference->offset == +1) {
        printf("R_");
      }else if (it->reference->offset < 0) {
        printf("U_");
        // TODO: MAKE U_ prefix!!!!!!!!
      }else if (it->reference->offset != 0) {
        error("oops");
      }
      printf("%s\n",it->reference->label);
    }
    it = it->next;
  }
  printf("\n");
}
