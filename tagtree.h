#ifndef TAG_TREE_H
#define TAG_TREE_H

#include "pyros.h"

typedef struct TagTree{
	struct TagTree *children;
	int child_count;
	size_t capacity;
	char *tag;
	int is_Alias;
	int depth;
} TagTree;

TagTree *PyrosTagToTree(PyrosTag **tag,int length);

void PrintTree(TagTree *tree);
void DestroyTree(TagTree *tree);

#endif
