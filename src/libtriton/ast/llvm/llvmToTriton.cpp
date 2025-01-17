//! \file
/*
**  Copyright (C) - Triton
**
**  This program is under the terms of the Apache License 2.0.
*/

#include <triton/exceptions.hpp>
#include <triton/llvmToTriton.hpp>



namespace triton {
  namespace ast {

    LLVMToTriton::LLVMToTriton(const SharedAstContext& actx)
      : actx(actx) {
    }


    triton::ast::SharedAbstractNode LLVMToTriton::do_convert(llvm::Value* value) {
      llvm::Argument* argument       = llvm::dyn_cast_or_null<llvm::Argument>(value);
      llvm::CallInst* call           = llvm::dyn_cast_or_null<llvm::CallInst>(value);
      llvm::ConstantInt* constant    = llvm::dyn_cast_or_null<llvm::ConstantInt>(value);
      llvm::ICmpInst* icmp           = llvm::dyn_cast_or_null<llvm::ICmpInst>(value);
      llvm::Instruction* instruction = llvm::dyn_cast_or_null<llvm::Instruction>(value);

      if (instruction != nullptr) {

        /* Check if the instruction is a call */
        if (call != nullptr) {
          if (call->getCalledFunction()->getName().find("llvm.bswap.i") != std::string::npos) {
            return this->actx->bswap(this->do_convert(call->getOperand(0)));
          }
          throw triton::exceptions::AstLifting("LLVMToTriton::do_convert(): LLVM call not supported");
        }

        switch (instruction->getOpcode()) {

          case llvm::Instruction::AShr: {
            auto LHS = this->do_convert(instruction->getOperand(0));
            auto RHS = this->do_convert(instruction->getOperand(1));
            return this->actx->bvashr(LHS, RHS);
          }

          case llvm::Instruction::Add: {
            auto LHS = this->do_convert(instruction->getOperand(0));
            auto RHS = this->do_convert(instruction->getOperand(1));
            return this->actx->bvadd(LHS, RHS);
          }

          case llvm::Instruction::And: {
            auto LHS = this->do_convert(instruction->getOperand(0));
            auto RHS = this->do_convert(instruction->getOperand(1));
            /* LLVM does not distinct a logical AND of the bitwise AND */
            if (LHS->isLogical() && RHS->isLogical()) {
              return this->actx->ite(this->actx->land(LHS, RHS), this->actx->bvtrue(), this->actx->bvfalse());
            }
            return this->actx->bvand(LHS, RHS);
          }

          case llvm::Instruction::ICmp: {
            triton::ast::SharedAbstractNode node = nullptr;
            auto LHS = this->do_convert(instruction->getOperand(0));
            auto RHS = this->do_convert(instruction->getOperand(1));
            if (icmp != nullptr) {
              switch (icmp->getPredicate()) {
                case llvm::ICmpInst::ICMP_EQ:   return this->actx->equal(LHS, RHS);
                case llvm::ICmpInst::ICMP_NE:   return this->actx->distinct(LHS, RHS);
                case llvm::ICmpInst::ICMP_UGE:  return this->actx->bvuge(LHS, RHS);
                case llvm::ICmpInst::ICMP_UGT:  return this->actx->bvugt(LHS, RHS);
                case llvm::ICmpInst::ICMP_ULE:  return this->actx->bvule(LHS, RHS);
                case llvm::ICmpInst::ICMP_ULT:  return this->actx->bvult(LHS, RHS);
                case llvm::ICmpInst::ICMP_SGE:  return this->actx->bvsge(LHS, RHS);
                case llvm::ICmpInst::ICMP_SGT:  return this->actx->bvsgt(LHS, RHS);
                case llvm::ICmpInst::ICMP_SLE:  return this->actx->bvsle(LHS, RHS);
                case llvm::ICmpInst::ICMP_SLT:  return this->actx->bvslt(LHS, RHS);
                default:
                  break;
              }
              return node;
            }
            throw triton::exceptions::AstLifting("LLVMToTriton::do_convert(): ICmpInst not supported");
          }

          case llvm::Instruction::LShr: {
            auto LHS = this->do_convert(instruction->getOperand(0));
            auto RHS = this->do_convert(instruction->getOperand(1));
            return this->actx->bvlshr(LHS, RHS);
          }

          case llvm::Instruction::Mul: {
            auto LHS = this->do_convert(instruction->getOperand(0));
            auto RHS = this->do_convert(instruction->getOperand(1));
            return this->actx->bvmul(LHS, RHS);
          }

          case llvm::Instruction::Or: {
            auto LHS = this->do_convert(instruction->getOperand(0));
            auto RHS = this->do_convert(instruction->getOperand(1));
            /* LLVM does not distinct a logical OR of the bitwise OR */
            if (LHS->isLogical() && RHS->isLogical()) {
              return this->actx->ite(this->actx->lor(LHS, RHS), this->actx->bvtrue(), this->actx->bvfalse());
            }
            return this->actx->bvor(LHS, RHS);
          }

          case llvm::Instruction::Ret:
            return this->do_convert(instruction->getOperand(0));

          case llvm::Instruction::SDiv: {
            auto LHS = this->do_convert(instruction->getOperand(0));
            auto RHS = this->do_convert(instruction->getOperand(1));
            return this->actx->bvsdiv(LHS, RHS);
          }

          case llvm::Instruction::SExt: {
            /* Final size */
            auto size = instruction->getType()->getIntegerBitWidth();
            auto node = this->do_convert(instruction->getOperand(0));
            /* Size of the child */
            auto csze = instruction->getOperand(0)->getType()->getIntegerBitWidth();
            return this->actx->sx(size - csze, node);
          }

          case llvm::Instruction::SRem: {
            auto LHS = this->do_convert(instruction->getOperand(0));
            auto RHS = this->do_convert(instruction->getOperand(1));
            return this->actx->bvsrem(LHS, RHS);
          }

          case llvm::Instruction::Select: {
            auto nif    = this->do_convert(instruction->getOperand(0));
            auto nthen  = this->do_convert(instruction->getOperand(1));
            auto nelse  = this->do_convert(instruction->getOperand(2));

            /*
             * In some cases, LLVM simplifies the icmp by a constant
             * which is lifted to a bvtrue on our side. In this case,
             * we have to translate it to a logical node.
             */
            if (nif->isLogical() == false) {
              nif = this->actx->equal(nif, this->actx->bvtrue());
            }

            return this->actx->ite(nif, nthen, nelse);
          }

          case llvm::Instruction::Shl: {
            auto LHS = this->do_convert(instruction->getOperand(0));
            auto RHS = this->do_convert(instruction->getOperand(1));
            return this->actx->bvshl(LHS, RHS);
          }

          case llvm::Instruction::Sub: {
            auto LHS = this->do_convert(instruction->getOperand(0));
            auto RHS = this->do_convert(instruction->getOperand(1));
            return this->actx->bvsub(LHS, RHS);
          }

          case llvm::Instruction::Trunc: {
            auto size = instruction->getType()->getIntegerBitWidth();
            auto node = this->do_convert(instruction->getOperand(0));
            return this->actx->extract(size - 1, 0, node);
          }

          case llvm::Instruction::UDiv: {
            auto LHS = this->do_convert(instruction->getOperand(0));
            auto RHS = this->do_convert(instruction->getOperand(1));
            return this->actx->bvudiv(LHS, RHS);
          }

          case llvm::Instruction::URem: {
            auto LHS = this->do_convert(instruction->getOperand(0));
            auto RHS = this->do_convert(instruction->getOperand(1));
            return this->actx->bvurem(LHS, RHS);
          }

          case llvm::Instruction::Xor: {
            auto LHS = this->do_convert(instruction->getOperand(0));
            auto RHS = this->do_convert(instruction->getOperand(1));
            /* LLVM does not distinct a logical XOR of the bitwise XOR */
            if (LHS->isLogical() && RHS->isLogical()) {
              return this->actx->ite(this->actx->lxor(LHS, RHS), this->actx->bvtrue(), this->actx->bvfalse());
            }
            return this->actx->bvxor(LHS, RHS);
          }

          case llvm::Instruction::ZExt: {
            /* Final size */
            auto size = instruction->getType()->getIntegerBitWidth();
            auto node = this->do_convert(instruction->getOperand(0));
            /* Size of the child */
            auto csze = instruction->getOperand(0)->getType()->getIntegerBitWidth();
            return this->actx->zx(size - csze, node);
          }

          default:
            throw triton::exceptions::AstLifting("LLVMToTriton::do_convert(): LLVM instruction not supported");
        }
      }
      else if (constant != nullptr) {
        return this->actx->bv(constant->getLimitedValue(), constant->getBitWidth());
      }
      else if (argument != nullptr) {
        return this->actx->getVariableNode(argument->getName().data());
      }

      throw triton::exceptions::AstLifting("LLVMToTriton::do_convert(): LLVM instruction not supported");
    }


    SharedAbstractNode LLVMToTriton::convert(llvm::Module* llvmModule, const char* fname) {
      /* Check if the given llvm::module contains the __triton function */
      llvm::Function* function = llvmModule->getFunction(fname);
      if (function == nullptr) {
        throw triton::exceptions::AstLifting("LLVMToTriton::convert(): llvm::Module doesn't contain the given function name");
      }

      /* Get the entry block of the function */
      llvm::BasicBlock& entryBlock = function->getEntryBlock();

      /* Get the return of the function */
      llvm::Instruction* returnInstruction = entryBlock.getTerminator();

      /* Let's convert everything */
      return this->do_convert(returnInstruction);
    }

  }; /* ast namespace */
}; /* triton namespace */
