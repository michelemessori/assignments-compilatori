// AlgebraicIdentityPass.cpp
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace llvm::PatternMatch;

namespace {

struct AlgebraicIdentityPass : public PassInfoMixin<AlgebraicIdentityPass> {

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
        bool changed = false;

        for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
            BasicBlock &BB = *Iter;
            for (auto it = BB.begin(); it != BB.end(); it++) {
                Instruction &I = *it;
                
                // Consider only binary operations
                if (auto *BO = dyn_cast<BinaryOperator>(I)) {

                    Value *op0 = BO->getOperand(0);
                    Value *op1 = BO->getOperand(1);

                    // x + 0  or  0 + x
                    if (BO->getOpcode() == Instruction::Add) {
                        if (match(op1, m_Zero())) {
                            BO->replaceAllUsesWith(op0);
                            BO->eraseFromParent();
                            changed = true;
                            continue;
                        }
                        if (match(op0, m_Zero())) {
                            BO->replaceAllUsesWith(op1);
                            BO->eraseFromParent();
                            changed = true;
                            continue;
                        }
                    }

                    // x * 1  or  1 * x
                    if (BO->getOpcode() == Instruction::Mul) {
                        if (match(op1, m_One())) {
                            BO->replaceAllUsesWith(op0);
                            BO->eraseFromParent();
                            changed = true;
                            continue;
                        }
                        if (match(op0, m_One())) {
                            BO->replaceAllUsesWith(op1);
                            BO->eraseFromParent();
                            changed = true;
                            continue;
                        }
                    }
                }
            }
        }

        return changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
};

} // namespace

// Registrazione del plugin
extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION,
        "AlgebraicIdentityPass",
        "v0.1",
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    
                    if (Name == "algebraic-identity") {
                        FPM.addPass(AlgebraicIdentityPass());
                        return true;
                    }
                    return false;
                });
        }
    };
}