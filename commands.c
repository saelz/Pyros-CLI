#include <stdio.h>
#include <stdlib.h>

#include <pyros.h>

#include "pyros_cli.h"
#include "files.h"

#define DECLARE(x) static void x(int argc, char **argv)

DECLARE(create);
DECLARE(help);
DECLARE(version);
DECLARE(add);
DECLARE(search);
DECLARE(list_hash);
DECLARE(get_alias);
DECLARE(get_children);
DECLARE(get_hash);
DECLARE(get_ext);
DECLARE(add_alias);
DECLARE(add_parent);
DECLARE(add_child);
DECLARE(remove_ext);
DECLARE(remove_tag);
DECLARE(merge);
/*DECLARE(export);*/
DECLARE(prune_tags);
DECLARE(add_tag);



extern char *PDB_PATH;
extern const char* ExecName;
extern int flags;

typedef void(*foreach)(PyrosDB*, const char*, const char*);


const struct Cmd commands[] = {
	{
		"help", "h",
		&help,
		0,-1,
		0,
		"Prints help message"
	},
	{
		"create", "c",
		&create,
		0,-1,
		0,
		"Creates a database"
	},
	{
		"version", "v",
		&version ,
		0,-1
		,0,
		"Shows database Version"
	},

	{
		"add", "a"
		,&add,
		1,-1,
		0,
		"Adds file to database"
	},
	{
		"add-tag" ,"at",
		&add_tag,
		2,-1,
		0,
		"Adds tag to file"
	},
	{
		"search" ,"s" ,
		&search,
		1,-1,
		CMD_HASH_FLAG,
		"Searches for files by tags"
	},
	{
		"list-hashes" ,"lh",
		&list_hash,
		0 ,0,
		0,
		"Lists all file hashes"
	},
	{
		"get-alias" ,"ga",
		&get_alias,
		1 ,1,
		0,
		"Gets alias of a tag"
	},
	{
		"get-children" ,"gc",
		&get_children,
		1 ,1,
		0,
		"Gets children of a tag"
	},
	{
		"get-tags" ,"gt",
		&get_hash ,
		1 ,1,
		0,
		"Get tags from hash"
	},
	{
		"get-ext" ,"ge",
		&get_ext,
		1 ,1,
		0,
		"Gets all extended tags related to a given tag"
	},
	{
		"add-alias" ,"aa",
		&add_alias,
		2,-1,
		0,
		"Adds alias to a tag"
	},
	{
		"add-parent" ,"ap",
		&add_parent ,
		2,-1,
		0,
		"Adds parent to tag"
	},
	{
		"add-child" ,"ac",
		&add_child ,
		2,-1,
		0,
	 "Adds child to tag"
	},
	{
		"remove-ext" ,"re",
		&remove_ext ,
		2,-1,
		0,
		"Removes relation between tags"
	},
	{
		"remove-tag" ,"rt",
		&remove_tag ,
		2,-1,
		0,
		"Removes tag from file"
	},
	{
		"merge" ,"m" ,
		&merge,
		2,-1,
		0,
		"merges two files"
	},
	{
		"prune" ,"p" ,
		&prune_tags ,
		0, 0,
		0,
		"prunes unused tags from database"
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
execute(PyrosDB *pyrosDB){
	if (Pyros_Execute(pyrosDB) != PYROS_OK){
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

	execute(pyrosDB);
	Pyros_Close_Database(pyrosDB);
}

static void
forEachChild(int argc, char **argv, foreach func){
	int i;
	//eror chech
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	for (i=1; i < argc; i++)
		func(pyrosDB, argv[0], argv[i]);

	execute(pyrosDB);
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
print_exts(PyrosTag** pt,size_t cur,size_t max,int indent){
	size_t i = 0;
	int j;

	for (; i < max; i++) {
		if (cur == pt[i]->par){
			for (j = 0; j < indent;j++)
				printf(">");
			printf("%s",pt[i]->tag);
			if (pt[i]->isAlias)
				printf(" <A>\n");
			else
				printf("\n");

			print_exts(pt,i,max,indent+1);
		}
	}

}

static void
create(int argc, char **argv){
	PyrosDB *pyrosDB;
	UNUSED(argc);
	UNUSED(argv);

	//TODO check that database does not exist
	if (!Pyros_Database_Exists(PDB_PATH)){
		pyrosDB = Pyros_Create_Database(PDB_PATH,PYROS_BLAKE2BHASH);
		execute(pyrosDB);
		Pyros_Close_Database(pyrosDB);
	} else {
		ERROR(stderr,"Database \"%s\" already exists.",PDB_PATH);
	}
}

static void
help(int argc, char **argv){
	int i;

	UNUSED(argc);
	UNUSED(argv);

	printf("Usage: %s [COMMAND] [ARGUMENTS]\n\n",ExecName);

	printf("COMMANDS\n");
	for(i = 0; i < command_length; i++){
		printf("\t%-13s %-4s %s\n",
			   commands[i].longName,commands[i].shortName,commands[i].description);
	}
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
	/*getDirContents(files,dirs,Recursive);*/
	getDirContents(files,dirs,TRUE);

	if (files->length == 0){
		ERROR(stderr,"no valid files given\n");
		exit(1);
	}

	Pyros_Add_Full(pyrosDB, (char**)files->list, files->length,
				   (char**)tags->list, tags->length,
				   TRUE,FALSE, NULL, NULL);

	execute(pyrosDB);
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
get_hash(int argc, char **argv){
	PyrosDB *pyrosDB = open_db(PDB_PATH);
	UNUSED(argc);

	PrintList(Pyros_Get_Tags_From_Hash_Simple(pyrosDB, argv[0],TRUE));
	Pyros_Close_Database(pyrosDB);
}

static void
get_ext(int argc, char **argv){
	PyrosList *tags;
	PyrosDB *pyrosDB = open_db(PDB_PATH);

	UNUSED(argc);
	tags = Pyros_Get_Ext_Tags_Structured(pyrosDB, argv[0],PYROS_TAG_EXT);
	print_exts((PyrosTag**)tags->list,-1,tags->length,0);

	Pyros_List_Free(tags,(Pyros_Free_Callback)Pyros_Free_Tag);
	Pyros_Close_Database(pyrosDB);
}

static void
merge(int argc, char **argv){
	forEachChild(argc,argv,Pyros_Merge_Hashes);
}

static void
remove_ext(int argc, char **argv){
	forEachChild(argc,argv,Pyros_Remove_Ext_Tag);
}

static void
remove_tag(int argc, char **argv){
	forEachChild(argc,argv,Pyros_Remove_Tag_From_Hash);
}

static void
prune_tags(int argc, char **argv){
	PyrosDB *pyrosDB = open_db(PDB_PATH);

	UNUSED(argc);
	UNUSED(argv);

	Pyros_Remove_Dead_Tags(pyrosDB);
	execute(pyrosDB);
	Pyros_Close_Database(pyrosDB);
}

static void
add_tag(int argc, char **argv){
	PyrosDB *pyrosDB = open_db(PDB_PATH);

	Pyros_Add_Tag(pyrosDB,argv[0], &argv[1], argc-1);

	execute(pyrosDB);
	Pyros_Close_Database(pyrosDB);
}
