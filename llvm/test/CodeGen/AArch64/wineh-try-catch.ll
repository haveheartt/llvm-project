; RUN: llc -o - %s -mtriple=aarch64-windows -verify-machineinstrs | FileCheck %s
; RUN: llc -o %t -filetype=obj %s -mtriple=aarch64-windows
; RUN: llvm-readobj --unwind %t | FileCheck %s -check-prefix=UNWIND

; We test the following
; 1) That the unwind help object is created and that its offset from the stack
;    pointer on entry is patched into the table fed to __CxxFrameHandler3
; 2) That the stack update for the catch funclet only includes the callee saved
;    registers
; 3) That the locals are accessed using the frame pointer in both the funclet
;    and the parent function.

; The following checks that the unwind help object has -2 stored into it at
; fp + 16, which is on-entry sp - 16.
; We check this offset in the table later on.

; CHECK-LABEL: "?func@@YAHXZ":
; CHECK:       stp     x19, x20, [sp, #-64]!
; CHECK:       str     x21, [sp, #16]
; CHECK:       str     x28, [sp, #24]
; CHECK:       stp     x29, x30, [sp, #32]
; CHECK:       add     x29, sp, #32
; CHECK:       sub     sp, sp, #608
; CHECK:       mov     x19, sp
; CHECK:       mov     x0, #-2
; CHECK:       stur    x0, [x29, #16]

; Now check that x is stored at fp - 20.  We check that this is the same
; location accessed from the funclet to retrieve x.
; CHECK:       mov     w8, #1
; CHECK:       stur    w8, [x29, [[X_OFFSET:#-[1-9][0-9]+]]

; Check the offset off the frame pointer at which B is located.
; Check the same offset is used to pass the address of B to init2 in the
; funclet.
; CHECK:       sub     x0, x29, [[B_OFFSET:#[1-9][0-9]+]]
; CHECK:       bl      "?init@@YAXPEAH@Z"

; This is the label for the throw that is encoded in the ip2state.
; We are inside the try block, where we make a call to func2
; CHECK-LABEL: .Ltmp0:
; CHECK:       bl      "?func2@@YAHXZ

; CHECK:        [[CATCHRETDEST:.LBB0_[0-9]+]]:      // Block address taken

; Check the catch funclet.
; CHECK-LABEL: "?catch$4@?0??func@@YAHXZ@4HA":

; Check that the stack space is allocated only for the callee saved registers.
; CHECK:       stp     x19, x20, [sp, #-48]!
; CHECK:       str     x21, [sp, #16]
; CHECK:       str     x28, [sp, #24]
; CHECK:       stp     x29, x30, [sp, #32]
; CHECK:       add     x20, x19, #0

; Check that there are no further stack updates.
; CHECK-NOT:   sub     sp, sp

; Check that the stack address passed to init2 is off the frame pointer, and
; that it matches the address of B in the parent function.
; CHECK:       sub     x0, x29, [[B_OFFSET]]
; CHECK:       bl      "?init2@@YAXPEAH@Z"

; Check that are storing x back to the same location off the frame pointer as in
; the parent function.
; CHECK:       stur    w8, [x29, [[X_OFFSET]]]

; Check that the funclet branches back to the catchret destination
; CHECK:       adrp    x0, .LBB0_2
; CHECK-NEXT:  add     x0, x0, [[CATCHRETDEST]]


; Now check that the offset of the unwind help object from the stack pointer on
; entry to func is encoded in cppxdata that is passed to __CxxFrameHandler3.  As
; computed above, this comes to -16.
; CHECK-LABEL:        "$cppxdata$?func@@YAHXZ":
; CHECK-NEXT:         .word   429065506               // MagicNumber
; CHECK-NEXT:         .word   2                       // MaxState
; CHECK-NEXT:         .word   "$stateUnwindMap$?func@@YAHXZ"@IMGREL // UnwindMap
; CHECK-NEXT:         .word   1                       // NumTryBlocks
; CHECK-NEXT:         .word   "$tryMap$?func@@YAHXZ"@IMGREL // TryBlockMap
; CHECK-NEXT:         .word   4                       // IPMapEntries
; CHECK-NEXT:         .word   "$ip2state$?func@@YAHXZ"@IMGREL // IPToStateXData
; CHECK-NEXT:         .word   -16                     // UnwindHelp

; UNWIND: Function: ?func@@YAHXZ (0x0)
; UNWIND: Prologue [
; UNWIND-NEXT: ; add fp, sp, #32
; UNWIND-NEXT: ; stp x29, x30, [sp, #32]
; UNWIND-NEXT: ; str x28, [sp, #24]
; UNWIND-NEXT: ; str x21, [sp, #16]
; UNWIND-NEXT: ; stp x19, x20, [sp, #-64]!
; UNWIND-NEXT: ; end
; UNWIND: Function: ?catch$4@?0??func@@YAHXZ@4HA
; UNWIND: Prologue [
; UNWIND-NEXT: ; stp x29, x30, [sp, #32]
; UNWIND-NEXT: ; str x28, [sp, #24]
; UNWIND-NEXT: ; str x21, [sp, #16]
; UNWIND-NEXT: ; stp x19, x20, [sp, #-48]!
; UNWIND-NEXT: ; end

target datalayout = "e-m:w-p:64:64-i32:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-unknown-windows-msvc19.11.0"

%rtti.TypeDescriptor2 = type { ptr, ptr, [3 x i8] }
%eh.CatchableType = type { i32, i32, i32, i32, i32, i32, i32 }
%eh.CatchableTypeArray.1 = type { i32, [1 x i32] }
%eh.ThrowInfo = type { i32, i32, i32, i32 }

$"??_R0H@8" = comdat any

$"_CT??_R0H@84" = comdat any

$_CTA1H = comdat any

$_TI1H = comdat any

@"??_7type_info@@6B@" = external constant ptr
@"??_R0H@8" = linkonce_odr global %rtti.TypeDescriptor2 { ptr @"??_7type_info@@6B@", ptr null, [3 x i8] c".H\00" }, comdat
@__ImageBase = external dso_local constant i8
@"_CT??_R0H@84" = linkonce_odr unnamed_addr constant %eh.CatchableType { i32 1, i32 trunc (i64 sub nuw nsw (i64 ptrtoint (ptr @"??_R0H@8" to i64), i64 ptrtoint (ptr @__ImageBase to i64)) to i32), i32 0, i32 -1, i32 0, i32 4, i32 0 }, section ".xdata", comdat
@_CTA1H = linkonce_odr unnamed_addr constant %eh.CatchableTypeArray.1 { i32 1, [1 x i32] [i32 trunc (i64 sub nuw nsw (i64 ptrtoint (ptr @"_CT??_R0H@84" to i64), i64 ptrtoint (ptr @__ImageBase to i64)) to i32)] }, section ".xdata", comdat
@_TI1H = linkonce_odr unnamed_addr constant %eh.ThrowInfo { i32 0, i32 0, i32 0, i32 trunc (i64 sub nuw nsw (i64 ptrtoint (ptr @_CTA1H to i64), i64 ptrtoint (ptr @__ImageBase to i64)) to i32) }, section ".xdata", comdat

; Function Attrs: noinline optnone
define dso_local i32 @"?func@@YAHXZ"() #0 personality ptr @__CxxFrameHandler3 {
entry:
  %B = alloca [50 x i32], align 4
  %x = alloca i32, align 4
  %tmp = alloca i32, align 4
  %i = alloca i32, align 4
  %C = alloca [100 x i32], align 4
  store i32 1, ptr %x, align 4
  call void @"?init@@YAXPEAH@Z"(ptr %B)
  %call = invoke i32 @"?func2@@YAHXZ"()
          to label %invoke.cont unwind label %catch.dispatch

invoke.cont:                                      ; preds = %entry
  store i32 %call, ptr %tmp, align 4
  invoke void @_CxxThrowException(ptr %tmp, ptr @_TI1H) #2
          to label %unreachable unwind label %catch.dispatch

catch.dispatch:                                   ; preds = %invoke.cont, %entry
  %0 = catchswitch within none [label %catch] unwind to caller

catch:                                            ; preds = %catch.dispatch
  %1 = catchpad within %0 [ptr @"??_R0H@8", i32 0, ptr %i]
  call void @"?init@@YAXPEAH@Z"(ptr %C) [ "funclet"(token %1) ]
  call void @"?init2@@YAXPEAH@Z"(ptr %B) [ "funclet"(token %1) ]
  %2 = load i32, ptr %i, align 4
  %idxprom = sext i32 %2 to i64
  %arrayidx = getelementptr inbounds [50 x i32], ptr %B, i64 0, i64 %idxprom
  %3 = load i32, ptr %arrayidx, align 4
  %4 = load i32, ptr %i, align 4
  %idxprom3 = sext i32 %4 to i64
  %arrayidx4 = getelementptr inbounds [100 x i32], ptr %C, i64 0, i64 %idxprom3
  %5 = load i32, ptr %arrayidx4, align 4
  %add = add nsw i32 %3, %5
  %6 = load i32, ptr %i, align 4
  %7 = load i32, ptr %i, align 4
  %mul = mul nsw i32 %6, %7
  %add5 = add nsw i32 %add, %mul
  store i32 %add5, ptr %x, align 4
  catchret from %1 to label %catchret.dest

catchret.dest:                                    ; preds = %catch
  br label %try.cont

try.cont:                                         ; preds = %catchret.dest
  %arrayidx6 = getelementptr inbounds [50 x i32], ptr %B, i64 0, i64 2
  %8 = load i32, ptr %arrayidx6, align 4
  %9 = load i32, ptr %x, align 4
  %add7 = add nsw i32 %8, %9
  ret i32 %add7

unreachable:                                      ; preds = %invoke.cont
  unreachable
}

declare dso_local void @"?init@@YAXPEAH@Z"(ptr)

declare dso_local i32 @"?func2@@YAHXZ"()

declare dso_local i32 @__CxxFrameHandler3(...)

declare dllimport void @_CxxThrowException(ptr, ptr)

declare dso_local void @"?init2@@YAXPEAH@Z"(ptr)

attributes #0 = { noinline optnone }
attributes #2 = { noreturn }
