#include <dirent.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>

#include <pyros.h>

#include "pyros_cli.h"



int flags;

extern const struct Cmd commands[];
extern const int command_length;

char const* ExecName;
const struct Cmd *current_cmd;

char *PDB_PATH = NULL;

static void
use_flag(int argc, char **argv,int index,struct Flag flag){
	if (flag.type == FLAG_FUNC){
		flag.flag.func(argc,argv,index);
	} else {
		/* TODO */
	}
}

static void
compare_flag(int argc, char **argv,int index,struct Flag *flag,int flag_count,int useLongName){
	int i;

	for (i = 0; i < flag_count; i++){
		if (useLongName){
			if (!strcmp(flag[i].longName,argv[index]+2)){
				use_flag(argc,argv,index,flag[i]);
				return;
			}
		} else {
			for(int j = 1; argv[index][j] != '\0';j++){
				if (flag[i].shortName == argv[index][j]){
					use_flag(argc,argv,index,flag[i]);
					return;
				}
			}
		}
	}
	ERROR(stderr,"unknown flag \"%s\"\n",argv[index]);
	exit(1);
}

static void
findFlags(int argc, char **argv,struct Flag *flag,int flag_count){
	int i;

	for (i = 1; i < argc; i++){
		if (argv[i] == NULL)
			continue;
		if (!strcmp(argv[i], "--")){
			compare_flag(argc,argv,i,flag,flag_count, TRUE);
			argv[i] = NULL;
			break;
		} else if (argv[i][0] == '-'){
			compare_flag(argc,argv,i,flag,flag_count, FALSE);
			argv[i] = NULL;
		}
	}
}

static const struct Cmd *
findCmd(const struct Cmd *commands,int argc, char**argv,int cmd_count){
	int i,j;

	for (i = 1; i < argc; i++){
		if (argv[i][0] != '-'){
			for (j = 0; j < cmd_count; j++){
				if (ARGCHECK(commands[j].shortName,commands[j].longName,argv[i])){
					argv[i] = NULL;
					return &commands[j];
				}
			}
			ERROR(stderr,"Unknown Command \"%s\"\n",argv[i]);
			exit(1);
		}
	}

	return NULL;
}

static void
findCmdArgs(char **cmd_args, int *cmd_arg_count, int argc, char **argv){
	int i;

	for (i = 1; i < argc; i++){
		if (argv[i] != NULL){
			cmd_args[*cmd_arg_count] = argv[i];
			(*cmd_arg_count)++;
		}
	}
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
help(int argc,char *argv[],int index){
	UNUSED(argc);
	UNUSED(argv);
	UNUSED(index);

	if (current_cmd == NULL){
		commands[0].func(argc,argv);
	} else {
		printf("COMMAND: %s %s \n\t%s\n",
			   current_cmd->longName,current_cmd->shortName,
			   current_cmd->description);
	}
	exit(0);

}

static void
get_dir(int argc,char *argv[],int index){
	const char *homedir;

	if (index+1 >= argc){
		ERROR(stderr,"no directory supplied\n");
		exit(1);
	}

	if ((homedir = getenv("HOME")) != NULL)
		homedir = getpwuid(getuid())->pw_dir;

	if (argv[index+1][0] == '~'){
		PDB_PATH = malloc(strlen(homedir)+strlen(argv[index+1])+1);
		strcpy(PDB_PATH,homedir);
		strcat(PDB_PATH,argv[index+1]+1);
	} else {
		PDB_PATH = malloc(strlen(argv[index+1])+1);
		strcpy(PDB_PATH,argv[index+1]);
	}
	argv[index+1] = NULL;
}

int
main(int argc, char *argv[]){

	struct Flag global_flags[] = {
		{'h',"help" ,FLAG_FUNC,{.func = help}},
		{'d',"dir"  ,FLAG_FUNC,{.func = get_dir}},
		/*{'r',"recursive",FLAG_VAR,{&flag_Recursive}},should be function flag for a*/
	};



	char *cmd_args[argc];
	int cmd_arg_count = 0;


	ExecName = argv[0];

	current_cmd = findCmd(commands,argc,argv,command_length);
	findFlags(argc,argv,global_flags,
			  sizeof(global_flags)/sizeof(global_flags[0]));

	if (current_cmd == NULL){
		ERROR(stderr,"No command given\n");
		exit(1);
	}

	findCmdArgs(cmd_args,&cmd_arg_count,argc,argv);

	/*parse command flags*/
	if (current_cmd->maxArgs != -1 && cmd_arg_count > current_cmd->maxArgs){
		ERROR(stderr,"command \"%s\""
			  " can only take %d argument%c %d given\n",
			  current_cmd->longName,current_cmd->maxArgs,
			  (cmd_arg_count != 1) ? '\0' : 's',
			  cmd_arg_count);
		exit(1);
	} else if (cmd_arg_count < current_cmd->minArgs){
		ERROR(stderr,"command \"%s\""
			  " requires at least %d argument%c %d given\n",
			  current_cmd->longName,current_cmd->minArgs,
			  (cmd_arg_count != 1) ? '\0' : 's',
			  cmd_arg_count);
		exit(1);
	}


	if (PDB_PATH == NULL)
		get_database_path();

	printf("%s\n",PDB_PATH);


	current_cmd->func(cmd_arg_count,cmd_args);

	free(PDB_PATH);

	return 0;
}
