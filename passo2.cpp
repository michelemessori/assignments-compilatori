#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"

using namespace llvm;

namespace {

struct StrengthReductionPass : public PassInfoMixin<StrengthReductionPass> {
    

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
        bool Changed = false;
        
        // Raccogliamo le istruzioni in una lista separata per evitare 
        // di modificare la BasicBlock mentre ci iteriamo sopra. grazie internet mi falliva il codice
         for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
            BasicBlock &BB = *Iter;
            for (auto it = BB.begin(); it != BB.end(); it++) {
                Instruction &I = *it;
            
            //MOLTIPLICAZIONE

                if (I->getOpcode() == Instruction::Mul) {
                    Value *Op0 = I->getOperand(0);
                    Value *Op1 = I->getOperand(1);
                
                    
                    ConstantInt *CI = dyn_cast<ConstantInt>(Op1);
                    Value *V = Op0;
                
                    if (!CI) {
                        CI = dyn_cast<ConstantInt>(Op0);
                        V = Op1;
                    }

                    if (CI) {
                        APInt Val = CI->getValue();
                        IRBuilder<> Builder(I);
                    
                        if (Val.isPowerOf2()) {
                            // Es: x * 4 -> x << 2
                            Value *Shl = Builder.CreateShl(V, Val.logBase2());
                            I->replaceAllUsesWith(Shl);
                            I->eraseFromParent();
                            Changed = true;
                        } 
                        else if ((Val - 1).isPowerOf2()) { 
                            Value *Shl = Builder.CreateShl(V, (Val - 1).logBase2());
                            Value *Add = Builder.CreateAdd(Shl, V);
                            I->replaceAllUsesWith(Add);
                            I->eraseFromParent();
                            Changed = true;
                        } 
                        else if ((Val + 1).isPowerOf2()) { 
                            Value *Shl = Builder.CreateShl(V, (Val + 1).logBase2());
                            Value *Sub = Builder.CreateSub(Shl, V);
                            I->replaceAllUsesWith(Sub);
                            I->eraseFromParent();
                            Changed = true;
                        }
                    }
                }
            } 


                else if (I->getOpcode() == Instruction::UDiv || I->getOpcode() == Instruction::SDiv) {
                    Value *Op0 = I->getOperand(0);
                    Value *Op1 = I->getOperand(1);
                
                    if (ConstantInt *CI = dyn_cast<ConstantInt>(Op1)) {
                        APInt Val = CI->getValue();
                    
                        if (Val.isPowerOf2()) {
                            IRBuilder<> Builder(I);
                            Value *Shr;
                        
                            
                            if (I->getOpcode() == Instruction::UDiv) {
                                Shr = Builder.CreateLShr(Op0, Val.logBase2()); 
                            } else {
                                Shr = Builder.CreateAShr(Op0, Val.logBase2()); 
                            }
                        
                            I->replaceAllUsesWith(Shr);
                            I->eraseFromParent();
                            Changed = true;
                        }
                    }
                }
            }

            return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
        }
    };
}

llvm::PassPluginLibraryInfo getStrengthReductionPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "StrengthReduction", LLVM_VERSION_STRING,
            [](PassBuilder &PB) {
                // Registriamo il passo richiamabile con il nome "strength-reduction" via linea di comando
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, FunctionPassManager &FPM,
                       ArrayRef<PassBuilder::PipelineElement>) {
                        if (Name == "strength-reduction") {
                            FPM.addPass(StrengthReductionPass());
                            return true;
                        }
                        return false;
                    });
            }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getStrengthReductionPluginInfo();
}