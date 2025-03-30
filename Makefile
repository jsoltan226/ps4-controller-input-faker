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

COMMON_CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -I. -pipe -fPIC -pthread $(INCLUDES)
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
STRIPFLAGS?=-g -s

# Shell commands
ECHO = echo
PRINTF = printf
RM = rm -f
TOUCH = touch
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
_release_build_marker = CGD_BUILDTYPE_RELEASE__

# Test sources and objects
TEST_SRCS = $(wildcard $(TEST_SRC_DIR)/*.c)
TEST_EXES = $(patsubst $(TEST_SRC_DIR)/%.c,$(TEST_BINDIR)/$(EXEPREFIX)%$(EXESUFFIX),$(TEST_SRCS))
TEST_LOGFILE = $(TEST_SRC_DIR)/testlog.txt

# Sources and objects
PLATFORM_SRCS = $(wildcard $(PLATFORM_SRCDIR)/$(PLATFORM)/*.c)

_all_srcs=$(wildcard */*.c) $(wildcard *.c)
SRCS = $(filter-out $(TEST_SRCS),$(_all_srcs)) $(PLATFORM_SRCS)

_real_objs=$(patsubst %.c,$(OBJDIR)/%.c.o,$(shell basename -a $(SRCS)))
OBJS = $(shell grep -q "$(_release_build_marker)" "$(EXE)" 2>/dev/null || echo $(_real_objs))
DEPS = $(patsubst %.o,%.d,$(OBJS))

_main_obj = $(OBJDIR)/main.c.o

# Executables
EXE = $(BINDIR)/$(EXEPREFIX)main$(EXESUFFIX)
TEST_LIB = $(TEST_BINDIR)/$(SO_PREFIX)libmain_test$(SO_SUFFIX)
TEST_LIB_OBJS = $(filter-out $(_main_obj),$(_real_objs))
EXEARGS =
EXE_INSTALL_NAME = ps4-controller-input-faker
EXE_INSTALL_DIR ?= $(PREFIX)/bin
SYSTEMD_SERVICE_FILE = ps4-controller-input-faker.service
SYSTEMD_SERVICE_INSTALL_DIR ?= $(PREFIX)/lib/systemd/system
CONFIG_FILE = ps4-controller-input-faker.ini
CONFIG_INSTALL_DIR ?= $(PREFIX)/etc

.NOTPARALLEL: build-tests install objects exe

# Build targets
.PHONY: all
.NOTPARALLEL: all
all: CFLAGS = -ggdb -O0 -Wall -fsanitize=address
all: LDFLAGS += -fsanitize=address
all: exe

.PHONY: release
.NOTPARALLEL: release
release: LDFLAGS += -flto
release: SO_LDFLAGS += -flto
release: CFLAGS = -O3 -Wall -Werror -flto -DNDEBUG -DCGD_BUILDTYPE_RELEASE
release: clean $(OBJDIR) parallel-real-objects exe tests mostlyclean strip

.PHONY: br
.NOTPARALLEL: br
br: all run

# Output executable rules
.PHONY: exe
.NOTPARALLEL: exe
exe: objects $(BINDIR) $(EXE)

$(EXE): $(OBJS)
	@$(PRINTF) "CCLD 	%-40s %-40s\n" "$(EXE)" "<= $(OBJS) $(LIBS)"
	@$(CCLD) $(LDFLAGS) -o $(EXE) $(OBJS) $(LIBS)

.PHONY: test-lib
.NOTPARALLEL: test-lib
test-lib: test-lib-objects $(TEST_BINDIR) $(TEST_LIB)

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
.PHONY: objects parallel-objects
.NOTPARALLEL: objects
objects: $(OBJDIR) parallel-objects
parallel-objects: $(OBJS)

.PHONY: parallel-real-objects
parallel-real-objects: $(_real_objs)

.PHONY: test-lib-objects test-lib-parallel-objects
.NOTPARALLEL: test-lib-objects
test-lib-objects: $(OBJDIR) test-lib-parallel-objects
test-lib-parallel-objects: $(TEST_LIB_OBJS)

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
.PHONY: test-hooks
.NOTPARALLEL: test-hooks
test-hooks:

# Test execution targets
.PHONY: run-tests
run-tests: tests

.PHONY: tests
.NOTPARALLEL: tests
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
.PHONY: build-tests
.NOTPARALLEL: build-tests
build-tests: CFLAGS = -ggdb -O0 -Wall -fsanitize=address
build-tests: LDFLAGS += -fsanitize=address
build-tests: test-lib compile-tests

.PHONY: compile-tests
.NOTPARALLEL: compile-tests
compile-tests: CFLAGS = -ggdb -O0 -Wall -fsanitize=address
build-tests: LDFLAGS += -fsanitize=address
compile-tests: $(TEST_EXES)

$(TEST_BINDIR)/$(EXEPREFIX)%$(EXESUFFIX): CFLAGS = -ggdb -O0 -Wall
$(TEST_BINDIR)/$(EXEPREFIX)%$(EXESUFFIX): $(TEST_SRC_DIR)/%.c Makefile
	@$(PRINTF) "CCLD	%-40s %-40s\n" "$@" "<= $< $(TEST_LIB)"
	@$(CC) $(COMMON_CFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS) $(TEST_LIB) $(LIBS)


# Installation targets
.PHONY: install
.NOTPARALLEL: install
# Here we don't care whether the executable is up to date; we only need it to ~~exist~~
install: $(OBJDIR) $(BINDIR) $(EXE) $(EXE_INSTALL_DIR) $(SYSTEMD_SERVICE_INSTALL_DIR) $(CONFIG_INSTALL_DIR) $(SYSTEMD_SERVICE_FILE) $(CONFIG_FILE)
	@$(PRINTF) "INSTALL	%-40s %-40s\n" "$(EXE)" "=> $(EXE_INSTALL_DIR)/$(EXE_INSTALL_NAME)"
	@$(INSTALL) -o 'root' -g 'root' -m '0755' "$(EXE)" "$(EXE_INSTALL_DIR)/$(EXE_INSTALL_NAME)"
	@$(PRINTF) "INSTALL	%-40s %-40s\n" "$(SYSTEMD_SERVICE_FILE)" "=> $(SYSTEMD_SERVICE_INSTALL_DIR)/$(SYSTEMD_SERVICE_FILE)"
	@$(INSTALL) -o 'root' -g 'root' -m '0644' "$(SYSTEMD_SERVICE_FILE)" "$(SYSTEMD_SERVICE_INSTALL_DIR)/$(SYSTEMD_SERVICE_FILE)"
	@$(PRINTF) "INSTALL	%-40s %-40s\n" "$(CONFIG_FILE)" "=> $(CONFIG_INSTALL_DIR)/$(CONFIG_FILE)"
	@$(INSTALL) -o 'root' -g 'root' -m '0644' "$(CONFIG_FILE)" "$(CONFIG_INSTALL_DIR)/$(CONFIG_FILE)"

.PHONY: install
.NOTPARALLEL: install
uninstall:
	@$(PRINTF) "RM	%-40s %-40s\n" "$(EXE_INSTALL_DIR)/$(EXE_INSTALL_NAME)"
	@$(RM) "$(EXE_INSTALL_DIR)/$(EXE_INSTALL_NAME)"
	@$(PRINTF) "RM	%-40s %-40s\n" "$(SYSTEMD_SERVICE_INSTALL_DIR)/$(SYSTEMD_SERVICE_FILE)"
	@$(RM) "$(SYSTEMD_SERVICE_INSTALL_DIR)/$(SYSTEMD_SERVICE_FILE)"
	@$(PRINTF) "RM	%-40s %-40s\n" "$(CONFIG_INSTALL_DIR)/$(CONFIG_FILE)"
	@$(RM) "$(CONFIG_INSTALL_DIR)/$(CONFIG_FILE)"

$(EXE_INSTALL_DIR):
	@$(ECHO) "MKDIR	$(EXE_INSTALL_DIR)"
	@$(MKDIR) $(EXE_INSTALL_DIR)

$(SYSTEMD_SERVICE_INSTALL_DIR):
	@$(ECHO) "MKDIR	$(SYSTEMD_SERVICE_INSTALL_DIR)"
	@$(MKDIR) $(SYSTEMD_SERVICE_INSTALL_DIR)

$(CONFIG_INSTALL_DIR):
	@$(ECHO) "MKDIR $(CONFIG_INSTALL_DIR)"
	@$(MKDIR) $(CONFIG_INSTALL_DIR)

# Cleanup targets
.PHONY: mostlyclean
mostlyclean:
	@$(ECHO) "RM	$(_real_objs) $(DEPS) $(TEST_LOGFILE)"
	@$(RM) $(_real_objs) $(DEPS) $(TEST_LOGFILE)

.PHONY: clean
clean:
	@$(ECHO) "RM	$(_real_objs) $(DEPS) $(EXE) $(TEST_LIB) $(BINDIR) $(OBJDIR) $(TEST_EXES) $(TEST_BINDIR) $(TEST_LOGFILE)"
	@$(RM) $(_real_objs) $(DEPS) $(EXE) $(TEST_LIB) $(TEST_EXES) $(TEST_LOGFILE) assets/tests/asset_load_test/*.png
	@$(RMRF) $(OBJDIR) $(BINDIR) $(TEST_BINDIR)

# Output execution targets
.PHONY: run
run:
	@$(ECHO) "EXEC	$(EXE) $(EXEARGS)"
	@$(EXEC) $(EXE) $(EXEARGS)

.PHONY: debug-run
debug-run:
	@$(ECHO) "EXEC	$(EXE) $(EXEARGS)"
	@bash -c '$(EXEC) -a debug $(EXE) $(EXEARGS)'

.PHONY: bdr
.NOTPARALLEL: bdr
bdr: all debug-run

# Miscellaneous targets
.PHONY: strip
strip:
	@$(ECHO) "STRIP	$(EXE)"
	@$(STRIP) $(STRIPFLAGS) $(EXE)

.PHONY: update
update:
	@$(ECHO) "TOUCH	$(SRCS)"
	@$(TOUCH) $(SRCS)

-include $(DEPS)
