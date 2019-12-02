.SUFFIXES:
.DEFAULT:

BUILDDIR ?= build
CC ?= cc
CFLAGS += -Wall -Wextra 
#-Werror
CFLAGS += -std=gnu99 -I/usr/include/libdrm 
CFLAGS += -ldrm -lGL -lEGL -lX11 -lcairo

ifeq ($(DEBUG), 1)
	CONFIG = dbg
	CFLAGS += -O0 -ggdb3
else
	CONFIG = rel
	CFLAGS += -O3
endif

DEPFLAGS = -MMD -MP
COMPILE.c = $(CC) -std=gnu99 $(CFLAGS) $(DEPFLAGS) -MT $@ -MF $@.d

OBJDIR ?= $(BUILDDIR)/$(CONFIG)

$(OBJDIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(COMPILE.c) -c $< -o $@
ENUM_SOURCES = egl_headless2.c
ENUM_OBJS = $(ENUM_SOURCES:%=$(OBJDIR)/%.o)
ENUM_DEPS = $(ENUM_OBJS:%=%.d)
-include $(ENUM_DEPS)
egl_headless2: $(OBJDIR)/egl_headless2
$(OBJDIR)/egl_headless2: $(ENUM_OBJS)
	@mkdir -p $(dir $@)
		$(CC) $^ $(LIBS) -o $@

ENUM_SOURCES = egl_headless.c
ENUM_OBJS = $(ENUM_SOURCES:%=$(OBJDIR)/%.o)
ENUM_DEPS = $(ENUM_OBJS:%=%.d)
-include $(ENUM_DEPS)
egl_headless: $(OBJDIR)/egl_headless
$(OBJDIR)/egl_headless: $(ENUM_OBJS)
	@mkdir -p $(dir $@)
		$(CC) $^ $(LIBS) -o $@

ENUM_SOURCES = fb_dump.c
ENUM_OBJS = $(ENUM_SOURCES:%=$(OBJDIR)/%.o)
ENUM_DEPS = $(ENUM_OBJS:%=%.d)
-include $(ENUM_DEPS)
fb_dump: $(OBJDIR)/fb_dump
$(OBJDIR)/fb_dump: $(ENUM_OBJS)
	@mkdir -p $(dir $@)
		$(CC) $^ $(LIBS) -o $@

ENUM_SOURCES = test-drm-prime-dumb-kms.c
ENUM_OBJS = $(ENUM_SOURCES:%=$(OBJDIR)/%.o)
ENUM_DEPS = $(ENUM_OBJS:%=%.d)
-include $(ENUM_DEPS)
test-drm-prime-dumb-kms: $(OBJDIR)/test-drm-prime-dumb-kms
$(OBJDIR)/test-drm-prime-dumb-kms: $(ENUM_OBJS)
	@mkdir -p $(dir $@)
		$(CC) $^ $(LIBS) -o $@

ENUM_SOURCES = prime-dumb-kms.c
ENUM_OBJS = $(ENUM_SOURCES:%=$(OBJDIR)/%.o)
ENUM_DEPS = $(ENUM_OBJS:%=%.d)
-include $(ENUM_DEPS)
prime-dumb-kms: $(OBJDIR)/prime-dumb-kms
$(OBJDIR)/prime-dumb-kms: $(ENUM_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ $(LIBS) -o $@

ENUM_SOURCES = enum.c
ENUM_OBJS = $(ENUM_SOURCES:%=$(OBJDIR)/%.o)
ENUM_DEPS = $(ENUM_OBJS:%=%.d)
-include $(ENUM_DEPS)
enum: $(OBJDIR)/enum
$(OBJDIR)/enum: $(ENUM_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ $(LIBS) -o $@

GRAB_SOURCES = grab.c
GRAB_OBJS = $(GRAB_SOURCES:%=$(OBJDIR)/%.o)
GRAB_DEPS = $(GRAB_OBJS:%=%.d)
-include $(GRAB_DEPS)
grab: $(OBJDIR)/grab
$(OBJDIR)/grab: $(GRAB_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ $(LIBS) -o $@

TEST_SOURCES = test2.c
TEST_OBJS = $(TEST_SOURCES:%=$(OBJDIR)/%.o)
TEST_DEPS = $(TEST_OBJS:%=%.d)
-include $(TEST_DEPS)
test2: $(OBJDIR)/test2
$(OBJDIR)/test2: $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ $(LIBS) -o $@


KMSGRAB_SOURCES = kmsgrab.c
KMSGRAB_OBJS = $(KMSGRAB_SOURCES:%=$(OBJDIR)/%.o)
KMSGRAB_DEPS = $(KMSGRAB_OBJS:%=%.d)
-include $(KMSGRAB_DEPS)
kmsgrab: $(OBJDIR)/kmsgrab
$(OBJDIR)/kmsgrab: $(KMSGRAB_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ $(LIBS) -o $@

DRMSEND_SOURCES = drmsend.c
DRMSEND_OBJS = $(DRMSEND_SOURCES:%=$(OBJDIR)/%.o)
DRMSEND_DEPS = $(DRMSEND_OBJS:%=%.d)
-include $(DRMSEND_DEPS)
drmsend: $(OBJDIR)/drmsend
$(OBJDIR)/drmsend: $(DRMSEND_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ $(LIBS) -o $@
