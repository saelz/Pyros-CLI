#ifndef PYROS_CLI_H
#define PYROS_CLI_H

#define ERROR fprintf(stderr,"%s: ",ExecName);fprintf
#define UNUSED(x) (void)(x)
#define ARGCHECK(arg,argsmall,argcheck) !strcmp(arg,argcheck) || !strcmp(argsmall,argcheck)


#define TRUE (1==1)
#define FALSE !TRUE

enum FlagType{
	FLAG_VAR,
	FLAG_FUNC
};

typedef void (*FlagFunc)(int argc,char *argv[],int flag_index);
typedef void (*Command)(int argc,char **argv);

struct Flag{
	char shortName;
	const char *longName;
	enum FlagType type;
	union{
		int value;
		FlagFunc func;
	} flag;
};

struct Cmd{
	const char *longName;
	const char *shortName;
	Command func;
	int minArgs;
	int maxArgs;
	struct Flag *flag;
	const char *description;
};

void print_error(char *,const void*);
#endif
