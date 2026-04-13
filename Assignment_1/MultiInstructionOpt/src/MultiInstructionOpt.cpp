#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
namespace{
struct MultiInstructionOpt: PassInfoMixin<MultiInstructionOpt> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) { 
    for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
      BasicBlock &B = *Iter;
      for (auto InstIter = B.begin(); InstIter != B.end(); ++InstIter) {
        Instruction &Inst = *InstIter;
        if(auto *BinOp = dyn_cast<BinaryOperator>(&*InstIter)) {
          // guardo quale dei due operandi è costante
          Value *VarOp = nullptr;
          ConstantInt *ConstOp = nullptr;

          if (auto *C = dyn_cast<ConstantInt>(BinOp->getOperand(0))) {
            ConstOp = C;
            VarOp = BinOp->getOperand(1);
          } else if (auto *C = dyn_cast<ConstantInt>(BinOp->getOperand(1))) {
            ConstOp = C;
            VarOp = BinOp->getOperand(0);
          }

          // se ha un operando costante, guardo gli users
          if(ConstOp){
            for (auto userIter = Inst.user_begin(); userIter != Inst.user_end(); ++userIter) {
              User *U = *userIter;
              // controllo che l'user sia un operatore binario
              if (auto *UserBinOp = dyn_cast<BinaryOperator>(U)) {
                // controllo che abbia lo stesso operando costante
                if (UserBinOp->getOperand(0) == ConstOp || UserBinOp->getOperand(1) == ConstOp) {
                  // guardo se ha operazione inversa rispetto a BinOp
                  if ((BinOp->getOpcode() == Instruction::Mul && UserBinOp->getOpcode() == Instruction::SDiv) ||
                      (BinOp->getOpcode() == Instruction::SDiv && UserBinOp->getOpcode() == Instruction::Mul) ||
                      (BinOp->getOpcode() == Instruction::Add && UserBinOp->getOpcode() == Instruction::Sub) ||
                      (BinOp->getOpcode() == Instruction::Sub && UserBinOp->getOpcode() == Instruction::Add)) {
                    outs() << "TROVATA OPERAZIONE INVERSA CON STESSI OPERATORI:\n\t " << Inst << "\n\t " << *U << "\n";
                    
                    // in questo caso, posso sostituire la seconda operazione con un assegnamento
                    Instruction *NewInst = BinaryOperator::Create(
                      Instruction::Add, 
                      VarOp,
                      ConstantInt::get(ConstOp->getType(), 0)
                    );
                    NewInst->insertAfter(UserBinOp);
                    UserBinOp->replaceAllUsesWith(NewInst);
                    outs() << "SOSTITUITA CON:\n\t " << *NewInst << "\n";
                  }
                }
              }
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

llvm::PassPluginLibraryInfo getMultiInstructionOptPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "MultiInstructionOpt", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "multi-instruction-opt") {
                    FPM.addPass(MultiInstructionOpt());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return getMultiInstructionOptPluginInfo();
}