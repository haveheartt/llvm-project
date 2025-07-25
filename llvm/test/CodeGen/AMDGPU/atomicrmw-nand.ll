; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc -mtriple=amdgcn-amd-amdhsa -mcpu=gfx900 < %s | FileCheck -check-prefix=GCN %s

define i32 @atomic_nand_i32_lds(ptr addrspace(3) %ptr) nounwind {
; GCN-LABEL: atomic_nand_i32_lds:
; GCN:       ; %bb.0:
; GCN-NEXT:    s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
; GCN-NEXT:    ds_read_b32 v1, v0
; GCN-NEXT:    s_mov_b64 s[4:5], 0
; GCN-NEXT:  .LBB0_1: ; %atomicrmw.start
; GCN-NEXT:    ; =>This Inner Loop Header: Depth=1
; GCN-NEXT:    s_waitcnt lgkmcnt(0)
; GCN-NEXT:    v_mov_b32_e32 v2, v1
; GCN-NEXT:    v_not_b32_e32 v1, v2
; GCN-NEXT:    v_or_b32_e32 v1, -5, v1
; GCN-NEXT:    ds_cmpst_rtn_b32 v1, v0, v2, v1
; GCN-NEXT:    s_waitcnt lgkmcnt(0)
; GCN-NEXT:    v_cmp_eq_u32_e32 vcc, v1, v2
; GCN-NEXT:    s_or_b64 s[4:5], vcc, s[4:5]
; GCN-NEXT:    s_andn2_b64 exec, exec, s[4:5]
; GCN-NEXT:    s_cbranch_execnz .LBB0_1
; GCN-NEXT:  ; %bb.2: ; %atomicrmw.end
; GCN-NEXT:    s_or_b64 exec, exec, s[4:5]
; GCN-NEXT:    v_mov_b32_e32 v0, v1
; GCN-NEXT:    s_setpc_b64 s[30:31]
  %result = atomicrmw nand ptr addrspace(3) %ptr, i32 4 seq_cst
  ret i32 %result
}

define i32 @atomic_nand_i32_global(ptr addrspace(1) %ptr) nounwind {
; GCN-LABEL: atomic_nand_i32_global:
; GCN:       ; %bb.0:
; GCN-NEXT:    s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
; GCN-NEXT:    global_load_dword v2, v[0:1], off
; GCN-NEXT:    s_mov_b64 s[4:5], 0
; GCN-NEXT:  .LBB1_1: ; %atomicrmw.start
; GCN-NEXT:    ; =>This Inner Loop Header: Depth=1
; GCN-NEXT:    s_waitcnt vmcnt(0)
; GCN-NEXT:    v_mov_b32_e32 v3, v2
; GCN-NEXT:    v_not_b32_e32 v2, v3
; GCN-NEXT:    v_or_b32_e32 v2, -5, v2
; GCN-NEXT:    global_atomic_cmpswap v2, v[0:1], v[2:3], off glc
; GCN-NEXT:    s_waitcnt vmcnt(0)
; GCN-NEXT:    buffer_wbinvl1_vol
; GCN-NEXT:    v_cmp_eq_u32_e32 vcc, v2, v3
; GCN-NEXT:    s_or_b64 s[4:5], vcc, s[4:5]
; GCN-NEXT:    s_andn2_b64 exec, exec, s[4:5]
; GCN-NEXT:    s_cbranch_execnz .LBB1_1
; GCN-NEXT:  ; %bb.2: ; %atomicrmw.end
; GCN-NEXT:    s_or_b64 exec, exec, s[4:5]
; GCN-NEXT:    v_mov_b32_e32 v0, v2
; GCN-NEXT:    s_setpc_b64 s[30:31]
  %result = atomicrmw nand ptr addrspace(1) %ptr, i32 4 seq_cst
  ret i32 %result
}

define i32 @atomic_nand_i32_flat(ptr %ptr) nounwind {
; GCN-LABEL: atomic_nand_i32_flat:
; GCN:       ; %bb.0:
; GCN-NEXT:    s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
; GCN-NEXT:    flat_load_dword v2, v[0:1]
; GCN-NEXT:    s_mov_b64 s[4:5], 0
; GCN-NEXT:  .LBB2_1: ; %atomicrmw.start
; GCN-NEXT:    ; =>This Inner Loop Header: Depth=1
; GCN-NEXT:    s_waitcnt vmcnt(0) lgkmcnt(0)
; GCN-NEXT:    v_mov_b32_e32 v3, v2
; GCN-NEXT:    v_not_b32_e32 v2, v3
; GCN-NEXT:    v_or_b32_e32 v2, -5, v2
; GCN-NEXT:    flat_atomic_cmpswap v2, v[0:1], v[2:3] glc
; GCN-NEXT:    s_waitcnt vmcnt(0) lgkmcnt(0)
; GCN-NEXT:    buffer_wbinvl1_vol
; GCN-NEXT:    v_cmp_eq_u32_e32 vcc, v2, v3
; GCN-NEXT:    s_or_b64 s[4:5], vcc, s[4:5]
; GCN-NEXT:    s_andn2_b64 exec, exec, s[4:5]
; GCN-NEXT:    s_cbranch_execnz .LBB2_1
; GCN-NEXT:  ; %bb.2: ; %atomicrmw.end
; GCN-NEXT:    s_or_b64 exec, exec, s[4:5]
; GCN-NEXT:    v_mov_b32_e32 v0, v2
; GCN-NEXT:    s_setpc_b64 s[30:31]
  %result = atomicrmw nand ptr %ptr, i32 4 seq_cst
  ret i32 %result
}
