# KallistiOS environment variable settings. These are the shared pieces
# that are generated from the user config. Configure if you like.

# Our includes
export KOS_INC_PATHS="${KOS_INC_PATHS} -I${KOS_BASE}/include -I${KOS_BASE}/libc/include \
-I${KOS_BASE}/kernel/arch/${KOS_ARCH}/include -I${KOS_BASE}/addons/include"

export KOS_INC_PATHS_CPP="${KOS_INC_PATHS_CPP} -I${KOS_BASE}/libk++/stlport"

# "System" libraries
export KOS_LIB_PATHS="-L${KOS_BASE}/lib/${KOS_ARCH} -L${KOS_BASE}/addons/lib/${KOS_ARCH}"
export KOS_LIBS="-lkallisti -lgcc"

# Main arch compiler paths
export KOS_CC="${KOS_CC_BASE}/bin/${KOS_CC_PREFIX}-gcc"
export KOS_CCPLUS="${KOS_CC_BASE}/bin/${KOS_CC_PREFIX}-g++"
export KOS_AS="${KOS_CC_BASE}/bin/${KOS_CC_PREFIX}-as"
export KOS_AR="${KOS_CC_BASE}/bin/${KOS_CC_PREFIX}-ar"
export KOS_OBJCOPY="${KOS_CC_BASE}/bin/${KOS_CC_PREFIX}-objcopy"
export KOS_LD="${KOS_CC_BASE}/bin/${KOS_CC_PREFIX}-ld"
export KOS_STRIP="${KOS_CC_BASE}/bin/${KOS_CC_PREFIX}-strip"
export KOS_CFLAGS="${KOS_INC_PATHS} -D_arch_${KOS_ARCH} -D_arch_sub_${KOS_SUBARCH} -Wall -g -ml -m4-single-only -O2 -fno-builtin -fno-strict-aliasing -fomit-frame-pointer -fno-optimize-sibling-calls"
# export KOS_CFLAGS="-D_arch_${KOS_ARCH} -D_arch_sub_${KOS_SUBARCH} -Wall -g -ml -m4-single-only -O2 -fno-builtin -fno-strict-aliasing -DFRAME_POINTERS -fno-optimize-sibling-calls"
export KOS_CPPFLAGS="${KOS_INC_PATHS_CPP} -fno-operator-names -fno-rtti -fno-exceptions"
export KOS_AFLAGS="-little"
export KOS_LDFLAGS="-ml -m4-single-only -nostartfiles -nostdlib -Wl,-Ttext=0x8c010000 ${KOS_LIB_PATHS}"

# Some extra vars based on architecture
export KOS_ARCH_DIR="${KOS_BASE}/kernel/arch/${KOS_ARCH}"
export KOS_START="${KOS_ARCH_DIR}/kernel/startup.o"

# If we're building for DC, we need the ARM compiler paths as well.
if [ x${KOS_ARCH} = xdreamcast ]; then
	export DC_ARM_CC="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-gcc"
	export DC_ARM_AS="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-as"
	export DC_ARM_AR="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-ar"
	export DC_ARM_OBJCOPY="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-objcopy"
	export DC_ARM_LD="${DC_ARM_BASE}/bin/${DC_ARM_PREFIX}-ld"
	export DC_ARM_CFLAGS="-mcpu=arm7 -Wall -O2"
	export DC_ARM_AFLAGS="-marm7"
fi
