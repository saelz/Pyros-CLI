#ifndef PYROS_CLI_FILES_H
#define PYROS_CLI_FILES_H

#include "pyros.h"

int isDirectory(const char *path);
int isFile(const char *path);
int pathExists(const char *path);

void cp(const char *src, const char *dst);

void writeListToFile(const PyrosList *list, const char *dst);

void getFilesFromArgs(PyrosList *other, PyrosList *files, PyrosList *dirs,
                      size_t argc, char **argv);

void getDirContents(PyrosList *files, PyrosList *dirs, int isRecursive);
#endif
