// =========================================================== //
// Questo passo implementa l'ottimizzazione di Loop Fusion in
// LLVM. Perché due loop siano "fondibili", devono soddisfare
// le seguenti condizioni:
//   1. devono essere ADIACENTI, ovvero non ci possono essere
//      istruzioni che vengono eseguite tra la fine del primo
//      e l'inizio del secondo;
//   2. devono iterare lo stesso numero di volte;
//   3. devono essere equivalenti rispetto al flusso di
//      controllo, ovvero soggetti alle stesse condizioni di
//      esecuzione;
//   4. non possono esserci iterazioni a distanza negativa tra
//      di essi, ovvero il secondo loop non può, a una sua
//      m-esima iterazione, usare un valore calcolato alla
//      m+n-esima iterazione del primo (con n > 0).
// ========================================================== //
#include <llvm/Analysis/ScalarEvolutionExpressions.h>
#include <llvm/IR/Analysis.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/ADT/DenseMap.h"
#include <llvm/Analysis/ScalarEvolution.h>
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

namespace{
struct LoopFusionPass: PassInfoMixin<LoopFusionPass>{
  // funzione per riscrivere una SCEV nei termini di un altro loop
  // (utile per dependency check)
  static const SCEV *rewriteSCEVInTermsOfLoop(const SCEV *S, const Loop *OldL, const Loop *NewL, ScalarEvolution &SE){
    // se si tratta di un contatore
    if(auto *AR = dyn_cast<SCEVAddRecExpr>(S)){
      // lo scriviamo uguale, ma come associato al nuovo loop
      return SE.getAddRecExpr(
        AR->getStart(),
        AR->getStepRecurrence(SE),
        NewL,
        AR->getNoWrapFlags()
      );
    }
    // altrimenti è loop invariant, e quindi non c'è nulla da riscrivere
    return S;
  }
    
  bool eligibilityCheck(Loop *L){
    // da https://llvm.org/docs/LoopFusion.html
    // punti 1. e 3. il loop deve essere in forma semplificata, con un singolo exiting block e un singolo exit block
    if(!L->isLoopSimplifyForm()){
      errs() << "Loop ";
      L->getHeader()->printAsOperand(errs(), false);
      errs() << " non in forma semplificata\n";
      return false;
    }
    if(!L->getExitingBlock() || !L->getUniqueExitBlock()){
      errs() << "Loop ";
      L->getHeader()->printAsOperand(errs(), false);
      errs() << " non ha un singolo exiting block o un singolo exit block\n";
      return false;
    }
    //punto 4. il loop deve essere in forma rotated
    if(!L->isRotatedForm()){
      errs() << "Loop ";
      L->getHeader()->printAsOperand(errs(), false);
      errs() << " non in forma rotated\n";
      return false;
    }
    return true;
  }


  // -- 1a condizione: loop adjancency --
  bool areLoopsAdjacent(Loop* L0, Loop* L1, DominatorTree &DT){
    // se solo uno dei due è guarded, non hanno control flow equivalence
    if(L0->isGuarded() != L1->isGuarded()) return false;

    // caso entrambi guarded
    if(L0->isGuarded() && L1->isGuarded()){
      BranchInst* G0 = L0->getLoopGuardBranch();
      BranchInst* G1 = L1->getLoopGuardBranch();

      /* i loop guarded sono adiacenti se:
       - l'entry block di L0 domina quello di L1; e
       - l'exit block di L0 ha un unico successore che è l'entry
         block di L1
      */
      if(!DT.dominates(G0->getParent(), G1->getParent()))
        return false;
      if(L0->getExitBlock()->getUniqueSuccessor() != G1->getParent())
        return false;
    }
    //caso entrambi non guarded
    else {
      /* i loop unguarded sono adiacenti se l'exit block di L0
      è esattamente il preheader di L1 */
      if(L0->getExitBlock() != L1->getLoopPreheader())
        return false;
    }
    return true;
  }

  // -- 2a condizione: stesso numero di iterazioni --
  bool sameIterationCount(Loop* L0, Loop *L1, ScalarEvolution &SE){
    const SCEV *TC0 = SE.getBackedgeTakenCount(L0),
               *TC1 = SE.getBackedgeTakenCount(L1);

    if(isa<SCEVCouldNotCompute>(TC0) || isa<SCEVCouldNotCompute>(TC1))
      return false;
    
    return SE.isKnownPredicate(CmpInst::ICMP_EQ, TC0, TC1);
  }
  
  // -- 3a condizione: control flow equivalency -- 
  bool areLoopsCFEquivalent(Loop *L0, Loop *L1, DominatorTree &DT, PostDominatorTree &PDT){
    if(L0->isGuarded()){
      BasicBlock *Guard0 = L0->getLoopGuardBranch()->getParent();
      BasicBlock *Guard1 = L1->getLoopGuardBranch()->getParent();
      return DT.dominates(Guard0, Guard1) && PDT.dominates(Guard1, Guard0);
    }

    return DT.dominates(L0->getHeader(), L1->getHeader())
        && PDT.dominates(L1->getHeader(), L0->getHeader());
  }



  // -- 4a condizione: nessuna dipendenza a distanza negativa tra L1 e L0 --

  bool noNegativeDistanceDependencies(Loop *L0, Loop *L1, ScalarEvolution &SE){
    auto memInstructions = [](Loop *L){
      SmallVector<Instruction *, 16> insts;
      for (BasicBlock *BB : L->blocks())
        for (Instruction &I : *BB)
          if(isa<LoadInst>(I) || isa<StoreInst>(I))
            insts.push_back(&I);
      return insts;
    };

    for(Instruction *I0 : memInstructions(L0)){
      for(Instruction *I1 : memInstructions(L1)){
        // il caso in cui siano entrambe load non è problematico
        if(isa<LoadInst>(I0) && isa<LoadInst>(I1))
          continue;
    
        Value *Ptr0 = getLoadStorePointerOperand(I0);
        Value *Ptr1 = getLoadStorePointerOperand(I1);
        if(!Ptr0 || !Ptr1) continue;

        // se accedono a oggetti diversi, non c'è dipendenza da controllare
        if(getUnderlyingObject(Ptr0) != getUnderlyingObject(Ptr1))
          continue;

        const SCEV *Scev0 = SE.getSCEVAtScope(Ptr0, L0);
        const SCEV *Scev1 = SE.getSCEVAtScope(Ptr1, L1);

        if(isa<SCEVCouldNotCompute>(Scev0) || isa<SCEVCouldNotCompute>(Scev1))
          return false;

        const SCEV *Scev1InL0 = rewriteSCEVInTermsOfLoop(Scev1, L1, L0, SE);

        // controllo: l'indirizzo che usa I1 è sempre <= di quello che usa I0?
        if(!SE.isKnownPredicate(CmpInst::ICMP_SLE, Scev1InL0, Scev0))
          return false;
      }
    }
    return true;
  }

  // funzione wrapper che raccoglie tutti i controlli necessari alla fusione
  bool canFuseLoops(Loop *L0, Loop *L1, DominatorTree &DT, PostDominatorTree &PDT, ScalarEvolution &SE){
    if(!eligibilityCheck(L0) || !eligibilityCheck(L1)){
      L0->getHeader()->printAsOperand(errs(), false);
      errs() << " o ";
      L1->getHeader()->printAsOperand(errs(), false);
      errs() << " non sono eligibile\n";
      return false;
    }

    if(!areLoopsAdjacent(L0, L1, DT)){
      L0->getHeader()->printAsOperand(errs(), false);
      errs() << " e ";
      L1->getHeader()->printAsOperand(errs(), false);
      errs() << " non sono adiacenti\n";
      return false;
    }

    if(!sameIterationCount(L0, L1, SE)){
      L0->getHeader()->printAsOperand(errs(), false);
      errs() << " e ";
      L1->getHeader()->printAsOperand(errs(), false);
      errs() << " non hanno lo stesso numero di iterazioni\n";
      return false;
    }

    if(!areLoopsCFEquivalent(L0, L1, DT, PDT)){
      L0->getHeader()->printAsOperand(errs(), false);
      errs() << " e ";
      L1->getHeader()->printAsOperand(errs(), false);
      errs() << " non sono equivalenti rispetto al flusso di controllo\n";
      return false;
    }
    if(!noNegativeDistanceDependencies(L0, L1, SE)){
      L0->getHeader()->printAsOperand(errs(), false);
      errs() << " e ";
      L1->getHeader()->printAsOperand(errs(), false);
      errs() << " hanno dipendenze a distanza negativa\n";
      return false;
    }

    return true;
  }

    /* La fusione di due loop si compone di due step:
     1. La sostituzione degli usi della seconda induction variable con la prima
     2. L'agganciamento del body del secondo loop dopo il primo */
  bool fuseLoops(Loop *L0, Loop *L1, ScalarEvolution &SE, LoopInfo &LI){


    BasicBlock *Header0 = L0->getHeader();
    BasicBlock *Latch0  = L0->getLoopLatch();
    BasicBlock *Header1 = L1->getHeader();
    BasicBlock *Latch1  = L1->getLoopLatch();
    BasicBlock *Preheader0 = L0->getLoopPreheader();
    BasicBlock *Preheader1 = L1->getLoopPreheader();

    // controllo preliminare sulle istruzioni di branch dei latch
    auto *Br0 = dyn_cast<BranchInst>(Latch0->getTerminator());
    auto *Br1 = dyn_cast<BranchInst>(Latch1->getTerminator());
    if(!Br0 || !Br0->isConditional() || !Br1 || !Br1->isConditional())
      return false;
  
    // -- step 1: sostituione usi PHI -- 
    
    // prima controlliamo che le due induction variable esistano e siano equivalenti
    PHINode *IV0 = L0->getInductionVariable(SE);
    PHINode *IV1 = L1->getInductionVariable(SE);
    if(!IV0 || !IV1) return false;
    if(!SE.isKnownPredicate(CmpInst::ICMP_EQ, SE.getSCEV(IV0),
                            rewriteSCEVInTermsOfLoop(SE.getSCEV(IV1), L1, L0, SE)))
      return false;

    
    // creo un vettore per i PHI node di L1, che dovranno essere ricreati
    SmallVector<PHINode*, 4> ExtraPHIs;
    for(PHINode &PN : L1->getHeader()->phis())
      if(&PN != IV1) ExtraPHIs.push_back(&PN);



    // sostituzione vera e propria della induction variable
    Value *Step0 = IV0->getIncomingValueForBlock(Latch0);
    Value *Step1 = IV1->getIncomingValueForBlock(Latch1);
    IV1->replaceAllUsesWith(IV0);
    IV1->eraseFromParent();
 
    // unisco le istruzioni di step
    if(Step0 != Step1){
      Step1->replaceAllUsesWith(Step0);
      if(auto *Step1Inst = dyn_cast<Instruction>(Step1))
        if(Step1Inst->use_empty())
          Step1Inst->eraseFromParent();
    }
    
    // -- secondo step: unione dei due loop --
    
    SmallVector<BasicBlock*, 4> DeadBlocks;
    DeadBlocks.push_back(Preheader1);

    // se i loop guarded, tolgo la seconda guard
    BranchInst *G0 = L0->getLoopGuardBranch();
    if(G0){
      BranchInst *G1 = L1->getLoopGuardBranch();
      BasicBlock *AfterLoop1 = G1->getSuccessor(G1->getSuccessor(0) == Preheader1 ? 1 : 0);

      DeadBlocks.push_back(L0->getExitBlock());
      DeadBlocks.push_back(G1->getParent());
      
      // salto direttamente a dopo l'intero loop se la verifica è falsa
      unsigned SkipIdx = (G0->getSuccessor(0) == Preheader0) ? 1 : 0;
      G0->setSuccessor(SkipIdx, AfterLoop1);
    }

    // elimino il vecchio latch del primo loop e lo sostituisco con un
    // salto incondizionato verso L1
    Br0->eraseFromParent();
    BranchInst::Create(Header1, Latch0);

    // il secondo latch deve tornare al primo loop
    for(unsigned i = 0; i < Br1->getNumSuccessors(); i++)
      if(Br1->getSuccessor(i) == Header1)
        Br1->setSuccessor(i, Header0);

    // le phi di L0 devono ricevere il backedge dal latch di L1
    for(PHINode &PN : Header0->phis()){
      int Idx = PN.getBasicBlockIndex(Latch0);
      if(Idx >= 0) PN.setIncomingBlock(Idx, Latch1);
    }

    // ricreo le phi del secondo loop
    for(PHINode *PN : ExtraPHIs){
      Value *InitVal = PN->getIncomingValueForBlock(Preheader1);
      Value *NextVal = PN->getIncomingValueForBlock(Latch1);
      PHINode *NewPN = PHINode::Create(PN->getType(), 2,
                                        PN->getName() + ".fused",
                                        &Header0->front()); // posizione di inserimento: l'header di L0
      NewPN->addIncoming(InitVal, Preheader0); // stesso valore iniziale, ma dal preheader di L0
      NewPN->addIncoming(NextVal, Latch1); // stesso valore aggiornato, ma dal latch di L1
      PN->replaceAllUsesWith(NewPN);
      PN->eraseFromParent();
    }

    // muovo le istruzioni residue del secondo preheader nel primo
    Instruction *InsertPt = Preheader0->getTerminator();
    for(Instruction &I : llvm::make_early_inc_range(*Preheader1))
      if(!I.isTerminator())
        I.moveBefore(InsertPt);

    // i blocchi di L1 ormai irraggiungibili (preheader, ed eventuale
    // guardia/exit di L0) vengono rimossi
    DeleteDeadBlocks(DeadBlocks);

    // aggiorno la struttura dati dei loop
    for (BasicBlock *BB : L1->blocks())
      L0->addBasicBlockToLoop(BB, LI);

    LI.erase(L1);

    return true;
  }
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM){
    bool Changed = false;

    // ad ogni fusione le analisi correnti vengono invalidate: ricominciamo
    // la ricerca da capo finché non si trova più nessuna coppia fondibile
    bool FusedSomething = true;
    while(FusedSomething){
      FusedSomething = false;

      LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
      DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
      PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
      ScalarEvolution &SE = AM.getResult<ScalarEvolutionAnalysis>(F);

      // lavoriamo soltanto sui loop più interni
      SmallVector<Loop *, 8> Worklist;
      for (Loop *TopLevelLoop : LI) {
        for (Loop *L : depth_first(TopLevelLoop)){
          if(L->isInnermost())
            Worklist.push_back(L);
        }
      }

      for(Loop *L0 : Worklist){
        for(Loop *L1 : Worklist){
          if(L0 == L1) continue;
          if(!canFuseLoops(L0, L1, DT, PDT, SE)) continue;
          errs() << "I loop ";
          L0->getHeader()->printAsOperand(errs(), false);
          errs() << " e ";
          L1->getHeader()->printAsOperand(errs(), false);
          errs() << " possono essere fusi\n";
          if(!fuseLoops(L0, L1, SE, LI)) continue;

          Changed = true;
          FusedSomething = true;
          errs() << "I loop sono stati fusi\n";
          // se ho fuso qualcosa, le analisi correnti non sono più valide: ricomincio da capo
          AM.invalidate(F, PreservedAnalyses::none());
          break;
        }
        if(FusedSomething) break;
      }
    }

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
  static bool isRequired(){ return true; }
};
} //namespace


llvm::PassPluginLibraryInfo getLoopFusionPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "LoopFusionPass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "custom-loop-fusion") {
                    FPM.addPass(LoopFusionPass());
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
  return getLoopFusionPassPluginInfo();
}
