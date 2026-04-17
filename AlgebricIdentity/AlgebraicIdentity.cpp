#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"

using namespace llvm;

namespace {

struct AlgebraicIdentityPass : public PassInfoMixin<AlgebraicIdentityPass> {
    
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
        
        for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
            BasicBlock &BB = *Iter;

            for (auto InstIter = BB.begin(); InstIter != BB.end(); /* vuoto */ ) {
                
                Instruction &I_ref = *InstIter++;
                Instruction *I = &I_ref;

                // ADDIZIONE
                if (I->getOpcode() == Instruction::Add) {
                    Value *Op0 = I->getOperand(0);
                    Value *Op1 = I->getOperand(1);
                    
                    ConstantInt *CI0 = dyn_cast<ConstantInt>(Op0);
                    ConstantInt *CI1 = dyn_cast<ConstantInt>(Op1);

                    //per mia fortuna esistono isZero() e isOne()
                    if ((CI0 && CI0->isZero()) || (CI1 && CI1->isZero())) {
                        Value *Replacement = (CI0 && CI0->isZero()) ? Op1 : Op0;
                        I->replaceAllUsesWith(Replacement);
                        I->eraseFromParent();
                    }
                } 
                //MOLTIPLICAZIONE
                else if (I->getOpcode() == Instruction::Mul) {
                    Value *Op0 = I->getOperand(0);
                    Value *Op1 = I->getOperand(1);
                    
                    ConstantInt *CI0 = dyn_cast<ConstantInt>(Op0);
                    ConstantInt *CI1 = dyn_cast<ConstantInt>(Op1);

                    if ((CI0 && CI0->isOne()) || (CI1 && CI1->isOne())) {
                        Value *Replacement = (CI0 && CI0->isOne()) ? Op1 : Op0;
                        I->replaceAllUsesWith(Replacement);
                        I->eraseFromParent();
                    }
                }
                //SOTTRAZIONE (SOLO x - 0 = x)
                else if (I->getOpcode() == Instruction::Sub) {
                    Value *Op0 = I->getOperand(0);
                    Value *Op1 = I->getOperand(1);
                    
                    ConstantInt *CI1 = dyn_cast<ConstantInt>(Op1);

                    // Controlliamo SOLO l'operando di destra (Op1)
                    if (CI1 && CI1->isZero()) {
                        I->replaceAllUsesWith(Op0);
                        I->eraseFromParent();
                    }
                }
                //DIVISIONE (SOLO x / 1 = x)
                else if (I->getOpcode() == Instruction::SDiv || I->getOpcode() == Instruction::UDiv) {
                    Value *Op0 = I->getOperand(0);
                    Value *Op1 = I->getOperand(1);
                    
                    ConstantInt *CI1 = dyn_cast<ConstantInt>(Op1);

                    // Controlliamo SOLO l'operando di destra (Op1)
                    if (CI1 && CI1->isOne()) {
                        I->replaceAllUsesWith(Op0);
                        I->eraseFromParent();
                    }
                }
            }
        }

        return PreservedAnalyses::all();
    }
};
}

llvm::PassPluginLibraryInfo getAlgebraicIdentityPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "AlgebraicIdentity", LLVM_VERSION_STRING,
            [](PassBuilder &PB) {
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, FunctionPassManager &FPM,
                       ArrayRef<PassBuilder::PipelineElement>) {
                        // Il nome richiamabile da linea di comando è "algebraic-identity"
                        if (Name == "algebraic-identity") {
                            FPM.addPass(AlgebraicIdentityPass());
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
