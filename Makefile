CC=cl
CFLAGS=/MD /D DLL_EXPORTS

all: so_stdio.dll

build: so_stdio.dll

so_stdio.dll: so_stdio.obj
	link /dll /out:so_stdio.dll so_stdio.obj

so_stdio.obj: so_stdio.c
	$(CC) /c so_stdio.c $(CFLAGS)
 
.PHONY: clean
clean:
	del so_stdio.dll so_stdio.lib so_stdio.exp *.obj