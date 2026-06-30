#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/IR/Analysis.h>

using namespace llvm;

namespace{
struct StrengthReduction: PassInfoMixin<StrengthReduction> {

  bool udivOptimization(Instruction &Inst){
    // guardo se l'operando destro è costante
    ConstantInt *ConstOp = dyn_cast<ConstantInt>(Inst.getOperand(1));

    // guardo se l'operando è una potenza di 2, nel caso shift
    if(ConstOp && ConstOp->getValue().isPowerOf2()) {
      outs() << "TROVATA UDIV CON OPERANDO POTENZA DI 2:\n\t " << Inst << "\n";
      Instruction* NewInst = BinaryOperator::Create(
        Instruction::LShr, 
        Inst.getOperand(0),
        ConstantInt::get(ConstOp->getType(), ConstOp->getValue().exactLogBase2())
      );
      
      //inserisco la nuova istruzione dopo quella vecchia
      NewInst->insertAfter(&Inst);

      //aggiorno gli usi
      Inst.replaceAllUsesWith(NewInst);
      outs() << "SOSTITUITA CON:\n\t " << *NewInst << "\n";
      return true;
    }
    return false;
  }
  
  bool mulOptimization(Instruction &Inst) {
    // guardo quale dei due operandi è costante
    Value *VarOp = nullptr;
    ConstantInt *ConstOp = nullptr;

    Instruction *NewInst = nullptr;
    if (ConstantInt *C = dyn_cast<ConstantInt>(Inst.getOperand(0))) {
      ConstOp = C;
      VarOp = Inst.getOperand(1);
    } else if (ConstantInt *C = dyn_cast<ConstantInt>(Inst.getOperand(1))) {
      ConstOp = C;
      VarOp = Inst.getOperand(0);
    }

    // guardo se l'operando è una potenza di 2 o di forma 2^n (+-) 1
    if(ConstOp){
      if(ConstOp->getValue().isPowerOf2()) {
        outs() << "TROVATA MUL CON OPERANDO POTENZA DI 2:\n\t " << Inst << "\n";
        NewInst = BinaryOperator::Create(
          Instruction::Shl, 
          VarOp,
          ConstantInt::get(ConstOp->getType(), ConstOp->getValue().exactLogBase2())
        );
        NewInst->insertAfter(&Inst);
        Inst.replaceAllUsesWith(NewInst);
        return true;
      } else if((ConstOp->getValue()+1).isPowerOf2()) {
        outs() << "TROVATA MUL CON OPERANDO DI FORMA 2^n-1:\n\t " << Inst << "\n";
        Instruction *Shift = BinaryOperator::Create(
          Instruction::Shl, 
          VarOp,
          ConstantInt::get(ConstOp->getType(), (ConstOp->getValue()+1).exactLogBase2())
        );
        Shift->insertAfter(&Inst);
        NewInst = BinaryOperator::Create(
          Instruction::Sub, 
          Shift,
          VarOp
        );
        NewInst->insertAfter(Shift);
        Inst.replaceAllUsesWith(NewInst);
        outs() << "SOSTITUITA CON:\n\t " << *Shift << "\n\t" << *NewInst << "\n";
        return true;
      } else if ((ConstOp->getValue()-1).isPowerOf2()) {
        outs() << "TROVATA MUL CON OPERANDO DI FORMA 2^n+1:\n\t " << Inst << "\n";
        Instruction *Shift = BinaryOperator::Create(
          Instruction::Shl, 
          VarOp,
          ConstantInt::get(ConstOp->getType(), (ConstOp->getValue()-1).exactLogBase2())
        );
        Shift->insertAfter(&Inst);
        NewInst = BinaryOperator::Create(
          Instruction::Add, 
          Shift,
          VarOp
        );
        NewInst->insertAfter(Shift);
        Inst.replaceAllUsesWith(NewInst);
        outs() << "SOSTITUITA CON:\n\t " << *Shift << "\n\t" << *NewInst << "\n";

        return true;
      }
    }
    return false;
  }

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) { 
    bool Changed = false;
    for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
      BasicBlock &B = *Iter;

      for (auto InstIter = B.begin(); InstIter != B.end(); ++InstIter) {
        Instruction &Inst = *InstIter;

        if(BinaryOperator *BinOp = dyn_cast<BinaryOperator>(&Inst)) {
          if(BinOp->getOpcode() == Instruction::UDiv) {
            Changed = udivOptimization(Inst) || Changed;
          } else if(BinOp->getOpcode() == Instruction::Mul) {
            Changed = mulOptimization(Inst) || Changed;
          }
        }
      }
    }

    return Changed ? PreservedAnalyses::none()
                   : PreservedAnalyses::all();
  }
  static bool isRequired() { return true; }
};
} //namespace

llvm::PassPluginLibraryInfo getStrengthReductionPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "StrengthReduction", LLVM_VERSION_STRING,
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, FunctionPassManager &FPM,
        ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "strength-reduction") {
            FPM.addPass(StrengthReduction());
            return true;
          }
          return false;
        });
  }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getStrengthReductionPluginInfo();
}
