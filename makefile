TARGET := sysreg2

.PHONY: all

all: $(TARGET)

CC=gcc
CFLAGS := -g -O0 -std=c99 -D_GNU_SOURCE -Wall -Wextra
LFLAGS := -L/usr/lib64
LIBS := -lvirt -lxml2
INC := -I/usr/include/libvirt/ -I/usr/include/libxml2/

SRCS := virt.c utils.c console.c options.c raddr2line.c revision.c

OBJS := $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(LFLAGS) -o $@ $(OBJS) $(LIBS)
	rm revision.c

.c.o: revision.c $<
	$(CC) $(INC) $(CFLAGS) -c $< -o $@

revision.c:
	echo 'const int SVNRev = ' > revision.c
	LANG=C svn info | grep Revision | cut -f 2 -d " " >> revision.c
	echo ';' >> revision.c

.PHONY: clean
clean:
	-@rm $(TARGET)
	-@rm $(OBJS)
					
