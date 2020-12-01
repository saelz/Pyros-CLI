#include <dirent.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>

#include <pyros.h>

#include "pyros_cli.h"




extern const struct Cmd commands[];
extern const int command_length;

char const* ExecName;

char *PDB_PATH = NULL;
int global_flags = 0;
int flags    = 0;

struct Flag gflags[] = {
	{
		'h',"help",
		"shows help page for command",
		"",
		GLOBAL_HELP_FLAG
	},
	{
		'd',"database" ,
		"sets database to use",
		"<dir>",
		GLOBAL_DIR_FLAG
	},
};

size_t gflags_len = LENGTH(gflags);

struct Flag cmdflags[] = {
	{
		'H',"show-hash",
		"returns file hases instead of file paths",
		"",
		CMD_HASH_FLAG
	},
	{
		'r',
		"recursive",
		"gets all files recursivly in directory",
		"",
		CMD_RECURSIVE_FLAG
	},
};

static const struct Cmd *
cmp_cmd(const struct Cmd *commands,int cmd_count,char *text){

	for (int j = 0; j < cmd_count; j++){
		if (ARGCHECK(commands[j].shortName,commands[j].longName,text)){
			return &commands[j];
		}
	}

	return NULL;
}

static void
get_database_path(){
	const char *homedir;
	const char *env_PYROSDB_path;
	const char *default_path = "/.local/share/pyros/main/";

	if ((homedir = getenv("HOME")) != NULL)
		homedir = getpwuid(getuid())->pw_dir;

	if ((env_PYROSDB_path = getenv("PYROSDB")) != NULL &&
		env_PYROSDB_path[0] != '\0') {
		if (env_PYROSDB_path[0] == '~'){
			PDB_PATH = malloc(strlen(homedir)+strlen(env_PYROSDB_path+1)+1);
			strcpy(PDB_PATH,homedir);
			strcat(PDB_PATH,env_PYROSDB_path+1);
		} else {
			PDB_PATH = malloc(strlen(env_PYROSDB_path)+1);
			strcpy(PDB_PATH,env_PYROSDB_path);
		}
	} else {
		PDB_PATH = malloc(strlen(homedir)+strlen(default_path)+1);
		strcpy(PDB_PATH,homedir);
		strcat(PDB_PATH,default_path);
	}
}

static void
help(const struct Cmd *cmd){

	if (cmd == NULL){
		commands[0].func(0,NULL);
	} else {
		/*printf("COMMAND: %s %s \n  %s\n",
			   cmd->longName,cmd->shortName,
			   cmd->description);*/
		printf("USAGE:\n  %s <%s | %s> [options] %s\n\n%s\n",
			   ExecName,cmd->longName,cmd->shortName,cmd->usage,
			cmd->description);
		if (cmd->supported_flags != 0){
			printf("\nOPTIONS:\n");
			for (size_t i = 0; i < LENGTH(cmdflags);i++){
				if (cmd->supported_flags & cmdflags[i].value)
					printf("  -%c --%s %s\t%s\n",
						   cmdflags[i].shortName,cmdflags[i].longName,
						   cmdflags[i].usage,cmdflags[i].desc);
			}
		}
	}
	exit(0);

}

static void
set_dir(char *dir){
	const char *homedir;

	if ((homedir = getenv("HOME")) != NULL)
		homedir = getpwuid(getuid())->pw_dir;

	if (dir[0] == '~'){
		PDB_PATH = malloc(strlen(homedir)+strlen(dir)+1);
		strcpy(PDB_PATH,homedir);
		strcat(PDB_PATH,dir+1);
	} else {
		PDB_PATH = malloc(strlen(dir)+1);
		strcpy(PDB_PATH,dir);
	}
}

static char
cmp_short_flags(struct Flag *flags,int flag_count,int* set_flags,char *text){
	int length = strlen(text);
	for (int i = 0; i < length; i++) {
		for (int j = 0; j < flag_count; j++) {
			if (text[i] == flags[j].shortName){
				(*set_flags) |= flags[j].value;
				goto next;
			}
		}
		return text[i];
	next:;
	}
	return '\0';
}

static int
cmp_long_flags(struct Flag *flags,int flag_count,int* set_flags,char *text){
	for (int i = 0; i < flag_count; i++) {
		if (!strcmp(text, flags[i].longName)){
			(*set_flags) |= flags[i].value;
			return TRUE;
		}
	}
	return FALSE;
}

static void
check_arg_count(int arg_count,const struct Cmd *cmd){
	if (cmd->maxArgs != -1 && arg_count > cmd->maxArgs){
		ERROR(stderr,"command \"%s\""
			  " can only take %d argument%c %d given\n",
			  cmd->longName,cmd->maxArgs,
			  (cmd->maxArgs < 2) ? '\0' : 's',
			  arg_count);
		exit(1);
	} else if (arg_count < cmd->minArgs){
		ERROR(stderr,"command \"%s\""
			  " requires at least %d argument%c %d given\n",
			  cmd->longName,cmd->minArgs,
			  (cmd->minArgs < 2) ? '\0' : 's',
			  arg_count);
		exit(1);
	}
}

static void
parse_input(int argc,char *argv[]){


	const struct Cmd *cmd = NULL;
	char *cmd_args[argc];
	int cmd_arg_count = 0;

	int ignore_flags  = FALSE;

	for (int i = 1; i < argc; i++) {
		if (global_flags & GLOBAL_DIR_FLAG){
			set_dir(argv[i]);
			global_flags &= ~GLOBAL_DIR_FLAG;
			continue;
		}

		/* check for flags */
		if (!ignore_flags && argv[i][0] == '-'){
			if (argv[i][1] == '-'){
				if (argv[i][2] == '\0'){
					ignore_flags = TRUE;
				} else if (cmp_long_flags(cmdflags, LENGTH(cmdflags),
										  &flags,
										  &argv[i][2])){
				} else if (cmp_long_flags(gflags, LENGTH(gflags),
										  &global_flags,
										  &argv[i][2])){
				} else {
					ERROR(stderr,"Unknown option \"%s\"\n",argv[i]);
					exit(1);
				}
			} else {
				char last_char;
				if (cmp_short_flags(cmdflags,
									LENGTH(cmdflags),
									&flags,
									&argv[i][1]) == '\0'){
				}else if ((last_char = cmp_short_flags(gflags,
													   LENGTH(gflags),
													   &global_flags,
													   &argv[i][1])) == '\0'){
				} else {
					ERROR(stderr,"Unknown option \"-%c\"\n",last_char);
					exit(1);
				}
			}
			continue;
		}

		/* get command */
		if (cmd == NULL){
			if ((cmd=cmp_cmd(commands,command_length,argv[i])) == NULL){
				if (global_flags & GLOBAL_HELP_FLAG)
					help(NULL);
				ERROR(stderr,"Unknown Command \"%s\"\n",argv[i]);
				exit(1);
			}
			continue;
		}

		cmd_args[cmd_arg_count] = argv[i];
		cmd_arg_count++;
	}

	if (global_flags & GLOBAL_HELP_FLAG)
		help(cmd);

	if (cmd == NULL){
		ERROR(stderr,"No command given\n");
		exit(1);
	}

	if ((~cmd->supported_flags & flags) != 0){
		for (size_t i = 0; i < LENGTH(cmdflags); i++) {
			if (cmdflags[i].value & flags){
				ERROR(stderr,"option \"%s\" not applicable to command \"%s\"\n",
					  cmdflags[i].longName,cmd->longName);
				exit(1);
			}
		}

	}

	if (PDB_PATH == NULL)
		get_database_path();

	check_arg_count(cmd_arg_count,cmd);

	cmd->func(cmd_arg_count,cmd_args);

}

int
main(int argc, char *argv[]){

	ExecName = argv[0];

	parse_input(argc, argv);

	free(PDB_PATH);

	return 0;
}
