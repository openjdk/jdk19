/*
 * Copyright (c) 2020 SAP SE. All rights reserved.
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
#include "code/vmreg.inline.hpp"
#include "runtime/jniHandles.hpp"
#include "runtime/jniHandles.inline.hpp"
#include "oops/typeArrayOop.inline.hpp"
#include "oops/oopCast.inline.hpp"
#include "prims/foreignGlobals.hpp"
#include "prims/foreignGlobals.inline.hpp"
#include "utilities/formatBuffer.hpp"

bool ABIDescriptor::is_volatile_reg(Register reg) const {
  return _integer_argument_registers.contains(reg)
      || _integer_additional_volatile_registers.contains(reg);
}

bool ABIDescriptor::is_volatile_reg(FloatRegister reg) const {
    return _float_argument_registers.contains(reg)
        || _float_additional_volatile_registers.contains(reg);
}

static constexpr int INTEGER_TYPE = 0;
static constexpr int FLOAT_TYPE = 1;

// Stubbed out, implement later
const ABIDescriptor ForeignGlobals::parse_abi_descriptor(jobject jabi) {
  oop abi_oop = JNIHandles::resolve_non_null(jabi);
  ABIDescriptor abi;
  constexpr Register (*to_Register)(int) = as_Register;

  objArrayOop inputStorage = jdk_internal_foreign_abi_ABIDescriptor::inputStorage(abi_oop);
  parse_register_array(inputStorage, INTEGER_TYPE, abi._integer_argument_registers, to_Register);
  parse_register_array(inputStorage, FLOAT_TYPE, abi._float_argument_registers, as_FloatRegister);

  objArrayOop outputStorage = jdk_internal_foreign_abi_ABIDescriptor::outputStorage(abi_oop);
  parse_register_array(outputStorage, INTEGER_TYPE, abi._integer_return_registers, to_Register);
  parse_register_array(outputStorage, FLOAT_TYPE, abi._float_return_registers, as_FloatRegister);

  objArrayOop volatileStorage = jdk_internal_foreign_abi_ABIDescriptor::volatileStorage(abi_oop);
  parse_register_array(volatileStorage, INTEGER_TYPE, abi._integer_additional_volatile_registers, to_Register);
  parse_register_array(volatileStorage, FLOAT_TYPE, abi._float_additional_volatile_registers, as_FloatRegister);

  abi._stack_alignment_bytes = jdk_internal_foreign_abi_ABIDescriptor::stackAlignment(abi_oop);
  abi._shadow_space_bytes = jdk_internal_foreign_abi_ABIDescriptor::shadowSpace(abi_oop);

  abi._target_addr_reg = parse_vmstorage(jdk_internal_foreign_abi_ABIDescriptor::targetAddrStorage(abi_oop))->as_Register();
  abi._ret_buf_addr_reg = parse_vmstorage(jdk_internal_foreign_abi_ABIDescriptor::retBufAddrStorage(abi_oop))->as_Register();

  return abi;
}

enum class RegType {
  INTEGER = 0,
  FLOAT = 1,
  STACK = 3
};

VMReg ForeignGlobals::vmstorage_to_vmreg(int type, int index) {
  switch(static_cast<RegType>(type)) {
    case RegType::INTEGER: return ::as_Register(index)->as_VMReg();
    case RegType::FLOAT: return ::as_FloatRegister(index)->as_VMReg();
    case RegType::STACK: return VMRegImpl::stack2reg(index LP64_ONLY(* 2));
  }
  return VMRegImpl::Bad();
}

int RegSpiller::pd_reg_size(VMReg reg) {
  return 8;
}

void RegSpiller::pd_store_reg(MacroAssembler* masm, int offset, VMReg reg) {
  if (reg->is_Register()) {
    masm->std(reg->as_Register(), offset, R1_SP);
  } else if (reg->is_FloatRegister()) {
    masm->stfd(reg->as_FloatRegister(), offset, R1_SP);
  } else {
    // stack and BAD
  }
}

void RegSpiller::pd_load_reg(MacroAssembler* masm, int offset, VMReg reg) {
  if (reg->is_Register()) {
    masm->ld(reg->as_Register(), offset, R1_SP);
  } else if (reg->is_FloatRegister()) {
    masm->lfd(reg->as_FloatRegister(), offset, R1_SP);
  } else {
    // stack and BAD
  }
}

void ArgumentShuffle::pd_generate(MacroAssembler* masm, VMReg tmp, int in_stk_bias, int out_stk_bias) const {
  Register callerSP = tmp->as_Register(); // preset
  for (int i = 0; i < _moves.length(); i++) {
    Move move = _moves.at(i);
    BasicType arg_bt     = move.bt;
    VMRegPair from_vmreg = move.from;
    VMRegPair to_vmreg   = move.to;

    masm->block_comment(err_msg("bt=%s", null_safe_string(type2name(arg_bt))));
    switch (arg_bt) {
      case T_BOOLEAN:
      case T_BYTE:
      case T_SHORT:
      case T_CHAR:
      case T_INT:
        masm->int_move(from_vmreg, to_vmreg, callerSP, R0, in_stk_bias, out_stk_bias);
        break;

      case T_FLOAT:
        masm->float_move(from_vmreg, to_vmreg, callerSP, R0, in_stk_bias, out_stk_bias);
        break;

      case T_DOUBLE:
        masm->double_move(from_vmreg, to_vmreg, callerSP, R0, in_stk_bias, out_stk_bias);
        break;

      case T_LONG :
        masm->long_move(from_vmreg, to_vmreg, callerSP, R0, in_stk_bias, out_stk_bias);
        break;

      default:
        fatal("found in upcall args: %s", type2name(arg_bt));
    }
  }
}
