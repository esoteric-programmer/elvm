#ifndef ELVM_HELLUTIL_H_
#define ELVM_HELLUTIL_H_

#include <ir/ir.h>

#define MALBOLGE_COMMAND_OPR 62
#define MALBOLGE_COMMAND_ROT 39
#define MALBOLGE_COMMAND_MOVD 40
#define MALBOLGE_COMMAND_JMP 4
#define MALBOLGE_COMMAND_IN 23
#define MALBOLGE_COMMAND_OUT 5
#define MALBOLGE_COMMAND_HALT 81
#define MALBOLGE_COMMAND_NOP 68

struct LabelTree;
struct HellBlock;

typedef struct LabelList {
	struct LabelTree* item;
	struct LabelList* next;
} LabelList;

typedef struct BlockPosition {
  struct HellBlock* father;
  int position;
} BlockPosition;

// .CODE section

typedef struct XlatCycle {
	unsigned char command; // MALBOLGE_COMMAND_...-value
	struct XlatCycle* next;
} XlatCycle;

typedef struct HellCodeAtom {
	XlatCycle* command;
	LabelList* labels;
	BlockPosition pos;
	struct HellCodeAtom* next;
} HellCodeAtom;

// .DATA section

typedef struct HellImmediate {
	int praefix_1t; // 1t... instead of 0t... value?
	const char* suffix; // string containing '0', '1', and '2'. NULL terminated.
} HellImmediate;

typedef struct HellReference {
	const char* label; // look up final value using label tree
	int offset; // add this value to reference address (value may be negative)
} HellReference;

typedef struct HellDataAtom {
	HellImmediate* value;
	HellReference* reference;
	LabelList* labels;
	BlockPosition pos;
	struct HellDataAtom* next;
} HellDataAtom;

// Label management

typedef struct HellReferencedBy {
	HellDataAtom* data;
	struct HellReferencedBy* next;
} HellReferencedBy;

typedef struct LabelTree {
	HellCodeAtom* code;
	HellDataAtom* data;
	const char* label;
	struct HellReferencedBy* referenced_by;
	struct LabelTree* left;
	struct LabelTree* right;
} LabelTree;

// Entire program

typedef struct HellBlock {
	HellImmediate* offset; // fixed offset?
	HellCodeAtom* code;
	HellDataAtom* data;
	struct HellBlock* next;
} HellBlock;

typedef struct StringList {
	char* str;
	struct StringList* next;
} StringList; // for free command

typedef struct HellProgram {
	HellBlock* blocks;
	LabelTree* labels;
	StringList* string_memory;
} HellProgram;

// helpter functions
int is_malbolge_cmd(unsigned char cmd);
LabelTree* find_label(LabelTree* tree, const char* name);

// main functions
void make_hell_object(Module* module, HellProgram** hell);
void free_hell_program(HellProgram** hell);

#endif
