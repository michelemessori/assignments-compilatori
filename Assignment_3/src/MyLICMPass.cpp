#include <llvm/IR/Instructions.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/ValueTracking.h"

using namespace llvm;

namespace{
struct MyLICMPass: PassInfoMixin<MyLICMPass>{

  bool isHoistableInstruction(Instruction *I, DominatorTree &DT) {
    if (I->isTerminator() || isa<PHINode>(I))
      return false;

    if (I->mayHaveSideEffects() || I->mayReadOrWriteMemory())
      return false;

    return isSafeToSpeculativelyExecute(I, I, nullptr, &DT);
  }

  bool isLoopInvariant(SetVector<Instruction*> Loop_Invs, Instruction *I, Loop *L){
    //i phi node hanno valore variabile a seconda del cammino percorso
    if(isa<PHINode>(I)) return false;

    for(Value *Operand : I->operands()){
      if(Instruction *def = dyn_cast<Instruction>(Operand)){
        //un'istruzione è loop invariant se:
        // 1. i suoi operandi sono definiti fuori dal loop
        // 2. i suoi operandi sono definiti in una istruzione invariante nel loop
        if(L->contains(def) && !Loop_Invs.contains(def)){
          return false;
        }
      }      
    }
    return true;
  }
  bool dominatesAllExits(BasicBlock *BB, Loop *L, DominatorTree &DT) {
    SmallVector<BasicBlock *> ExitBlocks;
    L->getExitBlocks(ExitBlocks);

    for (BasicBlock *ExitBlock : ExitBlocks) {
      if (!DT.dominates(BB, ExitBlock))
        return false;
    }

    return true;
  }

  bool isDeadAfterLoop(Instruction *I, Loop *L){
    for(Use &U: I->uses()){
      Instruction *UserInst = dyn_cast<Instruction>(U.getUser());
      if(!UserInst || !L->contains(UserInst)) return false;
    }
    return true;
  }

  bool isCMCandidate(Instruction *I, BasicBlock *BB, Loop *L, DominatorTree &DT){
    //l'istruzione, per essere candidabile alla code motion, deve:
    // 1. essere loop invariant (già dato)
    // 2. essere contenuta in un blocco che domina tutte le uscite,
    // oppure definire una variabile che sia dead all'uscita
    // 3. essere l'unica definizione di quella variabile nel loop
    // 4. dominare tutti i suoi usi

    // punto 2
    if(!dominatesAllExits(BB,L,DT) && !isDeadAfterLoop(I,L)) return false;

    // punto 3: l'IR LLVM è in SSA, e siccome l'istruzione non è un PHI node
    // (come abbiamo controllato in isLoopInvariant) non ha altre definizioni

    // punto 4: per gli stessi motivi, la definizione domina tutti i suoi usi
    
    
    return true;
  }
  
   

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM){
    bool Changed = false;

    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);

    for(Loop *L: LI){
      BasicBlock *Preheader = L->getLoopPreheader();      
      // controllo che il loop abbia un preheader dove spostare le istruzioni
      if(!Preheader) continue;

      SetVector<Instruction*> Loop_Invariants;
      SetVector<Instruction*> CM_Candidates;
      SetVector<Instruction*> CM_Moved;

      for(BasicBlock *BB: L->blocks()){

        // calcolo loop invariants
        for(Instruction &Inst : *BB){
          // non sposto terminator, accessi in memoria o istruzioni non sicure
          // da eseguire speculativamente nel preheader
          if(!isHoistableInstruction(&Inst, DT)) continue;

          if(isLoopInvariant(Loop_Invariants, &Inst, L)){
            Loop_Invariants.insert(&Inst);
          }
        }
        // calcolo istruzioni candidabili
        for(Instruction *Inst : Loop_Invariants){
          if(isCMCandidate(Inst, Inst->getParent(), L, DT)){
            CM_Candidates.insert(Inst);
          }
        }

        // sposto le istruzioni candidabili, se non dipendono
        // da altre istruzioni che non sono state ancora spostate
        for(Instruction *Inst : CM_Candidates){
          bool CanMove = true;

          for(Value *Operand : Inst->operands()){
            if(Instruction *Def = dyn_cast<Instruction>(Operand)){
              if(L->contains(Def) && !CM_Moved.contains(Def)){
                CanMove = false;
                break;
              }
            }
          }

          if(CanMove){
            Inst->moveBefore(Preheader->getTerminator());
            CM_Moved.insert(Inst);
            Changed = true;
          }
        }
        
      }
    }
  	return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
  static bool isRequired(){ return true; }
};
} //namespace


llvm::PassPluginLibraryInfo getMyLICMPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "MyLICMPass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "custom-licm") {
                    FPM.addPass(MyLICMPass());
                    return true;
                  }
                  return false;
                });
          }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize LoopPass when added to the pass pipeline on the
// command line, i.e. via '-passes=loop-pass'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getMyLICMPassPluginInfo();
}
