# Platform and paths
PLATFORM ?= linux
PREFIX ?= /usr/local

ifeq ($(PREFIX), /data/data/com.termux/files/usr)
TERMUX=1
endif

# Compiler and flags
CC ?= cc
CCLD ?= $(CC)

INCLUDES ?=
ifeq ($(PLATFORM), linux)
INCLUDES += -I$(PREFIX)/include/libdrm
endif

COMMON_CFLAGS = -Wall -I. -pipe -fPIC -pthread $(INCLUDES)
COMMON_CFLAGS += -DCGD_CONFIG_PLATFORM_LINUX_EVDEV_PS4_CONTROLLER_SUPPORT
DEPFLAGS ?= -MMD -MP

LDFLAGS ?= -pie
ifeq ($(PLATFORM), windows)
LDFLAGS += -municode -mwindows
endif
SO_LDFLAGS = -shared

LIBS ?= -lm
ifeq ($(TERMUX), 1)
LIBS += $(PREFIX)/lib/libandroid-shmem.a -llog
endif
ifeq ($(PLATFORM), windows)
LIBS += -lgdi32
endif

STRIP?=strip
DEBUGSTRIP?=strip -d
STRIPFLAGS?=-g -s

# Shell commands
ECHO = echo
PRINTF = printf
RM = rm -f
TOUCH = touch -c
EXEC = exec
MKDIR = mkdir -p
RMRF = rm -rf
7Z = 7z
INSTALL = install

# Executable file Prefix/Suffix
EXEPREFIX =
EXESUFFIX =
ifeq ($(PLATFORM),windows)
EXESUFFIX = .exe
endif

SO_PREFIX =
SO_SUFFIX = .so
ifeq ($(PLATFORM),windows)
SO_SUFFIX = .dll
endif

# Directories
OBJDIR = obj
BINDIR = bin
TEST_SRC_DIR = tests
TEST_BINDIR = $(TEST_SRC_DIR)/$(BINDIR)
PLATFORM_SRCDIR = platform

# Test sources and objects
TEST_SRCS = $(wildcard $(TEST_SRC_DIR)/*.c)
TEST_EXES = $(patsubst $(TEST_SRC_DIR)/%.c,$(TEST_BINDIR)/$(EXEPREFIX)%$(EXESUFFIX),$(TEST_SRCS))
TEST_LOGFILE = $(TEST_SRC_DIR)/testlog.txt

# Sources and objects
PLATFORM_SRCS = $(wildcard $(PLATFORM_SRCDIR)/$(PLATFORM)/*.c)

_all_srcs=$(wildcard */*.c) $(wildcard *.c)
SRCS = $(filter-out $(TEST_SRCS),$(_all_srcs)) $(PLATFORM_SRCS)

OBJS = $(patsubst %.c,$(OBJDIR)/%.c.o,$(shell basename -a $(SRCS)))
DEPS = $(patsubst %.o,%.d,$(OBJS))

_main_obj = $(OBJDIR)/main.c.o

STRIP_OBJS = $(OBJDIR)/log.c.o

# Executables
EXE = $(BINDIR)/$(EXEPREFIX)main$(EXESUFFIX)
TEST_LIB = $(TEST_BINDIR)/$(SO_PREFIX)libmain_test$(SO_SUFFIX)
TEST_LIB_OBJS = $(filter-out $(_main_obj),$(OBJS))
EXEARGS =
EXE_INSTALL_NAME = ps4-controller-input-faker
EXE_INSTALL_DIR ?= $(PREFIX)/bin
SYSTEMD_SERVICE_FILE = ps4-controller-input-faker.service
SYSTEMD_SERVICE_INSTALL_DIR ?= /etc/systemd/system

.PHONY: all release strip clean mostlyclean update run br tests build-tests run-tests debug-run bdr test-hooks install
.NOTPARALLEL: all release br bdr build-tests install

# Build targets
all: CFLAGS = -ggdb -O0 -Wall -fsanitize=address
all: LDFLAGS += -fsanitize=address
all: $(OBJDIR) $(BINDIR) $(EXE)

release: LDFLAGS += -flto
release: CFLAGS = -O3 -Wall -Werror -flto -DNDEBUG -DCGD_BUILDTYPE_RELEASE
release: clean $(OBJDIR) $(BINDIR) $(EXE) tests mostlyclean strip

br: all run

# Output executable rules
$(EXE): $(OBJS)
	@$(DEBUGSTRIP) $(STRIP_OBJS) 2>/dev/null
	@$(PRINTF) "CCLD 	%-40s %-40s\n" "$(EXE)" "<= $^"
	@$(CCLD) $(LDFLAGS) -o $(EXE) $(OBJS) $(LIBS)

$(TEST_LIB): $(TEST_LIB_OBJS)
	@$(PRINTF) "CCLD 	%-40s %-40s\n" "$(TEST_LIB)" "<= $(TEST_LIB_OBJS)"
	@$(CCLD) $(SO_LDFLAGS) -o $(TEST_LIB) $(TEST_LIB_OBJS) $(LIBS)

# Output directory rules
$(OBJDIR):
	@$(ECHO) "MKDIR	$(OBJDIR)"
	@$(MKDIR) $(OBJDIR)

$(BINDIR):
	@$(ECHO) "MKDIR	$(BINDIR)"
	@$(MKDIR) $(BINDIR)

$(TEST_BINDIR):
	@$(ECHO) "MKDIR	$(TEST_BINDIR)"
	@$(MKDIR) $(TEST_BINDIR)

# Generic compilation targets
$(OBJDIR)/%.c.o: %.c Makefile
	@$(PRINTF) "CC 	%-40s %-40s\n" "$@" "<= $<"
	@$(CC) $(DEPFLAGS) $(COMMON_CFLAGS) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.c.o: */%.c Makefile
	@$(PRINTF) "CC 	%-40s %-40s\n" "$@" "<= $<"
	@$(CC) $(DEPFLAGS) $(COMMON_CFLAGS) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.c.o: $(PLATFORM_SRCDIR)/$(PLATFORM)/%.c Makefile
	@$(PRINTF) "CC 	%-40s %-40s\n" "$@" "<= $<"
	@$(CC) $(DEPFLAGS) $(COMMON_CFLAGS) $(CFLAGS) -c -o $@ $<


# Test preparation targets
test-hooks:

# Test execution targets
run-tests: tests

tests: build-tests test-hooks
	@n_passed=0; \
	$(ECHO) -n > $(TEST_LOGFILE); \
	for i in $(TEST_EXES); do \
		$(PRINTF) "EXEC	%-40s " "$$i"; \
		if CGD_TEST_LOG_FILE="$(TEST_LOGFILE)" $$i >/dev/null 2>&1; then \
			$(PRINTF) "$(GREEN)OK$(COL_RESET)\n"; \
			n_passed="$$((n_passed + 1))"; \
		else \
			$(PRINTF) "$(RED)FAIL$(COL_RESET)\n"; \
		fi; \
	done; \
	n_total=$$(echo $(TEST_EXES) | wc -w); \
	if test "$$n_passed" -lt "$$n_total"; then \
		$(PRINTF) "$(RED)"; \
	else \
		$(PRINTF) "$(GREEN)"; \
	fi; \
	$(PRINTF) "%s/%s$(COL_RESET) tests passed.\n" "$$n_passed" "$$n_total";


# Test compilation targets
build-tests: CFLAGS = -ggdb -O0 -Wall -fsanitize=address
build-tests: LDFLAGS += -fsanitize=address
build-tests: $(OBJDIR) $(BINDIR) $(TEST_BINDIR) $(TEST_LIB) compile-tests

compile-tests: CFLAGS = -ggdb -O0 -Wall
compile-tests: $(TEST_EXES)

$(TEST_BINDIR)/$(EXEPREFIX)%$(EXESUFFIX): CFLAGS = -ggdb -O0 -Wall
$(TEST_BINDIR)/$(EXEPREFIX)%$(EXESUFFIX): $(TEST_SRC_DIR)/%.c Makefile
	@$(PRINTF) "CCLD	%-40s %-40s\n" "$@" "<= $< $(TEST_LIB)"
	@$(CC) $(COMMON_CFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS) $(TEST_LIB) $(LIBS)


# Installation targets
install: $(EXE_INSTALL_DIR) $(SYSTEMD_SERVICE_INSTALL_DIR) all $(SYSTEMD_SERVICE_FILE)
	@$(PRINTF) "INSTALL	%-40s %-40s\n" "$(EXE)" "=> $(EXE_INSTALL_DIR)/$(EXE_INSTALL_NAME)"
	@$(INSTALL) -o 'root' -g 'root' -m '0755' "$(EXE)" "$(EXE_INSTALL_DIR)/$(EXE_INSTALL_NAME)"
	@$(PRINTF) "INSTALL	%-40s %-40s\n" "$(SYSTEMD_SERVICE_FILE)" "=> $(SYSTEMD_SERVICE_INSTALL_DIR)/$(SYSTEMD_SERVICE_FILE)"
	@$(INSTALL) -o 'root' -g 'root' -m '0755' "$(SYSTEMD_SERVICE_FILE)" "$(SYSTEMD_SERVICE_INSTALL_DIR)/$(SYSTEMD_SERVICE_FILE)"

$(EXE_INSTALL_DIR):
	@$(ECHO) "MKDIR	$(EXE_INSTALL_DIR)"
	@$(MKDIR) $(EXE_INSTALL_DIR)

$(SYSTEMD_SERVICE_INSTALL_DIR):
	@$(ECHO) "MKDIR	$(SYSTEMD_SERVICE_INSTALL_DIR)"
	@$(MKDIR) $(SYSTEMD_SERVICE_INSTALL_DIR)

# Cleanup targets
mostlyclean:
	@$(ECHO) "RM	$(OBJS) $(DEPS) $(TEST_LOGFILE)"
	@$(RM) $(OBJS) $(DEPS) $(TEST_LOGFILE)

clean:
	@$(ECHO) "RM	$(OBJS) $(DEPS) $(EXE) $(TEST_LIB) $(BINDIR) $(OBJDIR) $(TEST_EXES) $(TEST_BINDIR) $(TEST_LOGFILE)"
	@$(RM) $(OBJS) $(DEPS) $(EXE) $(TEST_LIB) $(TEST_EXES) $(TEST_LOGFILE) assets/tests/asset_load_test/*.png
	@$(RMRF) $(OBJDIR) $(BINDIR) $(TEST_BINDIR)

# Output execution targets
run:
	@$(ECHO) "EXEC	$(EXE) $(EXEARGS)"
	@$(EXEC) $(EXE) $(EXEARGS)

debug-run:
	@$(ECHO) "EXEC	$(EXE) $(EXEARGS)"
	@bash -c '$(EXEC) -a debug $(EXE) $(EXEARGS)'

bdr: all debug-run

# Miscellaneous targets
strip:
	@$(ECHO) "STRIP	$(EXE)"
	@$(STRIP) $(STRIPFLAGS) $(EXE)

update:
	@$(ECHO) "TOUCH	$(SRCS)"
	@$(TOUCH) $(SRCS)

-include $(DEPS)
