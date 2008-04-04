BIN = spu
OBJS = spu.o romdisk.o
EXTRA_CLEAN = romdisk.img

include $(TOPDIR)/Makefile.prefab

romdisk.o: romdisk.img
	$(KOS_PATH)/bin/bin2o romdisk.img romdisk romdisk.o

romdisk.img:
	$(KOS_GENROMFS) -f romdisk.img -d romdisk -v


