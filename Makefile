.DEFAULT_GOAL := fx
SRC_DIR := src
INC_DIR := include
TEST_SRC_DIR := test/src
TEST_INC_DIR := test/include
TEST_BINARY := test_bin

ifeq (${INSTALL_DIR},)
	INSTALL_DIR := /usr/local/bin
endif

CXX := clang++
CXXFLAGS := -Wall -Werror -std=gnu++2b -IX11
LDFLAGS := -lX11

ifeq (${CXX}, g++)
	CXXFLAGS += -fconcepts-diagnostics-depth=2
endif

HEADERS = \
		  ${INC_DIR}/window_context.h

OBJS = \
		${SRC_DIR}/main.o \
		${SRC_DIR}/window_context.o

.PHONY: clean

debug: CXXFLAGS += -Og -fsanitize=unreachable -fsanitize=undefined
release: CXXFLAGS += -O3 -march=native
fx: CXXFLAGS += -O3 -march=native
memtest: CXXFLAGS += -DTEST -fsanitize=unreachable -fsanitize=undefined

debug: ${OBJS}
	${CXX} -o $@ $^ ${CXXFLAGS} ${LDFLAGS}

release: ${OBJS}
	${CXX} -o $@ $^ ${CXXFLAGS} ${LDFLAGS}

fx: ${OBJS}
	${CXX} -o fx_bin $^ ${CXXFLAGS} ${LDFLAGS}

memtest: ${OBJS}
	${CXX} -o debug $^ ${CXXFLAGS} ${LDFLAGS} && valgrind --track-origins=yes --leak-check=full ./debug ${PATTERN} ; rm -f ./debug

%.o: %.cpp ${HEADERS}
	${CXX} -c -o $@ $< ${CXXFLAGS}

install:
	mv fx_bin ${INSTALL_DIR}
	cp fx ${INSTALL_DIR}

uninstall:
	rm ${INSTALL_DIR}/fx_bin
	rm ${INSTALL_DIR}/fx

clean:
	find . -name '*.o' -delete
	rm -f debug
	rm -f release
	rm -f fx_bin
