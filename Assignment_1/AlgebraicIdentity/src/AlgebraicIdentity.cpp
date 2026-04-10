#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace{

struct AlgebraicIdentity: PassInfoMixin<AlgebraicIdentity> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
      BasicBlock &B = *Iter;

      for (auto InstIter = B.begin(); InstIter != B.end(); ++InstIter) {
        Instruction &Inst = *InstIter;

        if(auto *BinOp = dyn_cast<BinaryOperator>(&Inst)) {

          if(BinOp->getOpcode() == Instruction::Add) {
            // è una add: controllo se uno dei due operandi è 0
            auto *ConstOp0 = dyn_cast<ConstantInt>(BinOp->getOperand(0));
            auto *ConstOp1 = dyn_cast<ConstantInt>(BinOp->getOperand(1));

            if(ConstOp0 && ConstOp0->getValue() == 0) {
              //trovato 0 + x
              BinOp->replaceAllUsesWith(BinOp->getOperand(1));
            } else if(ConstOp1 && ConstOp1->getValue() == 0) {
              //trovato x + 0
              BinOp->replaceAllUsesWith(BinOp->getOperand(0));
            }
          } else if(BinOp->getOpcode() == Instruction::Mul) {
            //è una mul: controllo se uno dei due operandi è 1
            auto *ConstOp0 = dyn_cast<ConstantInt>(BinOp->getOperand(0));
            auto *ConstOp1 = dyn_cast<ConstantInt>(BinOp->getOperand(1));

            if(ConstOp0 && ConstOp0->getValue() == 1) {
                //trovato 1 * x
                BinOp->replaceAllUsesWith(BinOp->getOperand(1));
            } else if(ConstOp1 && ConstOp1->getValue() == 1) {
                //trovato x * 1
                BinOp->replaceAllUsesWith(BinOp->getOperand(0));
            }
          }
        }
      }
    }
    return PreservedAnalyses::all();
  }
  static bool isRequired() { return true; }
};
} //namespace

llvm::PassPluginLibraryInfo getAlgebraicIdentityPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "AlgebraicIdentity", LLVM_VERSION_STRING,
            [](PassBuilder &PB) {
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                        if (Name == "algebraic-identity") {
                            FPM.addPass(AlgebraicIdentity());
                            return true;
                        }
                        return false;
                    });
            }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getAlgebraicIdentityPluginInfo();
}
