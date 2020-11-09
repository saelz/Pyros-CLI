#ifndef PYROS_CLI_FILES_H
#define PYROS_CLI_FILES_H

#include "pyros.h"

int isDirectory(const char *path);
int isFile(const char *path);

void getFilesFromArgs(PyrosList *other, PyrosList *files, PyrosList *dirs,
					  size_t argc, char **argv);

void getDirContents(PyrosList *files, PyrosList *dirs,int isRecursive);
#endif
