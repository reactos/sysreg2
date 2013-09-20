TARGET := sysreg2

.PHONY: all

all: $(TARGET)

CC=gcc
CXX=g++
INCLUDE_DIR = -I/usr/include/libvirt/ -I/usr/include/libxml2/
CFLAGS := $(INCLUDE_DIR) -g -O0 -std=c99 -D_GNU_SOURCE -Wall -Wextra
CXXFLAGS := $(INCLUDE_DIR) -g -O0 -D_GNU_SOURCE -Wall -Wextra
LFLAGS := -L/usr/lib64
LIBS := -lvirt -lxml2

SRCS_C := utils.c console.c options.c raddr2line.c revision.c
SRCS_CPP := virt.cpp libvirt.cpp vmware_esx.cpp vmware_player.cpp kvm.cpp

OBJS_C := $(SRCS_C:.c=.o)
OBJS_CPP := $(SRCS_CPP:.cpp=.o)

$(TARGET): $(OBJS_C) $(OBJS_CPP)
	$(CXX) $(LFLAGS) -o $@ $(OBJS_CPP) $(OBJS_C) $(LIBS)
	rm revision.c

.c.o: revision.c $<
	$(CC) $(INC) $(CFLAGS) -c $< -o $@

revision.c:
	echo 'const int SVNRev = ' > revision.c
	LC_ALL=C LANG=C svn info | grep Revision | cut -f 2 -d " " >> revision.c
	echo ';' >> revision.c

.PHONY: clean
clean:
	-@rm $(TARGET)
	-@rm $(OBJS_C)
	-@rm $(OBJS_CPP)
					
