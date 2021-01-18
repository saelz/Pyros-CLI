#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <pyros.h>

#include "pyros_cli.h"
#include "files.h"
#include "tagtree.h"

#define DECLARE(x) static void x(int argc, char **argv)

DECLARE(create);
DECLARE(help);
DECLARE(version);
DECLARE(add);
DECLARE(search);
DECLARE(list_hash);
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



extern char *PDB_PATH;
extern const char* ExecName;
extern int flags;
extern struct Flag gflags[];
extern size_t gflags_len;

typedef void(*foreach)(PyrosDB*, const char*, const char*);


const struct Cmd commands[] = {
	{
		"help", "h",
		&help,
		0,-1,
		0,
		"Prints help message",
		"[command]"
	},
	{
		"create", "c",
		&create,
		0,1,
		0,
		"Creates a database",
		"[md5|sha1|sha256|sha512|blake2s|blake2b]"
	},
	{
		"version", "v",
		&version ,
		0,-1
		,0,
		"Shows database Version",
		""
	},

	{
		"add", "a"
		,&add,
		1,-1,
		CMD_RECURSIVE_FLAG | CMD_INPUT_FLAG,
		"Adds file(s) to database",
		"(file | directory)... [tag]..."
	},
	{
		"add-tag" ,"at",
		&add_tag,
		2,-1,
		0,
		"Adds tag to file",
		"<hash> <tag>..."
	},
	{
		"search" ,"s" ,
		&search,
		1,-1,
		CMD_HASH_FLAG | CMD_INPUT_FLAG,
		"Searches for files by tags",
		"(tag)..."
	},
	{
		"list-hashes" ,"lh",
		&list_hash,
		0 ,0,
		0,
		"Lists all file hashes",
		""
	},
	{
		"get-alias" ,"ga",
		&get_alias,
		1 ,1,
		0,
		"Gets alias of a tag",
		"(tag)"
	},
	{
		"get-children" ,"gc",
		&get_children,
		1 ,1,
		0,
		"Gets children of a tag",
		"(tag)"
	},
	{
		"get-parents" ,"gp",
		&get_parents,
		1 ,1,
		0,
		"Gets parents of a tag",
		"(tag)"
	},
	{
		"get-tags" ,"gt",
		&get_hash ,
		1 ,1,
		0,
		"Get tags from hash",
		"(hash)"
	},
	{
		"get-related" ,"gr",
		&get_related,
		1 ,1,
		0,
		"Recursivly retrives all children and alias tags",
		"(tag)"
	},
	{
		"add-alias" ,"aa",
		&add_alias,
		2,-1,
		0,
		"Adds alias to a tag",
		"(tag) (tag)..."
	},
	{
		"add-parent" ,"ap",
		&add_parent ,
		2,-1,
		0,
		"Adds parent to tag",
		"<child_tag> <parent_tag>..."
	},
	{
		"add-child" ,"ac",
		&add_child ,
		2,-1,
		0,
		"Adds child to tag",
		"<parent_tag> <child_tag>..."
	},
	{
		"remove-relation" ,"rr",
		&remove_relationship ,
		2,-1,
		0,
		"Removes relationships between tags",
		"(tag) (tag)..."
	},
	{
		"remove-tag" ,"rt",
		&remove_tag ,
		2,-1,
		0,
		"Removes tag from file",
		"<hash> <tag>..."
	},
	{
		"remove-file" ,"rf",
		&remove_file ,
		1,-1,
		CMD_INPUT_FLAG,
		"Removes file",
		"(hash)..."
	},
	{
		"merge" ,"m" ,
		&merge,
		2,-1,
		0,
		"merges file into another one",
		"<master_hash> <merged_hash>..."
	},
	{
		"prune" ,"p" ,
		&prune_tags ,
		0, 0,
		0,
		"prunes unused tags from database",
		""
	},
};

const int command_length = LENGTH(commands);

static PyrosDB*
open_db(char *path){
	PyrosDB *pyrosDB ;
	if (Pyros_Database_Exists(path)){
		pyrosDB = Pyros_Open_Database(PDB_PATH);
	} else {
		ERROR(stderr,"database \"%s\" does not exist use create command first\n",path);
		exit(1);
	}
	return pyrosDB;
}

static void
commit(PyrosDB *pyrosDB){
	if (Pyros_Commit(pyrosDB) != PYROS_OK){
		ERROR(stderr,"an error has occurred");
		exit(1);
	}
}

static void
forEachParent(int argc, char **argv, foreach func){
	int i;
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	//eror chech
	for (i=1; i < argc; i++)
		func(pyrosDB, argv[i], argv[0]);

	commit(pyrosDB);
	Pyros_Close_Database(pyrosDB);
}

static void
forEachChild(int argc, char **argv, foreach func){
	int i;
	//eror chech
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	for (i=1; i < argc; i++)
		func(pyrosDB, argv[0], argv[i]);

	commit(pyrosDB);
	Pyros_Close_Database(pyrosDB);
}

static void
PrintFileList(PyrosList *pList){
	PyrosFile **pFile = (PyrosFile**)pList->list;
	while (*pFile){
		if (flags & CMD_HASH_FLAG)
			printf("%s\n",(*pFile)->hash);
		else
			printf("%s\n",(*pFile)->path);
		pFile++;
	}
	Pyros_List_Free(pList,(Pyros_Free_Callback)Pyros_Close_File);
}

static void
PrintList(PyrosList *pList){
	char **ptr;
	/* add some sort of error */
	if (pList == NULL)
		return;

	ptr = (char**)pList->list;
	while (*ptr){
		printf("%s\n",*ptr);
		ptr++;
	}
	Pyros_List_Free(pList,free);
}

static void
create(int argc, char **argv){
	PyrosDB *pyrosDB;
	enum PYROS_HASHTYPE hashtype = PYROS_BLAKE2BHASH;

	if (argc >= 1){
		char lower[strlen(argv[0])];
		for(size_t i = 0; argv[0][i] != '\0'; i++)
			lower[i] = tolower(argv[0][i]);

		if (!strcmp(lower, "md5")){
			hashtype = PYROS_MD5HASH;
		} else if(!strcmp(lower, "sha1")){
			hashtype = PYROS_SHA1HASH;
		} else if(!strcmp(lower, "sha256")){
			hashtype = PYROS_SHA256HASH;
		} else if(!strcmp(lower, "sha512")){
			hashtype = PYROS_SHA512HASH;
		} else if(!strcmp(lower, "blake2b")){
			hashtype = PYROS_BLAKE2BHASH;
		} else if(!strcmp(lower, "blake2s")){
			hashtype = PYROS_BLAKE2SHASH;
		} else {
			ERROR(stderr,"Unkown hash type \"%s\".\n",argv[0]);
			exit(1);
		}
	}

	if (!Pyros_Database_Exists(PDB_PATH)){
		pyrosDB = Pyros_Create_Database(PDB_PATH,hashtype);

		commit(pyrosDB);
		Pyros_Close_Database(pyrosDB);
	} else {
		ERROR(stderr,"Database \"%s\" already exists.\n",PDB_PATH);
	}
}


static void
help(int argc, char **argv){
	int i;

	UNUSED(argc);
	UNUSED(argv);

	printf("Usage: %s <command> [options]\n\n",ExecName);

	printf("COMMANDS:\n");
	for(i = 0; i < command_length; i++){
		printf("  %-15s %-4s %s\n",
			   commands[i].longName,commands[i].shortName,commands[i].description);
	}

	printf("\nGLOBAL OPTIONS:\n");
	for (size_t i = 0; i < gflags_len;i++){
		printf("  -%c --%s\t %s\t%s\n",
			   gflags[i].shortName,gflags[i].longName,
			   gflags[i].usage,gflags[i].desc);
	}
	printf("  -- \t\t\tall further arguments starting with '-' won't be treated as options\n");
	printf("\n");
	version(argc,argv);
}

static void
version(int argc, char **argv){
	UNUSED(argc);
	UNUSED(argv);

	printf("Pyros Version: %d.%d\n",PYROS_VERSION,PYROS_VERSION_MINOR);
}

static void
add(int argc, char **argv){
	PyrosList *tags = Pyros_Create_List(argc,sizeof(char*));
	PyrosList *files = Pyros_Create_List(argc,sizeof(char*));
	PyrosList *dirs = Pyros_Create_List(1,sizeof(char*));
	PyrosDB *pyrosDB = open_db(PDB_PATH);

	getFilesFromArgs(tags,files,dirs,argc,argv);
	getDirContents(files,dirs,flags & CMD_RECURSIVE_FLAG);

	if (files->length == 0){
		ERROR(stderr,"no valid files given\n");
		exit(1);
	}

	Pyros_Add_Full(pyrosDB, (char**)files->list, files->length,
				   (char**)tags->list, tags->length,
				   TRUE,FALSE, NULL, NULL);

	commit(pyrosDB);
	Pyros_List_Free(tags,NULL);
	Pyros_List_Free(files,NULL);
	Pyros_List_Free(dirs,NULL);
	Pyros_Close_Database(pyrosDB);
}

static void
add_alias(int argc, char **argv){
	forEachParent(argc,argv,&Pyros_Add_Alias);
}

static void
add_parent(int argc, char **argv){
	forEachParent(argc,argv,&Pyros_Add_Parent);
}

static void
add_child(int argc, char **argv){
	forEachChild(argc,argv,&Pyros_Add_Parent);
}

/*static void
export(int argc, char **argv){
	UNUSED(argc);
	UNUSED(argv);

}*/

static void
search(int argc, char **argv){
	PyrosDB *pyrosDB = open_db(PDB_PATH);

	PrintFileList(Pyros_Search(pyrosDB, argv, argc));
	Pyros_Close_Database(pyrosDB);
}

static void
list_hash(int argc, char **argv){
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	UNUSED(argc);
	UNUSED(argv);

	PrintList(Pyros_Get_All_Hashes(pyrosDB));
	Pyros_Close_Database(pyrosDB);
}

static void
get_alias(int argc, char **argv){
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	UNUSED(argc);

	PrintList(Pyros_Get_Aliases(pyrosDB,argv[0]));
	Pyros_Close_Database(pyrosDB);
}

static void
get_children(int argc, char **argv){
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	UNUSED(argc);

	PrintList(Pyros_Get_Children(pyrosDB,argv[0]));
	Pyros_Close_Database(pyrosDB);
}

static void
get_parents(int argc, char **argv){
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	UNUSED(argc);

	PrintList(Pyros_Get_Parents(pyrosDB,argv[0]));
	Pyros_Close_Database(pyrosDB);
}

static void
get_hash(int argc, char **argv){
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	UNUSED(argc);

	PrintList(Pyros_Get_Tags_From_Hash_Simple(pyrosDB, argv[0],TRUE));
	Pyros_Close_Database(pyrosDB);
}

static void
get_related(int argc, char **argv){
	PyrosList *tags;
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	TagTree *tree;

	UNUSED(argc);
	tags = Pyros_Get_Related_Tags(pyrosDB, argv[0],PYROS_SEARCH_RELATIONSHIP);

	tree = PyrosTagToTree((PyrosTag**)tags->list,tags->length);
	PrintTree(tree);
	DestroyTree(tree);

	Pyros_List_Free(tags,(Pyros_Free_Callback)Pyros_Free_Tag);
	Pyros_Close_Database(pyrosDB);
}

static void
merge(int argc, char **argv){
	forEachChild(argc,argv,Pyros_Merge_Hashes);
}

static void
remove_relationship(int argc, char **argv){
	forEachChild(argc,argv,Pyros_Remove_Tag_Relationship);
}

static void
remove_tag(int argc, char **argv){
	forEachChild(argc,argv,Pyros_Remove_Tag_From_Hash);
}

static void
remove_file(int argc, char **argv){
	PyrosDB *pyrosDB = open_db(PDB_PATH);

	for(int i = 0;i < argc;i++){
		PyrosFile *pFile = Pyros_Get_File_From_Hash(pyrosDB, argv[i]);
		if (pFile != NULL){
			Pyros_Remove_File(pyrosDB,pFile);
			Pyros_Close_File(pFile);
		}
	}

	commit(pyrosDB);
	Pyros_Close_Database(pyrosDB);
}

static void
prune_tags(int argc, char **argv){
	PyrosDB *pyrosDB = open_db(PDB_PATH);

	UNUSED(argc);
	UNUSED(argv);

	Pyros_Remove_Dead_Tags(pyrosDB);
	commit(pyrosDB);
	Pyros_Close_Database(pyrosDB);
}

static void
add_tag(int argc, char **argv){
	PyrosDB *pyrosDB = open_db(PDB_PATH);

	Pyros_Add_Tag(pyrosDB,argv[0], &argv[1], argc-1);

	commit(pyrosDB);
	Pyros_Close_Database(pyrosDB);
}
