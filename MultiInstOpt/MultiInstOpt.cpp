#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"

using namespace llvm;

namespace {

struct MultiInstOptPass : public PassInfoMixin<MultiInstOptPass> {
    
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
        bool Changed = false;
        
        for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
            BasicBlock &BB = *Iter;

            for (auto InstIter = BB.begin(); InstIter != BB.end();) {
                Instruction &Inst2 = *InstIter++; // c = a - 1

                // Cerchiamo un'Addizione o Sottrazione
                if (Inst2.getOpcode() == Instruction::Add || Inst2.getOpcode() == Instruction::Sub) {
                    
                    Value *Op2_Left = Inst2.getOperand(0);
                    Value *Op2_Right = Inst2.getOperand(1);
                    
                    // Nelle sottrazioni la costante DEVE essere a destra (es. a - 1)
                    ConstantInt *C2 = dyn_cast<ConstantInt>(Op2_Right);
                    Instruction *Inst1 = dyn_cast<Instruction>(Op2_Left);

                    if (Inst2.getOpcode() == Instruction::Add && !C2) {
                        C2 = dyn_cast<ConstantInt>(Op2_Left);
                        Inst1 = dyn_cast<Instruction>(Op2_Right);
                    }

                    // Se abbiamo trovato una costante (C2) e l'altro operando è un'istruzione (Inst1)
                    if (C2 && Inst1) {
                        
                        // Ora ispezioniamo Inst1 (a = b + 1)
                        if (Inst1->getOpcode() == Instruction::Add || Inst1->getOpcode() == Instruction::Sub) {
                            
                            Value *Op1_Left = Inst1->getOperand(0);
                            Value *Op1_Right = Inst1->getOperand(1);

                            ConstantInt *C1 = dyn_cast<ConstantInt>(Op1_Right);
                            Value *BaseVar = Op1_Left; // Questa è la nostra 'b'

                            if (Inst1->getOpcode() == Instruction::Add && !C1) {
                                C1 = dyn_cast<ConstantInt>(Op1_Left);
                                BaseVar = Op1_Right;
                            }

                            if (C1) {
                                // Abbiamo trovato: Inst2( Inst1(BaseVar, C1), C2 )
                                if (C1->getValue() == C2->getValue()) {
                                    
                                    // CASO 1: Addizione seguita da Sottrazione (b + C) - C
                                    if (Inst1->getOpcode() == Instruction::Add && Inst2.getOpcode() == Instruction::Sub) {
                                        Inst2.replaceAllUsesWith(BaseVar);
                                        Inst2.eraseFromParent();
                                        Changed = true;
                                    }
                                    // CASO 2: Sottrazione seguita da Addizione (b - C) + C
                                    else if (Inst1->getOpcode() == Instruction::Sub && Inst2.getOpcode() == Instruction::Add) {
                                        Inst2.replaceAllUsesWith(BaseVar);
                                        Inst2.eraseFromParent();
                                        Changed = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
};
} // end anonymous namespace

llvm::PassPluginLibraryInfo getMultiInstOptPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "MultiInstOpt", LLVM_VERSION_STRING,
            [](PassBuilder &PB) {
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, FunctionPassManager &FPM,
                       ArrayRef<PassBuilder::PipelineElement>) {
                        if (Name == "multi-inst-opt") {
                            FPM.addPass(MultiInstOptPass());
                            return true;
                        }
                        return false;
                    });
            }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getMultiInstOptPluginInfo();
}