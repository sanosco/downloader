###############################################################################
#
#  common make file to build application
#
#  05.03.2018
#
###############################################################################

SRCDIR = src
BLDDIR = build
BINDIR = $(BLDDIR)/bin
OBJDIR = $(BLDDIR)/obj
DEPDIR = $(BLDDIR)/dep

include ./common-func.mk

######################################################
# Build file system tree for objects and dependencies
# corresponding to source file system tree
######################################################
SUBDIRS  = $(call enum-subdirs-in-depth,$(strip $(SRCDIR)))
OBJDIRS  = $(OBJDIR)
OBJDIRS += $(SUBDIRS:$(strip $(SRCDIR))/%=$(OBJDIR)/%)
DEPDIRS  = $(DEPDIR)
DEPDIRS += $(SUBDIRS:$(strip $(SRCDIR))/%=$(DEPDIR)/%)

######################################################
# Prepare list of directories those shall be created
######################################################
DIRS  = $(BINDIR)
DIRS += $(OBJDIRS)
DIRS += $(DEPDIRS)

SRCDIRS := $(SRCDIR) $(SUBDIRS)

SRCDIRS := $(call remove-duplicates-keep-first,$(SRCDIRS))
LIBDIRS := $(call remove-duplicates-keep-first,$(LIBDIRS))
INCLDIRS := $(call remove-duplicates-keep-first,$(INCLDIRS))

APPL = $(BINDIR)/$(APPLNAME)
SRCS = $(foreach srcdir,$(SRCDIRS),$(wildcard $(srcdir)/*.cc))
OBJS = $(SRCS:$(strip $(SRCDIR))/%.cc=$(OBJDIR)/%.o)
LIBS = $(addprefix -l,$(LIBNAMES))
LIBS := $(call remove-duplicates-libs,$(LIBS))

override CXXFLAGS += -std=c++17
override CXXFLAGS += $(addprefix -I,$(SRCDIRS))
override CXXFLAGS += $(addprefix -I,$(INCLDIRS))
override CXXFLAGS += -Wall
override CXXFLAGS += -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td

override LDFLAGS += $(addprefix -L,$(LIBDIRS))

ifdef DEBUG_BUILD
override CXXFLAGS += -O0 -g
else
override CXXFLAGS += -O3
endif

all : $(APPL)

$(DIRS) :
	mkdir -p $@

.PHONY : clean
clean :
	$(RM) -r $(BLDDIR)

$(OBJS) : $(strip $(OBJDIR))/%.o : $(strip $(SRCDIR))/%.cc $(strip $(DEPDIR))/%.d | $(DIRS)
	$(CXX) $(CXXFLAGS) -c $< -o $@
	mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d

$(APPL) : $(OBJS)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LIBS)

$(strip $(DEPDIR))/%.d: ;
.PRECIOUS: $(strip $(DEPDIR))/%.d

include $(foreach depdir,$(DEPDIRS),$(wildcard $(depdir)/*.d))

