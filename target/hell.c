#include <target/util.h>
#include <target/hellutil.h>

static int last_section_type = 0;

static void print_labels(LabelList* labels);
static void print_offset(HellImmediate* offset);
static void print_malbolge_command(unsigned char cmd);
static void print_hell_code(HellCodeAtom* code, HellImmediate* offset);
static void print_hell_data(HellDataAtom* data, HellImmediate* offset, LabelTree* tree);
static void print_hell_program(HellProgram* hell);

void target_hell(Module* module) {
  HellProgram* hp = NULL;
  make_hell_object(module, &hp);
  if (!hp) {
    error("oops");
  }
  print_hell_program(hp);
  free_hell_program(&hp);
}


static void print_hell_program(HellProgram* hell) {
  if (!hell) {
    return;
  }
  last_section_type = 0;
  HellBlock* it = hell->blocks;
  while (it) {
    if (it->code && it->data) {
      error("oops");
    }
    if (it->code) {
      print_hell_code(it->code, it->offset);
    }
    if (it->data) {
      print_hell_data(it->data, it->offset, hell->labels);
    }
    it = it->next;
  }
}

static void print_offset(HellImmediate* offset) {
  if (offset) {
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
  }
}

static void print_labels(LabelList* labels) {
  LabelList* it = labels;
  while (it) {
    if (it->item) {
      if (it->item->label) {
        printf("%s:\n",it->item->label);
      }
    }
    it = it->next;
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


static void print_hell_code(HellCodeAtom* code, HellImmediate* offset) {
  if (!code) {
    return;
  }
  if (last_section_type != 1) {
    printf(".CODE\n");
  }
  last_section_type = 1;
  print_offset(offset);
  HellCodeAtom* it = code;
  while (it) {
    if (!it->command) {
      error("oops");
    }
    print_labels(it->labels);
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

static void print_hell_data(HellDataAtom* data, HellImmediate* offset, LabelTree* tree) {
  if (!data) {
    return;
  }
  if (last_section_type != 2) {
    printf(".DATA\n");
  }
  last_section_type = 2;
  print_offset(offset);
  HellDataAtom* it = data;
  while (it) {
    print_labels(it->labels);
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
      LabelTree* dest = find_label(tree, it->reference->label);
      if (!dest) {
        error("oops");
      }
      if (dest->data && !dest->code) {
        printf("%s",it->reference->label);
        // if (it->reference->offset > 0) -- hack for 8cc
        if (it->reference->offset > 0 && (unsigned int)it->reference->offset < ((unsigned int)-1)/2) {
          printf(" + %u",it->reference->offset);
        // else if (it->reference->offset < 0) -- hack for 8cc
        }else if ((unsigned int)it->reference->offset >= ((unsigned int)-1)/2) {
          printf(" - %u",-it->reference->offset);
        }
        printf("\n");
      }else if (dest->code && !dest->data) {
        // if (it->reference->offset > 0) -- hack for 8cc
        if (it->reference->offset == +1) {
          printf("R_%s\n",it->reference->label);
        // else if (it->reference->offset < 0) -- hack for 8cc
        }else if ((unsigned int)it->reference->offset >= ((unsigned int)-1)/2) {
          printf("U_%s ",it->reference->label);
          HellDataAtom* dest_u = it->next;
          for (int i=0; i<-it->reference->offset && dest_u; i++) {
            dest_u = dest_u->next;
          }
          if (!dest_u) {
            error("oops");
          }
          if (dest_u->labels) {
            if (dest_u->labels->item) {
              if (dest_u->labels->item->label) {
                printf("%s\n",dest_u->labels->item->label);
              }else{
                error("oops");
              }
            }else{
              error("oops");
            }
          }else{
            error("oops");
          }
        }else if (it->reference->offset == 0) {
          printf("%s\n",it->reference->label);
        }else{
           error("oops");
        }
      }else{
        error("oops");
      }
    }
    it = it->next;
  }
  printf("\n");
}
