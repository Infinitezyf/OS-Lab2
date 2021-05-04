#告诉make怎么编译和链接
objects = main.c

MyShell:$(objects)
	gcc -o MyShell $(objects) -g -lpthread

.PHONY:clean

clean:
	rm $(objects)
