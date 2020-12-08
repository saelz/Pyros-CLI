#ifndef PYROS_CLI_H
#define PYROS_CLI_H

#define ERROR fprintf(stderr,"%s: ",ExecName);fprintf
#define UNUSED(x) (void)(x)
#define ARGCHECK(arg,argsmall,argcheck) !strcmp(arg,argcheck) || !strcmp(argsmall,argcheck)
#define LENGTH(x) sizeof(x)/sizeof(x[0])


#define TRUE (1==1)
#define FALSE !TRUE

typedef void (*Command)(int argc,char **argv);

enum GLOBAL_FLAGS{
	GLOBAL_HELP_FLAG = 0x01,
	GLOBAL_DIR_FLAG  = 0x02,
};

enum COMMAND_FLAGS{
	CMD_HASH_FLAG      = 0x01,
	CMD_RECURSIVE_FLAG = 0x02,
	CMD_INPUT_FLAG     = 0x04,
};

struct Flag{
	char shortName;
	const char *longName;
	const char *desc;
	const char *usage;
	int value;
};

struct Cmd{
	const char *longName;
	const char *shortName;
	Command func;
	int minArgs;
	int maxArgs;
	int supported_flags;
	const char *description;
	const char *usage;
};


void print_error(char *,const void*);
#endif
