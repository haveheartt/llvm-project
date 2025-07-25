// Check for --eh-frame-hdr being passed with static linking
// RUN: %clang --target=i686-pc-openbsd -static -### %s 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-LD-STATIC-EH %s
// CHECK-LD-STATIC-EH: "-cc1" "-triple" "i686-pc-openbsd"
// CHECK-LD-STATIC-EH: ld{{.*}}" "{{.*}}" "--eh-frame-hdr" "-Bstatic"

// Check for profiling variants of libraries when linking and -nopie
// RUN: %clang --target=i686-pc-openbsd -pg -pthread -### %s 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-PG %s
// CHECK-PG: "-cc1" "-triple" "i686-pc-openbsd"
// CHECK-PG: ld{{.*}}" "-e" "__start" "--eh-frame-hdr" "-dynamic-linker" "{{.*}}ld.so" "-nopie" "-o" "a.out" "{{.*}}gcrt0.o" "{{.*}}crtbegin.o" "{{.*}}.o" "-lcompiler_rt" "-lpthread_p" "-lc_p" "-lcompiler_rt" "{{.*}}crtend.o"

// Check for variants of crt* when creating shared libs
// RUN: %clang --target=i686-pc-openbsd -pthread -shared -### %s 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-SHARED %s
// CHECK-SHARED: "-cc1" "-triple" "i686-pc-openbsd"
// CHECK-SHARED: ld{{.*}}" "--eh-frame-hdr" "-shared" "-o" "a.out" "{{.*}}crtbeginS.o" "{{.*}}.o" "-lcompiler_rt" "-lpthread" "-lcompiler_rt" "{{.*}}crtendS.o"

// Check CPU type for i386
// RUN: %clang --target=i386-unknown-openbsd -### -c %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-i386-CPU %s
// CHECK-i386-CPU: "-target-cpu" "i586"

// Check CPU type for MIPS64
// RUN: %clang --target=mips64-unknown-openbsd -### -c %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-MIPS64-CPU %s
// RUN: %clang --target=mips64el-unknown-openbsd -### -c %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-MIPS64EL-CPU %s
// CHECK-MIPS64-CPU: "-target-cpu" "mips3"
// CHECK-MIPS64EL-CPU: "-target-cpu" "mips3"

// Check that the new linker flags are passed to OpenBSD
// RUN: %clang --target=i686-pc-openbsd -r -### %s 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-LD-R %s
// RUN: %clang --target=i686-pc-openbsd -s -t -### %s 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-LD-ST %s
// RUN: %clang --target=mips64-unknown-openbsd -### %s 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-MIPS64-LD %s
// RUN: %clang --target=mips64el-unknown-openbsd -### %s 2>&1 \
// RUN:   | FileCheck --check-prefix=CHECK-MIPS64EL-LD %s
// CHECK-LD-R-NOT: "-e"
// CHECK-LD-R-NOT: "-dynamic-linker"
// CHECK-LD-R-NOT: "-l
// CHECK-LD-R-NOT: crt{{[^./\\]+}}.o
// CHECK-LD-R:     "-r"
// CHECK-LD-ST: "-cc1" "-triple" "i686-pc-openbsd"
// CHECK-LD-ST: ld{{.*}}" "{{.*}}" "-s" "-t"
// CHECK-MIPS64-LD: "-cc1" "-triple" "mips64-unknown-openbsd"
// CHECK-MIPS64-LD: ld{{.*}}" "-EB"
// CHECK-MIPS64EL-LD: "-cc1" "-triple" "mips64el-unknown-openbsd"
// CHECK-MIPS64EL-LD: ld{{.*}}" "-EL"

// Check that --sysroot is passed to the linker
// RUN: %clang --target=i686-pc-openbsd -### %s 2>&1 \
// RUN:   --sysroot=%S/Inputs/basic_openbsd_tree \
// RUN:   | FileCheck --check-prefix=CHECK-LD-SYSROOT %s
// CHECK-LD-SYSROOT: ld{{.*}}" "--sysroot=[[SYSROOT:[^"]+]]"

// Check passing options to the assembler for various OpenBSD targets
// RUN: %clang --target=amd64-pc-openbsd -m32 -### -no-integrated-as -c %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-AMD64-M32 %s
// RUN: %clang --target=arm-unknown-openbsd -### -no-integrated-as -c %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-ARM %s
// RUN: %clang --target=powerpc-unknown-openbsd -### -no-integrated-as -c %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-POWERPC %s
// RUN: %clang --target=sparc64-unknown-openbsd -### -no-integrated-as -c %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-SPARC64 %s
// RUN: %clang --target=mips64-unknown-openbsd -### -no-integrated-as -c %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-MIPS64 %s
// RUN: %clang --target=mips64-unknown-openbsd -fPIC -### -no-integrated-as -c %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-MIPS64-PIC %s
// RUN: %clang --target=mips64el-unknown-openbsd -### -no-integrated-as -c %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-MIPS64EL %s
// RUN: %clang --target=mips64el-unknown-openbsd -fPIC -### -no-integrated-as -c %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-MIPS64EL-PIC %s
// CHECK-AMD64-M32: as{{.*}}" "--32"
// CHECK-ARM: as{{.*}}" "-mcpu=cortex-a8"
// CHECK-POWERPC: as{{.*}}" "-mppc" "-many"
// CHECK-SPARC64: as{{.*}}" "-64" "-Av9a"
// CHECK-MIPS64: as{{.*}}" "-march" "mips3" "-mabi" "64" "-EB"
// CHECK-MIPS64-PIC: as{{.*}}" "-march" "mips3" "-mabi" "64" "-EB" "-KPIC"
// CHECK-MIPS64EL: as{{.*}}" "-mabi" "64" "-EL"
// CHECK-MIPS64EL-PIC: as{{.*}}" "-mabi" "64" "-EL" "-KPIC"

// Check linking against correct startup code when (not) using PIE
// RUN: %clang --target=i686-pc-openbsd -### %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-PIE %s
// RUN: %clang --target=i686-pc-openbsd -pie -### %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-PIE-FLAG %s
// RUN: %clang --target=i686-pc-openbsd -fno-pie -### %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-PIE %s
// RUN: %clang --target=i686-pc-openbsd -static -### %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-STATIC-PIE %s
// RUN: %clang --target=i686-pc-openbsd -static -fno-pie -### %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-STATIC-PIE %s
// RUN: %clang --target=i686-pc-openbsd -nopie -### %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-NOPIE %s
// RUN: %clang --target=i686-pc-openbsd -fno-pie -nopie -### %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-NOPIE %s
// RUN: %clang --target=i686-pc-openbsd -static -nopie -### %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-NOPIE %s
// RUN: %clang --target=i686-pc-openbsd -fno-pie -static -nopie -### %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-NOPIE %s
// CHECK-PIE: "{{.*}}crt0.o"
// CHECK-PIE-NOT: "-nopie"
// CHECK-PIE-FLAG: "-pie"
// CHECK-STATIC-PIE: "{{.*}}rcrt0.o"
// CHECK-STATIC-PIE-NOT: "-nopie"
// CHECK-NOPIE: "-nopie" "{{.*}}crt0.o"

// Check ARM float ABI
// RUN: %clang --target=arm-unknown-openbsd -### -c %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-ARM-FLOAT-ABI %s
// CHECK-ARM-FLOAT-ABI-NOT: "-target-feature" "+soft-float"
// CHECK-ARM-FLOAT-ABI: "-target-feature" "+soft-float-abi"

// Check PowerPC for Secure PLT
// RUN: %clang --target=powerpc-unknown-openbsd -### -c %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-POWERPC-SECUREPLT %s
// CHECK-POWERPC-SECUREPLT: "-target-feature" "+secure-plt"

// Check that unwind tables are enabled
// RUN: %clang --target=arm-unknown-openbsd -### -S %s 2>&1 | \
// RUN: FileCheck -check-prefix=NO-UNWIND-TABLES %s
// RUN: %clang --target=mips64-unknown-openbsd -### -S %s 2>&1 | \
// RUN: FileCheck -check-prefix=UNWIND-TABLES %s
// UNWIND-TABLES: "-funwind-tables=2"
// NO-UNWIND-TABLES-NOT: "-funwind-tables=2"

// Check that the -X and --no-relax flags are passed to the linker
// RUN: %clang --target=loongarch64-unknown-openbsd -mno-relax -### %s 2>&1 \
// RUN:   | FileCheck --check-prefix=LA64-FLAGS %s
// RUN: %clang --target=riscv64-unknown-openbsd -mno-relax -### %s 2>&1 \
// RUN:   | FileCheck -check-prefix=RISCV64-FLAGS %s
// LA64-FLAGS: "-X" "--no-relax"
// RISCV64-FLAGS: "-X" "--no-relax"

// Check passing LTO flags to the linker
// RUN: %clang --target=amd64-unknown-openbsd -flto -### %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-LTO-FLAGS %s
// CHECK-LTO-FLAGS: "-plugin-opt=mcpu=x86-64"

// Check 64-bit ARM for BTI and PAC flags
// RUN: %clang --target=aarch64-unknown-openbsd -### -c %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-AARCH64-BTI-PAC %s
// CHECK-AARCH64-BTI-PAC: "-msign-return-address=non-leaf" "-msign-return-address-key=a_key" "-mbranch-target-enforce"

// Check 64-bit X86 for IBT flags
// RUN: %clang --target=amd64-unknown-openbsd -### -c %s 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-AMD64-IBT %s
// CHECK-AMD64-IBT: "-fcf-protection=branch" "-fno-jump-tables"
