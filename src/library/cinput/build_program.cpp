/*
 * Copyright 2014, IST Austria
 *
 * This file is part of TARA.
 *
 * TARA is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * TARA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with TARA.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <string> 
#include "build_program.h"
#include "helpers/z3interf.h"
#include <z3.h>
using namespace tara;
using namespace tara::cinput;
using namespace tara::helpers;

#include <stdexcept>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

// pragam'ed to aviod warnings due to llvm included files

#include "llvm/IRReader/IRReader.h"

#include "llvm/IR/InstIterator.h"

#include "llvm/LinkAllPasses.h"
#include "llvm/Pass.h"
#include "llvm/PassManager.h"
#include "llvm/IR/DebugInfo.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/CFG.h"

// #include "llvm/IR/InstrTypes.h"
// #include "llvm/IR/BasicBlock.h"

#include "llvm/IRReader/IRReader.h"
// #include "llvm/Support/Debug.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Dwarf.h"

#include "llvm/Analysis/CFGPrinter.h"

#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "llvm/ADT/StringMap.h"

#pragma GCC diagnostic pop


//----------------------------------------------------------------------------
// LLVM utils


std::string getInstructionLocationName(const llvm::Instruction* I ) {
  const llvm::DebugLoc d = I->getDebugLoc();
  unsigned line = d.getLine();
  unsigned col  = d.getCol();

  return "_l"+std::to_string(line)+"_c"+std::to_string(col);
}

void initBlockCount( llvm::Function &f,
                     std::map<llvm::BasicBlock*, unsigned>& block_to_id) {
  unsigned l = 0;
  block_to_id.clear();
  for( auto it  = f.begin(), end = f.end(); it != end; it++ ) {
    llvm::BasicBlock* b = &(*it);
    // auto new_l = program->createFreshLocation(threadId);
    block_to_id[b] = l++;
    // if( b->getName() == "err" ) {
    //   program->setErrorLocation( threadId, new_l );
    // }
  }
    // llvm::BasicBlock& b  = f.getEntryBlock();
    // unsigned entryLoc = getBlockCount( &b );
    // program->setStartLocation( threadId, entryLoc );
    // auto finalLoc = program->createFreshLocation(threadId);
    // program->setFinalLocation( threadId, finalLoc );
}

void removeBranchingOnPHINode( llvm::BranchInst *branch ) {
    if( branch->isUnconditional() ) return;
    llvm::Value* cond = branch->getCondition();
    if( llvm::PHINode* phi = llvm::dyn_cast<llvm::PHINode>(cond) ) {
      llvm::BasicBlock* phiBlock = branch->getParent();
      llvm::BasicBlock* phiDstTrue = branch->getSuccessor(0);
      llvm::BasicBlock* phiDstFalse = branch->getSuccessor(1);
      if( phiBlock->size() == 2 && cond == (phiBlock->getInstList()).begin() &&
          phi->getNumIncomingValues() == 2 ) {
        llvm::Value* val0      = phi->getIncomingValue(0);
        llvm::BasicBlock* src0 = phi->getIncomingBlock(0);
        llvm::BranchInst* br0  = (llvm::BranchInst*)(src0->getTerminator());
        unsigned br0_branch_idx = ( br0->getSuccessor(0) ==  phiBlock ) ? 0 : 1;
        if( llvm::ConstantInt* b = llvm::dyn_cast<llvm::ConstantInt>(val0) ) {
          assert( b->getType()->isIntegerTy(1) );
          llvm::BasicBlock* newDst = b->getZExtValue() ?phiDstTrue:phiDstFalse;
          br0->setSuccessor( br0_branch_idx, newDst );
        }else{std::cerr << "unseen case, not known how to handle!";}
        llvm::Value*      val1 = phi->getIncomingValue(1);
        llvm::BasicBlock* src1 = phi->getIncomingBlock(1);
        llvm::BranchInst* br1  = (llvm::BranchInst*)(src1->getTerminator());
        llvm::Instruction* cmp1 = llvm::dyn_cast<llvm::Instruction>(val1);
        assert( cmp1 );
        assert( br1->isUnconditional() );
        if( cmp1 != NULL && br1->isUnconditional() ) {
          llvm::BranchInst *newBr =
            llvm::BranchInst::Create( phiDstTrue, phiDstFalse, cmp1);
          llvm::ReplaceInstWithInst( br1, newBr );
          // TODO: memory leaked here? what happend to br1 was it deleted??
          llvm::DeleteDeadBlock( phiBlock );
          removeBranchingOnPHINode( newBr );
        }else{std::cerr << "unseen case, not known how to handle!";}
      }else{std::cerr << "unseen case, not known how to handle!";}
    }
  }

//----------------------------------------------------------------------------
// Splitting for assume pass
  char SplitAtAssumePass::ID = 0;
  bool SplitAtAssumePass::runOnFunction( llvm::Function &f ) {
    llvm::LLVMContext &C = f.getContext();
    llvm::BasicBlock *dum = llvm::BasicBlock::Create( C, "dummy", &f, f.end());
    new llvm::UnreachableInst(C, dum);
    llvm::BasicBlock *err = llvm::BasicBlock::Create( C, "err", &f, f.end() );
    new llvm::UnreachableInst(C, err);

    std::vector<llvm::BasicBlock*> splitBBs;
    std::vector<llvm::CallInst*> calls;
    std::vector<llvm::Value*> args;
    std::vector<llvm::Instruction*> splitIs;
    std::vector<bool> isAssume;

     //collect calls to assume statements
    auto bit = f.begin(), end = f.end();
    for( ; bit != end; ++bit ) {
      llvm::BasicBlock* bb = bit;
      auto iit = bb->begin(), iend = bb->end();
      size_t bb_sz = bb->size();
      for( unsigned i = 0; iit != iend; ++iit ) {
	i++;
        llvm::Instruction* I = iit;
        if( llvm::CallInst* call = llvm::dyn_cast<llvm::CallInst>(I) ) {
          llvm::Function* fp = call->getCalledFunction();
	  if( fp != NULL &&
              ( fp->getName() == "_Z6assumeb" ||
                fp->getName() == "_Z6assertb" ) &&
              i < bb_sz ) {
	    splitBBs.push_back( bb );
            auto arg_iit = iit;
	    llvm::Instruction* arg = --arg_iit;
            llvm::Value * arg0 = call->getArgOperand(0);
	    if( arg != arg0 ) std::cerr << "last out not passed!!";
            calls.push_back( call );
            args.push_back( arg0 );
	    auto local_iit = iit;
            splitIs.push_back( ++local_iit );
            isAssume.push_back( (fp->getName() == "_Z6assumeb") );
	  }
        }
      }
    }
    // Reverse order splitting for the case if there are more than
    // one assumes in one basic block
    for( unsigned i = splitBBs.size(); i != 0 ; ) { i--;
      llvm::BasicBlock* head = splitBBs[i];
      llvm::BasicBlock* tail = llvm::SplitBlock( head, splitIs[i], this );
      llvm::Instruction* cmp;
      if( llvm::Instruction* c = llvm::dyn_cast<llvm::Instruction>(args[i]) ) {
        cmp = c;
      }else{ std::cerr << "instruction expected here!!"; }
      llvm::BasicBlock* elseBlock = isAssume[i] ? dum : err;
      llvm::BranchInst *branch =llvm::BranchInst::Create( tail, elseBlock, cmp);
      llvm::ReplaceInstWithInst( head->getTerminator(), branch );
      calls[i]->eraseFromParent();
      if( llvm::isa<llvm::PHINode>(cmp) ) {
	removeBranchingOnPHINode( branch );
      }
    }
    return false;
  }

  void SplitAtAssumePass::getAnalysisUsage(llvm::AnalysisUsage &au) const {
    // it modifies blocks therefore may not preserve anything
    // au.setPreservesAll();
    //TODO: ...
    // au.addRequired<llvm::Analysis>();
  }
//----------------------------------------------------------------------------
// code for input object generation

char build_program::ID = 0;
void
build_program::join_histories( const std::vector< llvm::BasicBlock* > preds,
                               const std::vector< split_history >& hs,
                               split_history& h,
                               std::map< llvm::BasicBlock*, z3::expr >& conds) {
  h.clear();
  if(hs.size() == 0 ) return;
  if(hs.size() == 1 ) {
    h = hs[0];
    return;
  }
  auto sz = hs[0].size();
  for( auto& old_h: hs ) sz = std::min(sz, old_h.size());
  // unsigned sz = min( h1.size(), h2.size() );
  unsigned i=0;
  for( ; i < sz; i++ ) {
    const split_step& s = hs[0][i];
    bool diff_found = false;
    for( auto& old_h: hs ) {
      if( !( z3::eq( s.cond, old_h[i].cond ) ) ) {
        diff_found = true;
        break;
      }
    }
    if( diff_found ) break;
    h.push_back( s );
  }
  if( i == sz ) {
    bool longer_found = false;
    for( auto& old_h: hs ) if( sz != old_h.size() ) longer_found = true;
    if( longer_found ) {
      throw std::runtime_error( "histories can not be subset of each other!!" );
    }
  }else{
    z3::expr c = z3.mk_false();
    unsigned j = 0;
    for( auto& old_h: hs ) {
      unsigned i1 = i;
      z3::expr c1 = z3.mk_true();
      for(;i1 < old_h.size();i1++) {
        c1 = c1 && old_h[i1].cond;
      }
      auto pr = std::pair<llvm::BasicBlock*,z3::expr>( preds[j++], c1 );
      conds.insert( pr );
      c = c || c1;
    }
    c = c.simplify();
    if( z3::eq( c, z3.mk_true() ) )
      return;
    split_step ss( NULL, 0, 0, c);// todo: can store meaningful information
    h.push_back(ss);
  }
}

z3::expr
build_program::translateBlock( unsigned thr_id,
                               const llvm::BasicBlock* b,
                               hb_enc::se_set& prev_events,
                               std::map<llvm::BasicBlock*,z3::expr>& conds) {
  assert(b);
  z3::expr block_ssa = z3.mk_true();
  // std::vector<typename EHandler::expr> blockTerms;
  // //iterate over instructions
  for( const llvm::Instruction& Iobj : b->getInstList() ) {
    const llvm::Instruction* I = &(Iobj);
    assert( I );
    Z3_ast term = z3.mk_emptyexpr();
    bool recognized = false, record = false;
    if( auto wr = llvm::dyn_cast<llvm::StoreInst>(I) ) {
      llvm::Value* v = wr->getOperand(0);
      llvm::GlobalVariable* g = (llvm::GlobalVariable*)wr->getOperand(1);
      cssa::variable gv = p->get_global( (std::string)(g->getName()) );
      std::string loc_name = getInstructionLocationName( I );
      cssa::variable ssa_var = gv + "#" + loc_name;
      auto wr_e = mk_se_ptr( z3.c, hb_encoding, thr_id, 0, ssa_var, gv,
                             loc_name, hb_enc::event_kind_t::w );
      wr_e->set_pre_ses( prev_events );
      prev_events.clear() prev_events.insert( wr_e );
      block_ssa = block_ssa && ( ssa_var == getTerm( v, m ));
      p->add_event( thr_id, wr_e );
    }
    if( auto bop = llvm::dyn_cast<llvm::BinaryOperator>(I) ) {
      llvm::Value* op0 = bop->getOperand( 0 );
      llvm::Value* op1 = bop->getOperand( 1 );
      z3::expr a = getTerm( op0, m );
      z3::expr b = getTerm( op1, m );
      unsigned op = bop->getOpcode();
      switch( op ) {
      case llvm::Instruction::Add : insert_term_map( I, a+b, m); break;
      case llvm::Instruction::Sub : insert_term_map( I, a-b, m); break;
      case llvm::Instruction::Mul : insert_term_map( I, a*b, m); break;
      case llvm::Instruction::Xor : insert_term_map( I, a xor b, m); break;
      case llvm::Instruction::SDiv: insert_term_map( I, a/b, m); break;
      default: {
        const char* opName = bop->getOpcodeName();
        std::cerr << "unsupported instruction " << opName << " occurred!!";
      }
    }
	record = true;
	assert( !recognized );recognized = true;
    }
    if( const llvm::UnaryInstruction* str =
        llvm::dyn_cast<llvm::UnaryInstruction>(I) ) {
      if( const llvm::LoadInst* load = llvm::dyn_cast<llvm::LoadInst>(I) ) {
        llvm::GlobalVariable* g = load->getOperand(0);
        const llvm::Value* v = I;
        cssa::variable gv = p->get_global( (std::string)(g->getName()) );
        std::string loc_name = getInstructionLocationName( I );
        cssa::variable ssa_var = gv + "#" + loc_name;
        auto rd_e = mk_se_ptr( z3.c, hb_encoding, thr_id, 0, ssa_var, gv,
                               loc_name, hb_enc::event_kind_t::r );
        rd_e->set_pre_ses( prev_events );
        prev_events.clear() prev_events.insert( rd_e );
        z3::expr l_v = getTerm( I, m);
        block_ssa = block_ssa && ( ssa_var == l_v);
  //       record = true;
  //       assert( !recognized );recognized = true;
      }else{
        assert( false );
  //       cfrontend_warning( "I am uniary instruction!!" );
  //       assert( !recognized );recognized = true;
      }
    }
    if( const llvm::CmpInst* cmp = llvm::dyn_cast<llvm::CmpInst>(I) ) {
      llvm::Value* lhs = cmp->getOperand( 0 );
      llvm::Value* rhs = cmp->getOperand( 1 );
      z3::expr l = getTerm( lhs, m );
      z3::expr r = getTerm( rhs, m );
      llvm::CmpInst::Predicate pred = cmp->getPredicate();

      switch( pred ) {
      case llvm::CmpInst::ICMP_EQ : insert_term_map( I, l==r, m ); break;
      case llvm::CmpInst::ICMP_NE : insert_term_map( I, l!=r, m ); break;
      case llvm::CmpInst::ICMP_UGT : insert_term_map( I, l>r, m ); break;
      case llvm::CmpInst::ICMP_UGE : insert_term_map( I, l>=r, m ); break;
      case llvm::CmpInst::ICMP_ULT : insert_term_map( I, l<r, m ); break;
      case llvm::CmpInst::ICMP_ULE : insert_term_map( I, l<=r, m ); break;
      case llvm::CmpInst::ICMP_SGT : insert_term_map( I, l>r, m ); break;
      case llvm::CmpInst::ICMP_SGE : insert_term_map( I, l>=r, m ); break;
      case llvm::CmpInst::ICMP_SLT : insert_term_map( I, l<r, m ); break;
      case llvm::CmpInst::ICMP_SLE : insert_term_map( I, l<=r, m ); break;
      default: {
        std::cerr << "unsupported predicate in compare instruction" << pred << "!!";
      }
     }
      record = true;
      assert( !recognized );recognized = true;
    }
  //   if( const llvm::PHINode* phi = llvm::dyn_cast<llvm::PHINode>(I) ) {
  //     term = getPhiMap( phi, m);
  //     record = 1;
  //     assert( !recognized );recognized = true;
  //   }
  //   if( auto ret = llvm::dyn_cast<llvm::ReturnInst>(I) ) {
  //     llvm::Value* v = ret->getReturnValue();
  //     if( v ) {
  //       typename EHandler::expr retTerm = getTerm( v, m );
  //       //todo: use retTerm somewhere
  //     }
  //     if( config.verbose("mkthread") )
  //       cfrontend_warning( "return value ignored!!" );
  //     assert( !recognized );recognized = true;
  //   }
  //   if( auto unreach = llvm::dyn_cast<llvm::UnreachableInst>(I) ) {
  //     if( config.verbose("mkthread") )
  //       cfrontend_warning( "unreachable instruction ignored!!" );
  //     assert( !recognized );recognized = true;
  //   }

  //   // UNSUPPORTED_INSTRUCTIONS( ReturnInst,      I );
  //   UNSUPPORTED_INSTRUCTIONS( InvokeInst,      I );
  //   UNSUPPORTED_INSTRUCTIONS( IndirectBrInst,  I );
  //   // UNSUPPORTED_INSTRUCTIONS( UnreachableInst, I );

    if( const llvm::BranchInst* br = llvm::dyn_cast<llvm::BranchInst>(I) ) {
      if(  br->isConditional() ) {
        llvm::Value* c = br->getCondition();
        z3::expr bit = getTerm( c, m);
        block_to_exit_bit[b] = bit;
      }else{
        z3::expr b = get_fresh_bool();
        block_to_exit_bit[b] = bit;
      }
  //     assert( !recognized );recognized = true;
    }
    if( const llvm::SwitchInst* t = llvm::dyn_cast<llvm::SwitchInst>(I) ) {
  //     cfrontend_warning( "I am switch!!" );
  //     assert( !recognized );recognized = true;
    }
    if( const llvm::CallInst* str = llvm::dyn_cast<llvm::CallInst>(I) ) {
      if( const llvm::DbgValueInst* dVal =
          llvm::dyn_cast<llvm::DbgValueInst>(I) ) {
        // Ignore debug instructions
      }else{
        // todo... deal with callers
        // cfrontend_warning( "I am caller!!" );
      }
      assert( !recognized );recognized = true;
    }
  //   // Store in term map the result of current instruction
  //   if( record ) {
  //     if( eHandler->isLocalVar( I ) ) {
  //       typename EHandler::expr lp     = eHandler->getLocalVarNext( I );
  //       typename EHandler::expr assign = eHandler->mkEq( lp, term );
  //       blockTerms.push_back( assign );
  //     }else{
  //       const llvm::Value* v = I;
  //       eHandler->insertMap( v, term, m);
  //     }
    }
  //   if( !recognized ) cfrontend_error( "----- failed to recognize!!" );
  }
  // // print_double_line( std::cout );
  // //iterate over instructions and build
  return block_ssa;
}

bool build_program::runOnFunction( llvm::Function &f ) {
  thread_count++;
  std::string name = (std::string)f.getName();
  unsigned threadId = p->add_thread( name );

  initBlockCount( f, block_to_id );


  for( auto it = f.begin(), end = f.end(); it != end; it++ ) {
    llvm::BasicBlock* src = &(*it);
    if( o.print_input > 0 ) src->print( llvm::outs() );

    std::vector< split_history > histories; // needs to be ref
    std::vector<llvm::BasicBlock*> preds;
    hb_enc::se_set prev_events;
    for(auto PI = llvm::pred_begin(src),E = llvm::pred_end(src);PI != E;++PI){
      llvm::BasicBlock *prev = *PI;
      split_history h = block_to_split_stack[prev];
      if( llvm::isa<llvm::BranchInst>( prev->getTerminator() ) ) {
        z3::expr& b = block_to_exit_bit.at(prev);
        // llvm::TerminatorInst *term= prev->getTerminator();
        unsigned succ_num = PI.getOperandNo();
        if( succ_num == 1 ) b = !b;
        split_step ss(prev, block_to_id[prev], succ_num, b);
        h.push_back(ss);
      }
      histories.push_back(h);
      hb_enc::se_set& prev_trail = block_to_trailing_events.at(prev);
      prev_events.insert( prev_trail.begin(), prev_trail.end() );
      preds.push_back( prev );
    }
    split_history h;
    std::map<llvm::BasicBlock*, z3::expr> conds;
    join_histories( preds, histories, h, conds);
    block_to_split_stack[src] = h;

    z3::expr ssa = translateBlock( thread_count, src, prev_events, conds);
    p->append_ssa( ssa );

  //     typename EHandler::expr e = translateBlock( src, 0, exprMap );

  //   unsigned srcLoc = getBlockCount( src );
  //   llvm::TerminatorInst* c= (*it).getTerminator();
  //   unsigned num = c->getNumSuccessors();
  //   if( num == 0 ) {
  //     typename EHandler::expr e = translateBlock( src, 0, exprMap );
  //     auto dstLoc = program->getFinalLocation( threadId );
  //     program->addBlock( threadId, srcLoc, dstLoc, e );
  //   }else{
  //     for( unsigned i=0; i < num; ++i ) {
  //       // ith branch may be tranlate differently
  //       typename EHandler::expr e = translateBlock( src, i, exprMap );
  //       llvm::BasicBlock* dst = c->getSuccessor(i);
  //       unsigned dstLoc = getBlockCount( dst );
  //       program->addBlock( threadId, srcLoc, dstLoc, e );
  //     }
  //   }
  }
  // applyPendingInsertEdges( threadId );

  return false;
}
