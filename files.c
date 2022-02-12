#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <pyros.h>

#include "pyros_cli.h"

extern const char *ExecName;

int
pathExists(const char *path) {
	struct stat statbuf;
	if (stat(path, &statbuf))
		return FALSE;

	return TRUE;
}

int
isDirectory(const char *path) {
	struct stat statbuf;
	if (stat(path, &statbuf))
		return FALSE;

	return S_ISDIR(statbuf.st_mode);
}

int
isFile(const char *path) {
	struct stat statbuf;
	if (stat(path, &statbuf))
		return FALSE;

	return S_ISREG(statbuf.st_mode);
}

void
cp(const char *src_path, const char *dest_path) {
	FILE *src, *dest;
	char buf[4096];
	size_t read_bytes, written_bytes;
	char *ptr;

	src = fopen(src_path, "rb");
	if (src == NULL) {
		ERROR(stderr, "Unable to open source file %s", src_path);
		exit(1);
	}

	dest = fopen(dest_path, "wb");
	if (dest == NULL) {
		ERROR(stderr, "Unable to open destination file %s", dest_path);
		exit(1);
	}

	while ((read_bytes = fread(buf, sizeof(*buf), 4096, src)) > 0) {
		ptr = buf;

		while (read_bytes > 0) {
			written_bytes =
			    fwrite(ptr, sizeof(*ptr), read_bytes, dest);
			read_bytes -= written_bytes;
			ptr += written_bytes;
		}
	}

	fclose(src);
	fclose(dest);
}

void
writeListToFile(const PyrosList *pList, const char *dest_path) {
	FILE *dest;
	size_t i;
	dest = fopen(dest_path, "w");

	if (dest == NULL) {
		ERROR(stderr, "Unable to open file %s", dest_path);
		return;
	}

	for (i = 0; i < pList->length; i++) {
		fwrite(pList->list[i], sizeof(char), strlen(pList->list[i]),
		       dest);
		fwrite("\n", sizeof(char), 1, dest);
	}
	fclose(dest);
}

void
getFilesFromArgs(PyrosList *other, PyrosList *files, PyrosList *dirs,
                 size_t argc, char **argv) {
	size_t i;
	for (i = 0; i < argc; i++) {
		if (isDirectory(argv[i])) {
			if (Pyros_List_Append(dirs, argv[i]) != PYROS_OK)
				goto error;
		} else if (isFile(argv[i])) {
			if (Pyros_List_Append(files, argv[i]) != PYROS_OK)
				goto error;
		} else {
			if (Pyros_List_Append(other, argv[i]) != PYROS_OK)
				goto error;
		}
	}
	return;
error:
	ERROR(stderr, "Out of memory");
	exit(1);
}

void
getDirContents(PyrosList *files, PyrosList *dirs, int isRecursive) {
	DIR *d;
	struct dirent *dir;
	char *new_file;

	size_t i;

	for (i = 0; i < dirs->length; i++) {
		d = opendir(dirs->list[i]);
		if (d) {
			while ((dir = readdir(d)) != NULL) {
				// Condition to check regular file.
				if (dir->d_name[0] == '.' &&
				    (dir->d_name[1] == '.' ||
				     dir->d_name[1] == '\0'))
					continue;

				new_file = malloc(strlen(dirs->list[i]) +
				                  strlen(dir->d_name) + 2);
				if (new_file == NULL) {
					ERROR(stderr, "Out of memory");
					exit(1);
				}

				strcpy(new_file, dirs->list[i]);
				strcat(new_file, "/");
				strcat(new_file, dir->d_name);

				if (isFile(new_file)) {
					Pyros_List_Append(files, new_file);
				} else if (isDirectory(new_file)) {
					if (isRecursive)
						Pyros_List_Append(dirs,
						                  new_file);
					else
						free(new_file);
				} else {
					free(new_file);
				}
			}
			closedir(d);
		}
	}
}
