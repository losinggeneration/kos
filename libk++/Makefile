#
# libk++
#
# Simplified libstdc++ for KOS
#
# Makefile (c)2002 Dan Potter
#   

OBJS = mem.o pure_virtual.o
SUBDIRS =

myall: $(OBJS)
	rm -f $(TOPDIR)/lib/$(KOS_ARCH)/libk++.a
	$(KOS_AR) rcs $(TOPDIR)/lib/$(KOS_ARCH)/libk++.a $(OBJS)

include ../Makefile.prefab


