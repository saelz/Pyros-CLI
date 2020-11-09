#include <dirent.h>
#include <stddef.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

#include <pyros.h>

int
isDirectory(const char *path) {
	struct stat statbuf;
	if (stat(path, &statbuf) != 0)
		return 0;

	return S_ISDIR(statbuf.st_mode);
}

int
isFile(const char *path){
	struct stat statbuf;
	if (stat(path, &statbuf) != 0)
		return 0;

	return S_ISREG(statbuf.st_mode);
}

void
getFilesFromArgs(PyrosList *other, PyrosList *files, PyrosList *dirs,size_t argc,
				 char **argv){
	size_t i;
	for (i=0; i < argc; i++){
		if (isDirectory(argv[i]))
			Pyros_List_Append(dirs,argv[i]);
		else if(isFile(argv[i]))
			Pyros_List_Append(files,argv[i]);
		else
			Pyros_List_Append(other,argv[i]);
	}
}

void getDirContents(PyrosList *files, PyrosList *dirs,int isRecursive){
	DIR *d;
	struct dirent *dir;
	char *new_file;

	size_t i;

	for (i = 0; i < dirs->length; i++) {
		d = opendir(dirs->list[i]);
		if (d){
			while ((dir = readdir(d)) != NULL){
				//Condition to check regular file.
				if (dir->d_name[0] == '.' &&
					(dir->d_name[1] == '.' || dir->d_name[1] == '\0'))
					continue;

				new_file = malloc(strlen(dirs->list[i])+strlen(dir->d_name)+2);

				strcpy(new_file,dirs->list[i]);
				strcat(new_file,"/");
				strcat(new_file,dir->d_name);

				if(isFile(new_file)){
					Pyros_List_Append(files, new_file);
				} else if (isDirectory(new_file)){
					if (isRecursive)
						Pyros_List_Append(dirs, new_file);
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
