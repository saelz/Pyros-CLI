#include <stdio.h>
#include <stdlib.h>

#include "pyros_cli.h"
#include "tagtree.h"

extern const char *ExecName;

static void
NewTree(PyrosTag **tag, size_t length, size_t current, TagTree *tree,
        int depth) {
	size_t i = current;
	size_t current_child = 0;

	if (i == (size_t)-1)
		i = 0;

	tree->capacity = 5;
	tree->child_count = 0;
	tree->children = malloc(sizeof(*tree) * tree->capacity);
	if (tree->children == NULL)
		goto error;

	tree->depth = depth;

	for (i = 0; i < length; i++) {
		if (tag[i]->par == current) {
			if (current_child + 1 >= tree->capacity) {
				tree->capacity *= 2;
				tree->children = realloc(
				    tree->children,
				    sizeof(*tree->children) * tree->capacity);
				if (tree->children == NULL)
					goto error;
			}
			NewTree(tag, length, i, &tree->children[current_child],
			        depth + 1);
			tree->children[current_child].tag = tag[i]->tag;
			tree->children[current_child].is_Alias =
			    tag[i]->isAlias;
			current_child++;
			tree->child_count++;
		}
	}

	return;
error:
	ERROR(stderr, "Out of memory");
	exit(1);
}

TagTree *
PyrosTagToTree(PyrosTag **tag, int length) {
	TagTree *tree = malloc(sizeof(*tree));
	if (tree == NULL) {
		ERROR(stderr, "Out of memory");
		exit(1);
	}

	NewTree(tag, length, -1, tree, 0);
	tree->tag = NULL;
	tree->is_Alias = 0;

	return tree;
}

static void
printtree(TagTree *tree, int isLastChild, int bar_depth) {

	printf("\033[36;1m");
	for (int i = 0; i < tree->depth - 1; i++) {
		if (i + 1 == tree->depth - 1) {
			if (isLastChild)
				printf("┕");
			else
				printf("┝");
		} else {
			if (i < bar_depth)
				printf("│");
			else
				printf(" ");
		}
	}

	printf("\033[0m");

	printf("%s", tree->tag);
	if (tree->is_Alias)
		printf(" \033[36;1m<A>\033[0m\n");
	else
		printf("\n");

	for (int i = 0; i < tree->child_count; i++) {
		int isLast = i + 1 == tree->child_count;

		if (!isLastChild)
			bar_depth = tree->depth - 1;

		printtree(&tree->children[i], isLast, bar_depth);
	}
}
void
PrintTree(TagTree *root) {
	for (int i = 0; i < root->child_count; i++) {
		int isLast = i + 1 == root->child_count;
		printtree(&root->children[i], isLast, 0);
	}
}

void
DestroyTree(TagTree *tree) {
	for (int i = 0; i < tree->child_count; i++)
		DestroyTree(&tree->children[i]);

	free(tree->children);

	if (tree->depth == 0)
		free(tree);
}
