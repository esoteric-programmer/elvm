#include <stdarg.h>

#include "hellutil.h"

// Helper functions for HeLL program generation

typedef struct LabelList {
	LabelTree* item;
	LabelList* next;
} LabelList;

static HellProgram* current_program;
static HellBlock* current_block;
static HellCodeAtom* current_code;
static HellDataAtom* current_data;
static HellImmediate* fixed_offset;
static LabelList* current_labels;
static LabelList* current_labels_tail;

static int is_malbolge_cmd(unsigned char cmd);
static void emit_code_atom(XlatCycle* cyc);
static void emit_data_atom(HellImmediate* hi, HellReference* hr);
static void emit_hell_block(HellCodeAtom* code, HellDataAtom* data);
static void free_labeltree(LabelTree* tree);
static void free_xlat(XlatCycle* cyc);
static void	free_code(HellCodeAtom* code);
static void	free_data(HellDataAtom* data);


// call the following functions to generate HeLL program

static void emit_xlat_cycle(unsigned char command, ...); // terminated by invalid command, e.g. 0
static void emit_immediate(int praefix_1t, const char* suffix);
static void emit_label_reference(const char* label, int offset);
static void emit_unused_cell();
static void emit_finalize_block();
static void emit_offset(int praefix_1t, const char* suffix); // assign fixed offset to next code/data atom
static void emit_label(const char* name);

static int is_malbolge_cmd(unsigned char cmd) {
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

static void emit_xlat_cycle(unsigned char command, ...) {
    va_list ap;
    if (!is_malbolge_cmd(command)) {
    	error("oops");
    }
    XlatCycle* cyc = (XlatCycle*)malloc(sizeof(XlatCycle));
    if (!cyc) {
    	error("out of mem");
    }
	cyc->command = command;
	cyc->next = NULL;
	XlatCycle* it = cyc;

    va_start(ap, command);
    while (1) {
    	cmd = va_arg(ap, unsigned char);
    	if (!is_malbolge_cmd(cmd)) {
    		break;
    	}
    	it->next = (XlatCycle*)malloc(sizeof(XlatCycle));
    	if (!it->next) {
    		error("out of mem");
	    }
	    it = it->next;
        it->command = cmd;
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
	atom->referenced_by = NULL;
	atom->next = NULL;
	LabelList* it = current_labels;
	while (it) {
		it->item->code = atom;
		it = it->next;
		free(current_labels);
		current_labels = it;
	}
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
	atom->referenced_by = NULL;
	atom->next = NULL;
	LabelList* it = current_labels;
	while (it) {
		it->item->data = atom;
		it = it->next;
		free(current_labels);
		current_labels = it;
	}
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
			int cmp = strncmp(label,it->label,101);
			if (cmp > 0){
				if (it->left) {
					it = it->left;
				}else{
					it->left = label;
					break;
				}
			}else if (cmp < 0){
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
	LabelList* list = (LabelList*)malloc(LabelList);
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

// Generation of HeLL program from Module

void make_hell_object(Module* module, HellProgram** hell) {
	current_program = NULL;
	current_block = NULL;
	current_code = NULL;
	current_data = NULL;
	fixed_offset = NULL;
	current_labels = NULL;
	current_labels_tail = NULL;
	// TODO: emit HeLL code for module
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
		free_xlat(tmp->xlat);
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

void free_hell_program(HellProgram** hell) {
	if (!hell) {
		return;
	}
	if (!*hell) {
		return;
	}
	// free all memory
	free_labeltree((*hell)->labels);
	HellBlock* it = (*hell)->blocks;
	while (it) {
		HellBlock* tmp;
		it = it->next;
		// clear tmp
		free_code(tmp->code);
		free_data(tmp->data);
		free(tmp);
	}
	free(*hell);
	*hell = NULL;
}
