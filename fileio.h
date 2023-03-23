#pragma once

#include "log.h"


#ifndef FILEIO_H
#define FILEIO_H


	int get_last_file_size();
	size_t get_last_zip_file_size();
	int getFileSize(FILE *input);
	unsigned char *load_file(char *filename);
	int save_file(char *filename, unsigned char *buf, int size);
	unsigned char* load_generic_zip(const char *archname, const char *filename);



#endif