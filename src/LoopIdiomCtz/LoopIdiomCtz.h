//
// Created by kalas on 30.12.2023.
//

#ifndef LOOPIDIOMCTZ_H
#define LOOPIDIOMCTZ_H

#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

namespace llvm {

class Loop;
class LPMUpdater;

class LoopIdiomCtzPass : public PassInfoMixin<LoopIdiomCtzPass> {
public:
  PreservedAnalyses run(Loop &L, LoopAnalysisManager &AM,
                        LoopStandardAnalysisResults &AR, LPMUpdater &U);

    static  bool isRequired() { return true; }
};

}

#endif //LOOPIDIOMCTZ_H
