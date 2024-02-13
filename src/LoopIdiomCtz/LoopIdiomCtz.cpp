//===- LoopCtzIdiomRecognize.cpp - Loop idiom recognition --------------------===//
//
// That file is modified version of LoopIdiomRecognize.cpp(Part of LLVM Project)
// Author: Kalashnikov M
// Description of changes: Added new optimization pass that recognizes a loop that counts the number of trailing zeros
// License: Apache 2.0 License
// Original source code: https://github.com/llvm/llvm-project/blob/main/llvm/lib/Transforms/Scalar/LoopIdiomRecognize.cpp
//
//===----------------------------------------------------------------------===//
//
// This pass implements an idiom recognizer that transforms simple loops which
// transforms a simple loop into an cttz intrinsic.
//
// If the loop is completely removed,
// this will give a significant increase in performance.
//
//===----------------------------------------------------------------------===//


#include "LoopIdiomCtz.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopAccessAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/MemorySSAUpdater.h"
#include "llvm/Analysis/MustExecute.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/InstructionCost.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "loop-ctz-idiom"


namespace
{
    class LoopCtzIdiomRecognize
    {
        Loop* CurLoop = nullptr;
        AliasAnalysis* AA;
        DominatorTree* DT;
        LoopInfo* LI;
        ScalarEvolution* SE;
        TargetLibraryInfo* TLI;
        const TargetTransformInfo* TTI;
        const DataLayout* DL;
        OptimizationRemarkEmitter& ORE;
        bool ApplyCodeSizeHeuristics;

    public:
        explicit LoopCtzIdiomRecognize(AliasAnalysis* AA, DominatorTree* DT,
                                       LoopInfo* LI, ScalarEvolution* SE,
                                       TargetLibraryInfo* TLI,
                                       const TargetTransformInfo* TTI,
                                       const DataLayout* DL,
                                       OptimizationRemarkEmitter& ORE)
            : AA(AA), DT(DT), LI(LI), SE(SE), TLI(TLI), TTI(TTI), DL(DL), ORE(ORE)
        {}

        bool runOnLoop(Loop* L);

    private:


        bool runOnNoncountableLoop();



        bool recognizeAndInsertCtz();
        void transformLoopToCtz(BasicBlock* PreCondBB, Instruction* CntInst,
                                PHINode* CntPhi, Value* Var,
                                const DebugLoc& DL, bool ZeroCheck,
                                bool IsCntPhiUsedOutsideLoop);
    };
}

PreservedAnalyses LoopIdiomCtzPass::run(Loop& L, LoopAnalysisManager& AM,
                                        LoopStandardAnalysisResults& AR,
                                        LPMUpdater&)
{
    const auto* DL = &L.getHeader()->getModule()->getDataLayout();

    // For the new PM, we also can't use OptimizationRemarkEmitter as an analysis
    // pass.  Function analyses need to be preserved across loop transformations
    // but ORE cannot be preserved (see comment before the pass definition).
    OptimizationRemarkEmitter ORE(L.getHeader()->getParent());

    LoopCtzIdiomRecognize LIR(&AR.AA, &AR.DT, &AR.LI, &AR.SE, &AR.TLI, &AR.TTI, DL, ORE);
    if (!LIR.runOnLoop(&L))
        return PreservedAnalyses::all();

    auto PA = getLoopPassPreservedAnalyses();

    return PA;
}


bool LoopCtzIdiomRecognize::runOnLoop(Loop* L)
{
    CurLoop = L;
    // If the loop could not be converted to canonical form, it must have an
    // indirectbr in it, just give up.
    if (!L->getLoopPreheader())
        return false;

    // Determine if code size heuristics need to be applied.
    ApplyCodeSizeHeuristics =
        L->getHeader()->getParent()->hasOptSize();

    return runOnNoncountableLoop();
}

bool LoopCtzIdiomRecognize::runOnNoncountableLoop()
{
    LLVM_DEBUG(dbgs() << DEBUG_TYPE " Scanning: F["
        << CurLoop->getHeader()->getParent()->getName()
        << "] Noncountable Loop %"
        << CurLoop->getHeader()->getName() << "\n");

    return recognizeAndInsertCtz();
}

/// Check if the given conditional branch is based on the comparison between
/// a variable and zero, and if the variable is non-zero or zero (JmpOnZero is
/// true), the control yields to the loop entry. If the branch matches the
/// behavior, the variable involved in the comparison is returned. This function
/// will be called to see if the precondition and postcondition of the loop are
/// in desirable form.
static Value* matchCondition(BranchInst* BI, BasicBlock* LoopEntry,
                             bool JmpOnZero = false)
{
    if (!BI || !BI->isConditional())
        return nullptr;

    ICmpInst* Cond = dyn_cast<ICmpInst>(BI->getCondition());
    if (!Cond)
        return nullptr;

    ConstantInt* CmpZero = dyn_cast<ConstantInt>(Cond->getOperand(1));
    if (!CmpZero || !CmpZero->isZero())
        return nullptr;

    BasicBlock* TrueSucc = BI->getSuccessor(0);
    BasicBlock* FalseSucc = BI->getSuccessor(1);
    if (JmpOnZero)
        std::swap(TrueSucc, FalseSucc);

    ICmpInst::Predicate Pred = Cond->getPredicate();
    if ((Pred == ICmpInst::ICMP_NE && TrueSucc == LoopEntry) ||
        (Pred == ICmpInst::ICMP_EQ && FalseSucc == LoopEntry))
    {
        return Cond->getOperand(0);
    }
    return nullptr;
}

// Check if the recurrence variable `VarX` is in the right form to create
// the idiom. Returns the value coerced to a PHINode if so.
static PHINode* getRecurrenceVar(Value* VarX, Instruction* DefX,
                                 BasicBlock* LoopEntry)
{
    auto* PhiX = dyn_cast<PHINode>(VarX);
    if (PhiX && PhiX->getParent() == LoopEntry &&
        (PhiX->getOperand(0) == DefX || PhiX->getOperand(1) == DefX))
        return PhiX;
    return nullptr;
}


//This function recognizes a loop that counts the number of trailing zeros
// loop:
// %count.010 = phi i32 [ %add, %while.body ], [ 0, %while.body.preheader ]
// %n.addr.09 = phi i32 [ %shr, %while.body ], [ %n, %while.body.preheader ]
// %add = add nuw nsw i32 %count.010, 1
// %shr = ashr exact i32 %n.addr.09, 1
// %0 = and i32 %n.addr.09, 2
// %cmp1 = icmp eq i32 %0, 0
// br i1 %cmp1, label %while.body, label %if.end.loopexit
static bool detectShiftUntilZeroAndOneIdiom(Loop* CurLoop, Value*& InitX,
                                            Instruction*& CntInst, PHINode*& CntPhi,
                                            Instruction*& DefX)
{
    BasicBlock* LoopEntry;
    Value* VarX;

    DefX = nullptr;
    CntInst = nullptr;
    CntPhi = nullptr;
    LoopEntry = *(CurLoop->block_begin());

    // step 1: Check if the loop-back branch is in desirable form.
    //  "if (x == 0) goto loop-entry"
    if (Value* T = matchCondition(
        dyn_cast<BranchInst>(LoopEntry->getTerminator()), LoopEntry, true))
    {
        DefX = dyn_cast<Instruction>(T);
    }
    else
    {
        LLVM_DEBUG(dbgs() << "Bad condition for branch instruction\n");
        return false;
    }

    // here in naive case DefX must be a "and" instruction
    // step 2: detect instructions corresponding to "(x & 2)" , that eqiuvalent to "(x >> 1) & 1"

    if (!DefX || DefX->getOpcode() != Instruction::And)
        return false;

    ConstantInt* And;
    if ((And = dyn_cast<ConstantInt>(DefX->getOperand(1))))
        VarX = DefX->getOperand(0);
    else
    {
        VarX = DefX->getOperand(1);
        And = dyn_cast<ConstantInt>(DefX->getOperand(0));
    }

    // operand compares with 2, becouse we are looking for "x & 2"
    // which was optimized by previous pases from "(x >> 1) & 1"
    if (!And || !And->equalsInt(2))
        return false;

    // check if VarX is a phi node

    auto* PhiX = dyn_cast<PHINode>(VarX);

    if (!PhiX || PhiX->getParent() != LoopEntry)
        return false;

    Instruction* DefXRShift = nullptr;

    //check if PhiX has a shift instruction as a operand, which is a "x >> 1"

    for (int i = 0; i < 2; ++i)
    {
        if (auto* Inst = dyn_cast<Instruction>(PhiX->getOperand(i)))
        {
            if (Inst->getOpcode() == Instruction::AShr || Inst->getOpcode() == Instruction::LShr)
            {
                DefXRShift = Inst;
                break;
            }
        }
    }

    if (DefXRShift == nullptr)
    {
        LLVM_DEBUG(dbgs() << "Shift instruction not recognized\n");
        return false;
    }

    // check if the shift instruction is a "x >> 1"
    ConstantInt* Shft = dyn_cast<ConstantInt>(DefXRShift->getOperand(1));
    if (!Shft || !Shft->isOne())
        return false;
    Value* VarXRShift = DefX->getOperand(0);

    // step 3: Check the recurrence of variable X
    PHINode* PhiXRShift = getRecurrenceVar(VarXRShift, DefXRShift, LoopEntry);
    if (!PhiXRShift)
        return false;

    InitX = PhiXRShift->getIncomingValueForBlock(CurLoop->getLoopPreheader());

    // Find the instruction which count the CTTZ: cnt.next = cnt + 1.
    for (Instruction& Inst : llvm::make_range(
             LoopEntry->getFirstNonPHI()->getIterator(), LoopEntry->end()))
    {
        if (Inst.getOpcode() != Instruction::Add)
            continue;

        ConstantInt* Inc = dyn_cast<ConstantInt>(Inst.getOperand(1));
        if (!Inc || (!Inc->isOne() && !Inc->isMinusOne()))
            continue;

        PHINode* Phi = getRecurrenceVar(Inst.getOperand(0), &Inst, LoopEntry);
        if (!Phi)
            continue;

        CntInst = &Inst;
        CntPhi = Phi;
        break;
    }
    if (!CntInst)
        return false;
    DefX = DefXRShift;

    return true;
}


/// Recognize CTTZ idiom in a non-countable loop and convert it to countable with CTTZ of variable as a trip count.
/// If  CTTZ was inserted, returns true; otherwise, returns false.
///
// int count_trailing_zeroes(int n) {
// int count = 0;
// if (n == 0){
//     return 0;
// }
// while ((n & 1) == 0) {
//     count += 1;
//     n >>= 1;
// }
//
//
// return count;
// }
bool LoopCtzIdiomRecognize::recognizeAndInsertCtz()
{
    // Give up if the loop has multiple blocks or multiple backedges.
    if (CurLoop->getNumBackEdges() != 1 || CurLoop->getNumBlocks() != 1)
        return false;

    Value* InitX;
    Instruction* DefX = nullptr;
    PHINode* CntPhi = nullptr;
    Instruction* CntInst = nullptr;
    // Help decide if transformation is profitable. For counting trailing zeroes with uncountable loop idiom,
    // this is always 7.
    const size_t IdiomCanonicalSize = 7;

    if (!detectShiftUntilZeroAndOneIdiom(CurLoop, InitX,
                                         CntInst, CntPhi, DefX))
        return false;

    bool IsCntPhiUsedOutsideLoop = false;
    for (User* U : CntPhi->users())
        if (!CurLoop->contains(cast<Instruction>(U)))
        {
            IsCntPhiUsedOutsideLoop = true;
            break;
        }
    bool IsCntInstUsedOutsideLoop = false;
    for (User* U : CntInst->users())
        if (!CurLoop->contains(cast<Instruction>(U)))
        {
            IsCntInstUsedOutsideLoop = true;
            break;
        }
    // If both CntInst and CntPhi are used outside the loop the profitability
    // is questionable.
    if (IsCntInstUsedOutsideLoop && IsCntPhiUsedOutsideLoop)
        return false;

    BasicBlock* PH = CurLoop->getLoopPreheader();

    auto* PreCondBB = PH->getSinglePredecessor();
    if (!PreCondBB)
        return false;
    auto* PreCondBI = dyn_cast<BranchInst>(PreCondBB->getTerminator());
    if (!PreCondBI || PreCondBI->isUnconditional())
        return false;

    // check that initial value is not zero and "(init & 1) == 0"
    // initial value must not be zero, because it will cause infinite loop
    // without this check, after replacing the loop with ctz, the counter will be only 64,
    // while before the replacement the loop would have executed indefinitely


    if (PreCondBI->getSuccessor(0) != PH)
        return false;

    auto* AndOfCond = dyn_cast<Instruction>(PreCondBI->getCondition());
    if (!AndOfCond || AndOfCond->getOpcode() != Instruction::And)
        return false;


    using namespace PatternMatch;

    ICmpInst::Predicate FirstPred;
    Value* FirstInitCondX;

    ICmpInst::Predicate SecondPred;
    Value* SecondInitCondX;
    // match that case, where n is initial value
    // %cmp = icmp ne i32 %n, 0
    // %and5 = and i32 %n, 1
    // %cmp16 = icmp eq i32 %and5, 0
    // %or.cond = and i1 %cmp, %cmp16

    if (!match(AndOfCond, m_c_And(m_ICmp(FirstPred, m_Value(FirstInitCondX), m_Zero()),
                                  m_ICmp(SecondPred, m_And(m_Value(SecondInitCondX), m_One()), m_Zero()))))
        return false;

    if (FirstPred != ICmpInst::ICMP_NE || SecondPred != ICmpInst::ICMP_EQ)
        return false;
    if (!(FirstInitCondX == InitX && SecondInitCondX == InitX))
        return false;

    bool ZeroCheck = true;

    // Check if CTTZ intrinsic is profitable. Assume it is always
    // profitable if we delete the loop.

    // the loop has only 7 instructions:
    // %count.07 = phi i32 [ %add, %while.body ], [ 0, %while.body.preheader ]
    // %n.addr.06 = phi i32 [ %shr, %while.body ], [ %n, %while.body.preheader ]
    // %add = add nuw nsw i32 %count.07, 1
    // %shr = ashr i32 %n.addr.06, 1
    // %0 = and i32 %n.addr.06, 2
    // %cmp1 = icmp eq i32 %0, 0
    // br i1 %cmp1, label %while.body, label %cleanup.loopexit

    const Value* Args[] = {
        InitX,
        ConstantInt::getBool(InitX->getContext(), ZeroCheck)
    };

    // @llvm.dbg doesn't count as they have no semantic effect.
    auto InstWithoutDebugIt = CurLoop->getHeader()->instructionsWithoutDebug();
    uint32_t HeaderSize =
        std::distance(InstWithoutDebugIt.begin(), InstWithoutDebugIt.end());

    IntrinsicCostAttributes Attrs(Intrinsic::cttz, InitX->getType(), Args);
    InstructionCost Cost =
        TTI->getIntrinsicInstrCost(Attrs, TargetTransformInfo::TCK_SizeAndLatency);
    if (HeaderSize != IdiomCanonicalSize &&
        Cost > TargetTransformInfo::TCC_Basic)
        return false;

    transformLoopToCtz(PH, CntInst, CntPhi, InitX,
                       DefX->getDebugLoc(), ZeroCheck,
                       IsCntPhiUsedOutsideLoop);
    return true;
}

static CallInst* createFFSIntrinsic(IRBuilder<>& IRBuilder, Value* Val,
                                    const DebugLoc& DL, bool ZeroCheck,
                                    Intrinsic::ID IID)
{
    Value* Ops[] = {Val, IRBuilder.getInt1(ZeroCheck)};
    Type* Tys[] = {Val->getType()};

    Module* M = IRBuilder.GetInsertBlock()->getParent()->getParent();
    Function* Func = Intrinsic::getDeclaration(M, IID, Tys);
    CallInst* CI = IRBuilder.CreateCall(Func, Ops);
    CI->setDebugLoc(DL);

    return CI;
}

void LoopCtzIdiomRecognize::transformLoopToCtz(
    BasicBlock* Preheader, Instruction* CntInst,
    PHINode* CntPhi, Value* InitX, const DebugLoc& DL,
    bool ZeroCheck, bool IsCntPhiUsedOutsideLoop)
{
    BranchInst* PreheaderBr = cast<BranchInst>(Preheader->getTerminator());

    // Step 1: Insert the CTTZ instruction at the end of the preheader block
    IRBuilder<> Builder(PreheaderBr);
    Builder.SetCurrentDebugLocation(DL);
    Value* Count =
        createFFSIntrinsic(Builder, InitX, DL, ZeroCheck, Intrinsic::cttz);
    Type* CountTy = Count->getType();
    Value* NewCount = Count;

    NewCount = Builder.CreateZExtOrTrunc(NewCount, CntInst->getType());

    Value* CntInitVal = CntPhi->getIncomingValueForBlock(Preheader);
    if (cast<ConstantInt>(CntInst->getOperand(1))->isOne())
    {
        // If the counter was being incremented in the loop, add NewCount to the
        // counter's initial value, but only if the initial value is not zero.
        ConstantInt* InitConst = dyn_cast<ConstantInt>(CntInitVal);
        if (!InitConst || !InitConst->isZero())
            NewCount = Builder.CreateAdd(NewCount, CntInitVal);
    }
    else
    {
        // If the count was being decremented in the loop, subtract NewCount from
        // the counter's initial value.
        NewCount = Builder.CreateSub(CntInitVal, NewCount);
    }

    // Step 2: Insert new IV and loop condition:
    // loop:
    //   ...
    //   PhiCount = PHI [Count, Dec]
    //   ...
    //   Dec = PhiCount - 1
    //   ...
    //   Br: loop if (Dec != 0)
    BasicBlock* Body = *(CurLoop->block_begin());
    auto* LbBr = cast<BranchInst>(Body->getTerminator());
    ICmpInst* LbCond = cast<ICmpInst>(LbBr->getCondition());

    PHINode* TcPhi = PHINode::Create(CountTy, 2, "tcphi");
    TcPhi->insertBefore(Body->begin());

    Builder.SetInsertPoint(LbCond);
    Instruction* TcDec = cast<Instruction>(Builder.CreateSub(
        TcPhi, ConstantInt::get(CountTy, 1), "tcdec", false, true));

    TcPhi->addIncoming(Count, Preheader);
    TcPhi->addIncoming(TcDec, Body);

    CmpInst::Predicate Pred =
        (LbBr->getSuccessor(0) == Body) ? CmpInst::ICMP_NE : CmpInst::ICMP_EQ;
    LbCond->setPredicate(Pred);
    LbCond->setOperand(0, TcDec);
    LbCond->setOperand(1, ConstantInt::get(CountTy, 0));

    // Step 3: All the references to the original counter outside
    //  the loop are replaced with the NewCount
    if (IsCntPhiUsedOutsideLoop)
        CntPhi->replaceUsesOutsideBlock(NewCount, Body);
    else
        CntInst->replaceUsesOutsideBlock(NewCount, Body);

    // step 4: Forget the "non-computable" trip-count SCEV associated with the
    //   loop. The loop would otherwise not be deleted even if it becomes empty.
    SE->forgetLoop(CurLoop);
}

PassPluginLibraryInfo getLoopCtzIdiomRecognizePluginInfo()
{
    return {
        LLVM_PLUGIN_API_VERSION, DEBUG_TYPE, LLVM_VERSION_STRING,
        [](PassBuilder& PB)
        {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, LoopPassManager& LPM,
                   ArrayRef<PassBuilder::PipelineElement>)
                {
                    if (Name == "loop-ctz-idiom")
                    {
                        LPM.addPass(LoopIdiomCtzPass());

                        return true;
                    }
                    return false;
                });
        }
    };
}

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo
llvmGetPassPluginInfo()
{
    return getLoopCtzIdiomRecognizePluginInfo();
}
