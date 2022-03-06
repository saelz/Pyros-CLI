#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <pyros.h>

#include "files.h"
#include "pyros_cli.h"
#include "tagtree.h"

#define DECLARE(x) static void x(int argc, char **argv)
#define SHOW_ERROR_AND_EXIT                                                    \
	ERROR(stderr, "%s\n", Pyros_Get_Error_Message(pyrosDB));               \
	exit(1);
#define CHECK_ERROR(x)                                                         \
	if (x != PYROS_OK) {                                                   \
		Pyros_Rollback(pyrosDB);                                       \
		SHOW_ERROR_AND_EXIT;                                           \
	}

DECLARE(create);
DECLARE(help);
DECLARE(version);
DECLARE(add);
DECLARE(search);
DECLARE(list_hash);
DECLARE(list_tags);
DECLARE(get_alias);
DECLARE(get_children);
DECLARE(get_parents);
DECLARE(get_hash);
DECLARE(get_related);
DECLARE(add_alias);
DECLARE(add_parent);
DECLARE(add_child);
DECLARE(remove_relationship);
DECLARE(remove_tag);
DECLARE(remove_file);
DECLARE(merge);
DECLARE(prune_tags);
DECLARE(add_tag);
DECLARE(vacuum);
DECLARE(export);

extern char *PDB_PATH;
extern const char *ExecName;
extern int flags;
extern struct Flag gflags[];
extern size_t gflags_len;

typedef enum PYROS_ERROR (*foreach)(PyrosDB *, const char *, const char *);

/* clang-format off */
const struct Cmd commands[] = {
	{
		"help", "h",
		&help,
		0,-1,
		0,
		"Print help message",
		"[command]"
	},
	{
		"create", "c",
		&create,
		0,1,
		0,
		"Create a new database",
		"[md5|sha1|sha256|sha512|blake2s|blake2b]"
	},
	{
		"version", "v",
		&version ,
		0,-1
		,0,
		"Show Pyros version",
		""
	},

	{
		"add", "a"
		,&add,
		1,-1,
		CMD_RECURSIVE_FLAG | CMD_INPUT_FLAG | CMD_PROGRESS_FLAG,
		"Add file(s) to database",
		"(file | directory)... [tag]..."
	},
	{
		"add-tag" ,"at",
		&add_tag,
		2,-1,
		0,
		"Add tag(s) to file",
		"<hash> <tag>..."
	},
	{
		"search" ,"s" ,
		&search,
		1,-1,
		CMD_HASH_FLAG | CMD_INPUT_FLAG,
		"Search for files by tags",
		"(tag)..."
	},
	{
		"list-hashes" ,"lh",
		&list_hash,
		0 ,0,
		0,
		"List all file hashes",
		""
	},
	{
		"list-tags" ,"lt",
		&list_tags,
		0 ,0,
		0,
		"List all tags",
		""
	},
	{
		"get-alias" ,"ga",
		&get_alias,
		1 ,1,
		0,
		"List aliases of a tag",
		"(tag)"
	},
	{
		"get-children" ,"gc",
		&get_children,
		1 ,1,
		0,
		"List children of a tag",
		"(tag)"
	},
	{
		"get-parents" ,"gp",
		&get_parents,
		1 ,1,
		0,
		"List parents of a tag",
		"(tag)"
	},
	{
		"get-tags" ,"gt",
		&get_hash ,
		1 ,1,
		0,
		"List tags associated with a file",
		"(hash)"
	},
	{
		"get-related" ,"gr",
		&get_related,
		1 ,1,
		0,
		"Recursivly list all children and aliases of a tag",
		"(tag)"
	},
	{
		"add-alias" ,"aa",
		&add_alias,
		2,-1,
		0,
		"Add alias to a tag",
		"(tag) (tag)..."
	},
	{
		"add-parent" ,"ap",
		&add_parent ,
		2,-1,
		0,
		"Add parent to a tag",
		"<child_tag> <parent_tag>..."
	},
	{
		"add-child" ,"ac",
		&add_child ,
		2,-1,
		0,
		"Add child to a tag",
		"<parent_tag> <child_tag>..."
	},
	{
		"remove-relation" ,"rr",
		&remove_relationship ,
		2,-1,
		0,
		"Remove relationship between tags",
		"(tag) (tag)..."
	},
	{
		"remove-tag" ,"rt",
		&remove_tag ,
		2,-1,
		0,
		"Remove tag(s) from file",
		"<hash> <tag>..."
	},
	{
		"remove-file" ,"rf",
		&remove_file ,
		1,-1,
		CMD_INPUT_FLAG,
		"Remove file(s) from database",
		"(hash)..."
	},
	{
		"merge" ,"m" ,
		&merge,
		2,-1,
		0,
		"Merge files together",
		"<master_hash> <merged_hash>..."
	},
	{
		"prune" ,"p" ,
		&prune_tags ,
		0, 0,
		0,
		"Prune unused tags from database",
		""
	},
	{
		"vacuum" ,"vc" ,
		&vacuum ,
		0, 0,
		0,
		"Vacuum the database",
		""
	},
	{
		"export" ,"ex" ,
		&export ,
		2, -1,
		0,
		"Copy files from the database to specified directory",
		"<output_dir> <tags>..."
	},
};
/* clang-format on */

const int command_length = LENGTH(commands);

static PyrosDB *
open_db(char *path) {
	PyrosDB *pyrosDB;
	if (!Pyros_Database_Exists(path)) {
		ERROR(
		    stderr,
		    "database \"%s\" does not exist use create command first\n",
		    path);
		exit(1);
	}

	if ((pyrosDB = Pyros_Alloc_Database(path)) == NULL) {
		ERROR(stderr, "Out of memory\n");
		exit(1);
	}

	if (Pyros_Open_Database(pyrosDB) != PYROS_OK) {
		ERROR(stderr, "Unable to open database: %s\n",
		      Pyros_Get_Error_Message(pyrosDB));
		exit(1);
	}
	return pyrosDB;
}

static void
commit(PyrosDB *pyrosDB) {
	CHECK_ERROR(Pyros_Commit(pyrosDB))
}
static void
close_db(PyrosDB *pyrosDB) {
	CHECK_ERROR(Pyros_Close_Database(pyrosDB))
}
static void
forEachParent(int argc, char **argv, foreach func) {
	int i;
	PyrosDB *pyrosDB = open_db(PDB_PATH);

	for (i = 1; i < argc; i++) {
		CHECK_ERROR(func(pyrosDB, argv[i], argv[0]));
	}

	commit(pyrosDB);
	close_db(pyrosDB);
}

static void
forEachChild(int argc, char **argv, foreach func) {
	int i;
	PyrosDB *pyrosDB = open_db(PDB_PATH);

	for (i = 1; i < argc; i++) {
		CHECK_ERROR(func(pyrosDB, argv[0], argv[i]));
	}

	commit(pyrosDB);
	close_db(pyrosDB);
}

static void
PrintFileList(PyrosList *pList) {
	PyrosFile **pFile = (PyrosFile **)pList->list;
	while (*pFile) {
		if (flags & CMD_HASH_FLAG)
			printf("%s\n", (*pFile)->hash);
		else
			printf("%s\n", (*pFile)->path);
		pFile++;
	}
	Pyros_List_Free(pList, (Pyros_Free_Callback)Pyros_Free_File);
}

static void
PrintList(PyrosList *pList) {
	char **ptr;
	/* add some sort of error */
	if (pList == NULL)
		return;

	ptr = (char **)pList->list;
	while (*ptr) {
		printf("%s\n", *ptr);
		ptr++;
	}
	Pyros_List_Free(pList, free);
}

static void
create(int argc, char **argv) {
	PyrosDB *pyrosDB;
	enum PYROS_HASHTYPE hashtype = PYROS_BLAKE2BHASH;

	if (argc >= 1) {
		char lower[strlen(argv[0])];
		for (size_t i = 0; argv[0][i] != '\0'; i++)
			lower[i] = tolower(argv[0][i]);

		if (!strcmp(lower, "md5")) {
			hashtype = PYROS_MD5HASH;
		} else if (!strcmp(lower, "sha1")) {
			hashtype = PYROS_SHA1HASH;
		} else if (!strcmp(lower, "sha256")) {
			hashtype = PYROS_SHA256HASH;
		} else if (!strcmp(lower, "sha512")) {
			hashtype = PYROS_SHA512HASH;
		} else if (!strcmp(lower, "blake2b")) {
			hashtype = PYROS_BLAKE2BHASH;
		} else if (!strcmp(lower, "blake2s")) {
			hashtype = PYROS_BLAKE2SHASH;
		} else {
			ERROR(stderr, "Unkown hash type \"%s\".\n", argv[0]);
			exit(1);
		}
	}

	if (!Pyros_Database_Exists(PDB_PATH)) {
		pyrosDB = Pyros_Alloc_Database(PDB_PATH);
		if (pyrosDB == NULL) {
			ERROR(stderr, "Out of memory");
			exit(1);
		}

		CHECK_ERROR(Pyros_Create_Database(pyrosDB, hashtype));

		commit(pyrosDB);
		close_db(pyrosDB);
	} else {
		ERROR(stderr, "Database \"%s\" already exists.\n", PDB_PATH);
	}
}

static void
help(int argc, char **argv) {
	int i;

	UNUSED(argc);
	UNUSED(argv);

	printf("Usage: %s <command> [options]\n\n", ExecName);

	printf("COMMANDS:\n");
	for (i = 0; i < command_length; i++) {
		printf("  %-15s %-4s %s\n", commands[i].longName,
		       commands[i].shortName, commands[i].description);
	}

	printf("\nGLOBAL OPTIONS:\n");
	for (size_t i = 0; i < gflags_len; i++) {
		printf("  -%c --%s\t %s\t%s\n", gflags[i].shortName,
		       gflags[i].longName, gflags[i].usage, gflags[i].desc);
	}
	printf("  -- \t\t\tall further arguments starting with '-' won't be "
	       "treated as options\n");
	printf("\n");
	version(argc, argv);
}

static void
version(int argc, char **argv) {
	UNUSED(argc);
	UNUSED(argv);

	printf("Pyros Version: %d.%d\n", PYROS_VERSION, PYROS_VERSION_MINOR);
}

static void
add_progress_cb(const char *hash, const char *file, size_t position,
                void *data) {
	struct winsize w;
	size_t len = *(size_t *)data;
	double progress = position / (double)len;
	double prog_bar_size;

	UNUSED(hash);
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	prog_bar_size = w.ws_col / 2.0;

	for (int i = 0; i < w.ws_col; i++)
		putchar(' ');

	printf("\r%s\n[", file);
	for (int i = 0; i < prog_bar_size; i++) {
		if (progress >= i / prog_bar_size)
			putchar('*');
		else
			putchar(' ');
	}
	printf("] %d%%\r", (int)(progress * 100));
	fflush(stdout);
}

static void
add(int argc, char **argv) {
	PyrosList *tags = Pyros_Create_List(argc);
	PyrosList *files = Pyros_Create_List(argc);
	PyrosList *dirs = Pyros_Create_List(1);
	PyrosDB *pyrosDB = open_db(PDB_PATH);

	if (tags == NULL || files == NULL || dirs == NULL) {
		ERROR(stderr, "Out of memory\n");
		exit(1);
	}

	getFilesFromArgs(tags, files, dirs, argc, argv);
	getDirContents(files, dirs, flags & CMD_RECURSIVE_FLAG);

	if (files->length == 0) {
		ERROR(stderr, "no valid files given\n");
		exit(1);
	}

	if (flags & CMD_PROGRESS_FLAG) {
		CHECK_ERROR(Pyros_Add_Full(
		    pyrosDB, (const char **)files->list, files->length,
		    (const char **)tags->list, tags->length, TRUE, FALSE,
		    &add_progress_cb, &files->length));
	} else {
		CHECK_ERROR(
		    Pyros_Add_Full(pyrosDB, (const char **)files->list,
		                   files->length, (const char **)tags->list,
		                   tags->length, TRUE, FALSE, NULL, NULL));
	}

	commit(pyrosDB);
	Pyros_List_Free(tags, NULL);
	Pyros_List_Free(files, NULL);
	Pyros_List_Free(dirs, NULL);
	close_db(pyrosDB);
}

static void
add_alias(int argc, char **argv) {
	forEachParent(argc, argv, &Pyros_Add_Alias);
}

static void
add_parent(int argc, char **argv) {
	forEachParent(argc, argv, &Pyros_Add_Parent);
}

static void
add_child(int argc, char **argv) {
	forEachChild(argc, argv, &Pyros_Add_Parent);
}

static void
search(int argc, char **argv) {
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	PyrosList *files;

	files = Pyros_Search(pyrosDB, (const char **)argv, argc);
	if (files == NULL) {
		CHECK_ERROR(Pyros_Get_Error_Type(pyrosDB));
	}

	PrintFileList(files);
	close_db(pyrosDB);
}

static void
list_hash(int argc, char **argv) {
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	PyrosList *list;
	UNUSED(argc);
	UNUSED(argv);

	list = Pyros_Get_All_Hashes(pyrosDB);
	if (list == NULL) {
		CHECK_ERROR(Pyros_Get_Error_Type(pyrosDB));
	}

	PrintList(list);
	close_db(pyrosDB);
}

static void
list_tags(int argc, char **argv) {
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	PyrosList *list;
	UNUSED(argc);
	UNUSED(argv);

	list = Pyros_Get_All_Tags(pyrosDB);
	if (list == NULL) {
		CHECK_ERROR(Pyros_Get_Error_Type(pyrosDB));
	}

	PrintList(list);
	close_db(pyrosDB);
}

static void
get_alias(int argc, char **argv) {
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	PyrosList *list;
	UNUSED(argc);

	list = Pyros_Get_Aliases(pyrosDB, argv[0]);
	if (list == NULL) {
		CHECK_ERROR(Pyros_Get_Error_Type(pyrosDB));
	}

	PrintList(list);
	close_db(pyrosDB);
}

static void
get_children(int argc, char **argv) {
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	PyrosList *list;
	UNUSED(argc);

	list = Pyros_Get_Children(pyrosDB, argv[0]);
	if (list == NULL) {
		CHECK_ERROR(Pyros_Get_Error_Type(pyrosDB));
	}

	PrintList(list);
	close_db(pyrosDB);
}

static void
get_parents(int argc, char **argv) {
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	PyrosList *list;
	UNUSED(argc);

	list = Pyros_Get_Parents(pyrosDB, argv[0]);
	if (list == NULL) {
		CHECK_ERROR(Pyros_Get_Error_Type(pyrosDB));
	}

	PrintList(list);
	close_db(pyrosDB);
}

static void
get_hash(int argc, char **argv) {
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	PyrosList *tags;
	UNUSED(argc);

	tags = Pyros_Get_Tags_From_Hash_Simple(pyrosDB, argv[0], TRUE);
	if (tags == NULL) {
		CHECK_ERROR(Pyros_Get_Error_Type(pyrosDB));
	}

	PrintList(tags);
	close_db(pyrosDB);
}

static void
get_related(int argc, char **argv) {
	PyrosList *tags;
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	TagTree *tree;

	UNUSED(argc);
	tags =
	    Pyros_Get_Related_Tags(pyrosDB, argv[0], PYROS_SEARCH_RELATIONSHIP);

	CHECK_ERROR(Pyros_Get_Error_Type(pyrosDB));

	tree = PyrosTagToTree((PyrosTag **)tags->list, tags->length);
	PrintTree(tree);
	DestroyTree(tree);

	Pyros_List_Free(tags, (Pyros_Free_Callback)Pyros_Free_Tag);
	close_db(pyrosDB);
}

static void
merge(int argc, char **argv) {
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	for (int i = 1; i < argc; i++) {
		CHECK_ERROR(
		    Pyros_Merge_Hashes(pyrosDB, argv[0], argv[i], TRUE));
	}

	commit(pyrosDB);
	close_db(pyrosDB);
}

static void
remove_relationship(int argc, char **argv) {
	forEachChild(argc, argv, Pyros_Remove_Tag_Relationship);
}

static void
remove_tag(int argc, char **argv) {
	forEachChild(argc, argv, Pyros_Remove_Tag_From_Hash);
}

static void
remove_file(int argc, char **argv) {
	PyrosDB *pyrosDB = open_db(PDB_PATH);

	for (int i = 0; i < argc; i++) {
		PyrosFile *pFile = Pyros_Get_File_From_Hash(pyrosDB, argv[i]);
		if (pFile != NULL) {
			CHECK_ERROR(Pyros_Remove_File(pyrosDB, pFile));
			Pyros_Free_File(pFile);
		} else if (Pyros_Get_Error_Type(pyrosDB) != PYROS_OK) {
			SHOW_ERROR_AND_EXIT;
		}
	}

	commit(pyrosDB);
	close_db(pyrosDB);
}

static void
prune_tags(int argc, char **argv) {
	PyrosDB *pyrosDB = open_db(PDB_PATH);

	UNUSED(argc);
	UNUSED(argv);

	CHECK_ERROR(Pyros_Remove_Dead_Tags(pyrosDB))

	commit(pyrosDB);

	close_db(pyrosDB);
}

static void
add_tag(int argc, char **argv) {
	PyrosDB *pyrosDB = open_db(PDB_PATH);

	CHECK_ERROR(
	    Pyros_Add_Tag(pyrosDB, argv[0], (const char **)&argv[1], argc - 1))

	commit(pyrosDB);
	close_db(pyrosDB);
}

static void
vacuum(int argc, char **argv) {
	PyrosDB *pyrosDB = open_db(PDB_PATH);

	UNUSED(argc);
	UNUSED(argv);

	CHECK_ERROR(Pyros_Vacuum_Database(pyrosDB));

	close_db(pyrosDB);
}

static void export(int argc, char **argv) {
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	PyrosList *files =
	    Pyros_Search(pyrosDB, (const char **)argv + 1, argc - 1);
	PyrosFile *file;
	PyrosList *tags;
	char *dest_path = NULL;
	size_t i;

	if (files != NULL && files->length > 0) {
		if (!pathExists(argv[0])) {
			ERROR(stderr, "%s does not exist\n", argv[0]);
			goto end;
		} else if (isFile(argv[0])) {
			ERROR(stderr, "%s is a file not a directory\n",
			      argv[0]);
			goto end;
		}

		for (i = 0; i < files->length; i++) {
			file = files->list[i];
			dest_path =
			    malloc(strlen(argv[0]) + strlen(file->hash) +
			           strlen(file->ext) + 6);
			if (dest_path == NULL) {
				ERROR(stderr, "Out of memory");
				exit(1);
			}

			strcpy(dest_path, argv[0]);
			strcat(dest_path, "/");
			strcat(dest_path, file->hash);
			strcat(dest_path, ".");
			strcat(dest_path, file->ext);
			printf("%s -> %s\n", file->hash, dest_path);
			cp(file->path, dest_path);

			tags = Pyros_Get_Tags_From_Hash_Simple(
			    pyrosDB, file->hash, FALSE);

			CHECK_ERROR(Pyros_Get_Error_Type(pyrosDB));

			strcat(dest_path, ".txt");
			if (tags != NULL)
				writeListToFile(tags, dest_path);

			Pyros_List_Free(tags, free);
			free(dest_path);
		}
	}

end:
	Pyros_List_Free(files, (Pyros_Free_Callback)Pyros_Free_File);
	Pyros_Close_Database(pyrosDB);
}
