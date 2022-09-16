/*
 * Copyright (c) 2022 SAP SE. All rights reserved.
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include "precompiled.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "code/codeBlob.hpp"
#include "code/codeCache.hpp"
#include "code/vmreg.inline.hpp"
#include "compiler/oopMap.hpp"
#include "compiler/disassembler.hpp"
#include "logging/logStream.hpp"
#include "memory/resourceArea.hpp"
#include "prims/downcallLinker.hpp"
#include "register_ppc.hpp"
#include "runtime/stubCodeGenerator.hpp"

#define __ _masm->

class DowncallStubGenerator : public StubCodeGenerator {
  BasicType* _signature;
  int _num_args;
  BasicType _ret_bt;
  const ABIDescriptor& _abi;

  const GrowableArray<VMReg>& _input_registers;
  const GrowableArray<VMReg>& _output_registers;

  bool _needs_return_buffer;

  int _frame_complete;
  int _framesize;
  OopMapSet* _oop_maps;
public:
  DowncallStubGenerator(CodeBuffer* buffer,
                         BasicType* signature,
                         int num_args,
                         BasicType ret_bt,
                         const ABIDescriptor& abi,
                         const GrowableArray<VMReg>& input_registers,
                         const GrowableArray<VMReg>& output_registers,
                         bool needs_return_buffer)
   : StubCodeGenerator(buffer, PrintMethodHandleStubs),
     _signature(signature),
     _num_args(num_args),
     _ret_bt(ret_bt),
     _abi(abi),
     _input_registers(input_registers),
     _output_registers(output_registers),
     _needs_return_buffer(needs_return_buffer),
     _frame_complete(0),
     _framesize(0),
     _oop_maps(NULL) {
  }

  void generate();

  int frame_complete() const {
    return _frame_complete;
  }

  int framesize() const {
    return (_framesize >> (LogBytesPerWord - LogBytesPerInt));
  }

  OopMapSet* oop_maps() const {
    return _oop_maps;
  }
};

static const int native_invoker_code_size = 1024;

RuntimeStub* DowncallLinker::make_downcall_stub(BasicType* signature,
                                                int num_args,
                                                BasicType ret_bt,
                                                const ABIDescriptor& abi,
                                                const GrowableArray<VMReg>& input_registers,
                                                const GrowableArray<VMReg>& output_registers,
                                                bool needs_return_buffer) {
  int locs_size  = 64;
  CodeBuffer code("nep_invoker_blob", native_invoker_code_size, locs_size);
  DowncallStubGenerator g(&code, signature, num_args, ret_bt, abi, input_registers, output_registers, needs_return_buffer);
  g.generate();
  code.log_section_sizes("nep_invoker_blob");

  RuntimeStub* stub =
    RuntimeStub::new_runtime_stub("nep_invoker_blob",
                                  &code,
                                  g.frame_complete(),
                                  g.framesize(),
                                  g.oop_maps(), false);

#ifndef PRODUCT
  LogTarget(Trace, foreign, downcall) lt;
  if (lt.is_enabled()) {
    ResourceMark rm;
    LogStream ls(lt);
    stub->print_on(&ls);
  }
#endif

  return stub;
}

void DowncallStubGenerator::generate() {
  Register tmp = R11_scratch1,
           shuffle_reg = tmp;
  JavaCallingConvention in_conv;
  NativeCallingConvention out_conv(_input_registers);
  ArgumentShuffle arg_shuffle(_signature, _num_args, _signature, _num_args, &in_conv, &out_conv, shuffle_reg->as_VMReg());

#ifndef PRODUCT
  LogTarget(Trace, foreign, downcall) lt;
  if (lt.is_enabled()) {
    ResourceMark rm;
    LogStream ls(lt);
    arg_shuffle.print_on(&ls);
  }
#endif

  int allocated_frame_size = frame::abi_reg_args_size;
  if (_needs_return_buffer) {
    ShouldNotReachHere();
    //allocated_frame_size += 8; // for address spill
  }
  allocated_frame_size += arg_shuffle.out_arg_stack_slots() << LogBytesPerInt;
  assert(_abi._shadow_space_bytes == frame::abi_reg_args_size, "expected space according to ABI");

  int ret_buf_addr_sp_offset = -1;
  //if (_needs_return_buffer) {
  //   // in sync with the above
  //   ret_buf_addr_sp_offset = allocated_frame_size - 8;
  //}

  RegSpiller out_reg_spiller(_output_registers);
  int spill_offset = frame::abi_reg_args_size;

  if (!_needs_return_buffer) {
    // spill area can be shared with the above, so we take the max of the 2
    allocated_frame_size = MAX2(out_reg_spiller.spill_size_bytes() + frame::abi_reg_args_size,
                                allocated_frame_size);
  }

  allocated_frame_size = align_up(allocated_frame_size, frame::alignment_in_bytes);
  _framesize = allocated_frame_size >> LogBytesPerInt; // In 4 Byte stack slots.

  _oop_maps = new OopMapSet();
  address start = __ pc();

  __ save_LR_CR(tmp); // Save in old frame.
  __ push_frame(allocated_frame_size, tmp);

  _frame_complete = __ pc() - start;

  address the_pc = __ pc();
  __ calculate_address_from_global_toc(tmp, the_pc, true, true, true, true);
  __ set_last_Java_frame(R1_SP, tmp);

  // State transition
  __ li(tmp, _thread_in_native);
  __ release();
  __ stw(tmp, in_bytes(JavaThread::thread_state_offset()), R16_thread);

  __ block_comment("{ argument shuffle");
  // TODO: Check if in_stk_bias is always correct (interpreter / JIT)?
  arg_shuffle.generate(_masm, shuffle_reg->as_VMReg(), frame::jit_out_preserve_size, _abi._shadow_space_bytes);
  //if (_needs_return_buffer) {
  //  assert(ret_buf_addr_sp_offset != -1, "no return buffer addr spill");
  //  __ std(_abi._ret_buf_addr_reg, ret_buf_addr_sp_offset, R1_SP);
  //}
  __ block_comment("} argument shuffle");

  __ mtctr(_abi._target_addr_reg);
  __ bctrl();
  int return_add_offs = __ pc() - start;
  OopMap* map = new OopMap(_framesize, 0);
  _oop_maps->add_gc_map(return_add_offs, map);

  if (!_needs_return_buffer) {
    // Unpack native results.
    switch (_ret_bt) {
      case T_BOOLEAN: // convert !=0 to 1
                      __ neg(tmp, R3_RET);
                      __ orr(tmp, R3_RET, tmp);
                      __ srwi(R3_RET, tmp, 31);       break;
      case T_CHAR   : __ clrldi(R3_RET, R3_RET, 48);  break;
      case T_BYTE   : __ extsb(R3_RET, R3_RET);       break;
      case T_SHORT  : __ extsh(R3_RET, R3_RET);       break;
      case T_INT    : __ extsw(R3_RET, R3_RET);       break;
      case T_DOUBLE :
      case T_FLOAT  :
        // Result is in F1
        break;
      case T_VOID: break;
      case T_LONG: break;
      default       : ShouldNotReachHere();
    }
  }
  //else {
  //  assert(ret_buf_addr_sp_offset != -1, "no return buffer addr spill");
  //  __ ld(tmp, ret_buf_addr_sp_offset, R1_SP);
  //  int offset = 0;
  //  for (int i = 0; i < _output_registers.length(); i++) {
  //    VMReg reg = _output_registers.at(i);
  //    if (reg->is_Register()) {
  //      __ std(reg->as_Register(), offset, tmp);
  //      offset += 8;
  //    } else if(reg->is_FloatRegister()) {
  //      __ stfd(reg->as_FloatRegister(), offset, tmp);
  //      offset += 8;
  //    } else {
  //      ShouldNotReachHere();
  //    }
  //  }
  //}

  // State transition
  __ li(tmp, _thread_in_native_trans);
  __ release();
  __ stw(tmp, in_bytes(JavaThread::thread_state_offset()), R16_thread);
  // TODO: JDK 20 supports UseSystemMemoryBarrier
  __ fence(); // Order state change wrt. safepoint poll.

  Label L_after_safepoint_poll;
  Label L_safepoint_poll_slow_path;

  __ safepoint_poll(L_safepoint_poll_slow_path, tmp, true /* at_return */, false /* in_nmethod */);

  __ lwz(tmp, in_bytes(JavaThread::suspend_flags_offset()), R16_thread);
  __ cmpwi(CCR0, tmp, 0);
  __ bne(CCR0, L_safepoint_poll_slow_path);
  __ bind(L_after_safepoint_poll);

  // change thread state
  // State transition
  __ li(tmp, _thread_in_Java);
  __ lwsync(); // Acquire safepoint and suspend state, release thread state.
  __ stw(tmp, in_bytes(JavaThread::thread_state_offset()), R16_thread);

  __ block_comment("reguard stack check");
  Label L_reguard;
  Label L_after_reguard;
  __ lwz(tmp, in_bytes(JavaThread::stack_guard_state_offset()), R16_thread);
  __ cmpwi(CCR0, tmp, StackOverflow::stack_guard_yellow_reserved_disabled);
  __ beq(CCR0, L_reguard);
  __ bind(L_after_reguard);

  __ reset_last_Java_frame();

  __ pop_frame();
  __ restore_LR_CR(tmp);
  __ blr();

  //////////////////////////////////////////////////////////////////////////////

  __ block_comment("{ L_safepoint_poll_slow_path");
  __ bind(L_safepoint_poll_slow_path);

  if (!_needs_return_buffer) {
    // Need to save the native result registers around any runtime calls.
    out_reg_spiller.generate_spill(_masm, spill_offset);
  }

  __ untested("DowncallStub: trans");
  address trans_entry_point = CAST_FROM_FN_PTR(address, JavaThread::check_special_condition_for_native_trans);
  //assert(frame::arg_reg_save_area_bytes == 0, "not expecting frame reg save area");
  __ call_VM_leaf(trans_entry_point, R16_thread);

  if (!_needs_return_buffer) {
    out_reg_spiller.generate_fill(_masm, spill_offset);
  }

  __ b(L_after_safepoint_poll);
  __ block_comment("} L_safepoint_poll_slow_path");

  //////////////////////////////////////////////////////////////////////////////

  __ block_comment("{ L_reguard");
  __ bind(L_reguard);

  if (!_needs_return_buffer) {
    out_reg_spiller.generate_spill(_masm, spill_offset);
  }

  __ untested("DowncallStub: reguard");
  address reguard_entry_point = CAST_FROM_FN_PTR(address, SharedRuntime::reguard_yellow_pages);
  __ call_VM_leaf(reguard_entry_point);

  if (!_needs_return_buffer) {
    out_reg_spiller.generate_fill(_masm, spill_offset);
  }

  __ b(L_after_reguard);
  __ block_comment("} L_reguard");

  //////////////////////////////////////////////////////////////////////////////

  __ flush();
  // Disassembler::decode((u_char*)start, (u_char*)__ pc(), tty);
}
