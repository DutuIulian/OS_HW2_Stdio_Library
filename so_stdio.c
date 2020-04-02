#include "so_stdio.h"

#include <fcntl.h>
#include <string.h>

#if defined(__linux__)
#include <unistd.h>
#elif defined(_WIN32)

#endif

#define BUFFER_SIZE 4096

struct _so_file {
#if defined(__linux__)
int fd;
#elif defined(_WIN32)
HANDLE hFile;
#endif
long fileCursor;
long bufferFirstIndex;
long bufferLastIndex;
char buffer[BUFFER_SIZE];
char pathName[128];
char mode[16];
char written;
char reachedTheEnd;
};

#if defined(__linux__)
FUNC_DECL_PREFIX int so_fileno(SO_FILE *stream)
{
	if (stream != NULL)
		return stream->fd;
	else
		return -1;
}
#elif defined(_WIN32)
FUNC_DECL_PREFIX HANDLE so_fileno(SO_FILE *stream)
{
	return stream->hFile;
}
#endif

FUNC_DECL_PREFIX int getSize(const char *pathname)
{
	char buffer[4096];
	long size = 0;
	int bytesRead;
#if defined(__linux__)
	int fd = open(pathname, O_RDONLY);

	while (1) {
		bytesRead = read(fd, buffer, 4096);
		if (bytesRead <= 0)
			break;
		size += bytesRead;
	}
#elif defined(_WIN32)
	HANDLE hFile = CreateFile(pathname, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	while (1) {
		ReadFile(hFile, buffer, BUFFER_SIZE, &bytesRead, NULL);
		if (bytesRead <= 0)
			break;
		size += bytesRead;
	}
#endif
	return size;
}

FUNC_DECL_PREFIX SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	SO_FILE *file = calloc(1, sizeof(SO_FILE));

	strcpy(file->pathName, pathname);
	strcpy(file->mode, mode);
	file->written = 0;
	file->bufferFirstIndex = 0;
	file->bufferLastIndex = -1;
	file->fileCursor = 0;
	file->reachedTheEnd = 0;
#if defined(__linux__)
	if (strcmp(mode, "r") == 0)
		file->fd = open(pathname, O_RDONLY);
	else if (strcmp(mode, "w") == 0)
		file->fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	else if (strcmp(mode, "a") == 0)
		file->fd = open(pathname, O_WRONLY | O_APPEND, 0644);
	else if (strcmp(mode, "a+") == 0 || strcmp(mode, "w+") == 0) {
		file->fd = open(pathname, O_RDWR | O_APPEND, 0644);
		file->bufferFirstIndex = 0;
	} else if (strcmp(mode, "r+") == 0) {
		file->fd = open(pathname, O_RDWR, 0644);
		so_fseek(file, 0, SEEK_SET);
	} else {
		free(file);
		return NULL;
	}

	if (so_fileno(file) < 0) {
		free(file);
		return NULL;
	}
#elif defined(_WIN32)
	if (strcmp(mode, "r") == 0)
		file->hFile = CreateFile(pathname, GENERIC_READ|GENERIC_WRITE,
			FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_READONLY, NULL);
	else if (strcmp(mode, "r+") == 0)
		file->hFile = CreateFile(pathname, GENERIC_READ|GENERIC_WRITE,
			FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, NULL);
	else if (strcmp(mode, "w") == 0)
		file->hFile = CreateFile(pathname, GENERIC_READ|GENERIC_WRITE,
			FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, NULL);
	else if (strcmp(mode, "w+") == 0)
		file->hFile = CreateFile(pathname, GENERIC_READ|GENERIC_WRITE,
			FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, NULL);
	else if (strcmp(mode, "a") == 0)
		file->hFile = CreateFile(pathname, FILE_APPEND_DATA,
			FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, NULL);
	else if (strcmp(mode, "a+") == 0)
		file->hFile = CreateFile(pathname, FILE_APPEND_DATA,
			FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, NULL);
	else {
		free(file);
		return NULL;
	}

	if (file->hFile == INVALID_HANDLE_VALUE) {
		free(file->buffer);
		free(file);
		return NULL;
	}
#endif
	return file;
}

FUNC_DECL_PREFIX int so_fclose(SO_FILE *stream)
{
	int ret;

	if (stream == NULL)
		return -1;
	ret = so_fflush(stream);
#if defined(__linux__)
	close(stream->fd);
#elif defined(_WIN32)
	CloseHandle(stream->hFile);
#endif
	free(stream);

	if (ret != 0)
		return -1;

	return 0;
}

FUNC_DECL_PREFIX int so_fflush(SO_FILE *stream)
{
	int written, i;
	char error;

	if (stream == NULL)
		return -1;

	if (stream->written && stream->bufferLastIndex > 0)
		while (1) {
#if defined(__linux__)
			written = write(stream->fd, stream->buffer,
					stream->bufferLastIndex);
			error = written < 0;
#elif defined(_WIN32)
			error = (WriteFile(stream->hFile,
					stream->buffer, stream->bufferLastIndex,
					&written, NULL) == 0);
#endif
			if (error)
				return -1;

			if (written == stream->bufferLastIndex) {
				stream->bufferFirstIndex = 0;
				stream->bufferLastIndex = -1;
				return 0;
			}

			stream->bufferLastIndex -= written;
			for (i = 0; i < stream->bufferLastIndex; i++)
				stream->buffer[i]
					= stream->buffer[i + written];
		}

	stream->bufferFirstIndex = 0;
	stream->bufferLastIndex = -1;

	return 0;
}

FUNC_DECL_PREFIX int so_fseek(SO_FILE *stream, long offset, int whence)
{
#if defined(__linux__)
	so_fflush(stream);
	lseek(stream->fd, offset, whence);
	if (whence == SEEK_CUR)
		stream->fileCursor += offset;
	else if (whence == SEEK_SET)
		stream->fileCursor = offset;
	else if (whence == SEEK_END)
		stream->fileCursor = getSize(stream->pathName) - offset;
	else
		return -1;

	return 0;
#elif defined(_WIN32)
	DWORD moveMethod;

	so_fflush(stream);

	if (whence == SEEK_CUR) {
		moveMethod = FILE_CURRENT;
		stream->fileCursor += offset;
	} else if (whence == SEEK_SET) {
		moveMethod = FILE_BEGIN;
		stream->fileCursor = offset;
	} else if (whence == SEEK_END) {
		moveMethod = FILE_END;
		stream->fileCursor = getSize(stream->pathName) - offset;
	} else
		return -1;
	SetFilePointer(stream->hFile, offset, 0, moveMethod);

	return 0;
#endif
}

FUNC_DECL_PREFIX long so_ftell(SO_FILE *stream)
{
	return stream->fileCursor;
}

FUNC_DECL_PREFIX
size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	char *buffer = (char *)ptr;
	char c;
	long i, count = 0, limit = (long)size * (long)nmemb;

	for (i = 0; i < limit; i++) {
		c = so_fgetc(stream);
		if (so_feof(stream))
			break;
		buffer[i] = c;
		count++;
	}

	return count / size;
}

FUNC_DECL_PREFIX
size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	char *buffer = (char *)ptr;
	long i, count = 0, limit = (long)size * (long)nmemb;

	for (i = 0; i < limit; i++) {
		so_fputc(buffer[i], stream);
		count++;
	}

	return count / size;
}

FUNC_DECL_PREFIX int so_readMoreIntoBuffer(SO_FILE *stream)
{
	int bytesRead;
	char error;

#if defined(__linux__)
	bytesRead = read(stream->fd, stream->buffer, BUFFER_SIZE);
	error = bytesRead <= 0;
#elif defined(_WIN32)
	error = ReadFile(stream->hFile, stream->buffer, BUFFER_SIZE, &bytesRead, NULL);
#endif
	if (error <= 0) {
		stream->reachedTheEnd = 1;
		return SO_EOF;
	}

	stream->bufferFirstIndex = 0;
	stream->bufferLastIndex = bytesRead - 1;

	return 0;
}

FUNC_DECL_PREFIX int so_fgetc(SO_FILE *stream)
{
	int c, ret;

	if (stream->written) {
		so_fflush(stream);
		stream->written = 0;
	}

	if (stream->bufferFirstIndex > stream->bufferLastIndex) {
		ret = so_readMoreIntoBuffer(stream);
		if (ret == SO_EOF)
			return SO_EOF;
	}

	c = stream->buffer[stream->bufferFirstIndex];
	stream->bufferFirstIndex++;
	stream->fileCursor++;

	return c;
}

FUNC_DECL_PREFIX int so_fputc(int c, SO_FILE *stream)
{
	int i, written;
	int error;

	if (stream->bufferLastIndex == -1)
		stream->bufferLastIndex = 0;

	while (stream->bufferLastIndex == BUFFER_SIZE) {
#if defined(__linux__)
		written = write(stream->fd, stream->buffer, BUFFER_SIZE);
		error = written <= 0;
#elif defined(_WIN32)
		error = WriteFile(stream->hFile, stream->buffer,
				BUFFER_SIZE, &written, NULL);
#endif
		if (!error)
			stream->bufferLastIndex -= written;
		for (i = 0; i < stream->bufferLastIndex; i++)
			stream->buffer[i] = stream->buffer[i + written];
	}

	stream->written = 1;
	stream->buffer[stream->bufferLastIndex] = c;
	stream->bufferLastIndex++;
	stream->fileCursor++;

	return c;
}

FUNC_DECL_PREFIX int so_feof(SO_FILE *stream)
{
	return stream->reachedTheEnd;
}

FUNC_DECL_PREFIX int so_ferror(SO_FILE *stream)
{
	return 1;
}

FUNC_DECL_PREFIX SO_FILE *so_popen(const char *command, const char *type)
{
	return NULL;
}

FUNC_DECL_PREFIX int so_pclose(SO_FILE *stream)
{
	return 0;
}
