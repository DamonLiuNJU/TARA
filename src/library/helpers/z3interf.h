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

#ifndef Z3INTERF_H
#define Z3INTERF_H

#include <z3++.h>
#include <vector>
#include <tuple>
#include <unordered_set>
#include <unordered_map>
#include "helpers/helpers.h"
#include "program/variable.h"

#include <boost/regex.hpp>

namespace tara {
namespace helpers {



class z3interf
{
public:
  z3interf(z3::context& ctx);
  ~z3interf();
  z3::context& c;
private:
  z3interf(z3interf const & s);
  z3interf & operator=(z3interf const & s);

  //void add_hb(z3::expr a, std::string a_name, z3::expr b, std::string b_name);

  void _printFormula(const z3::expr& ast, std::ostream& out);

  static void get_variables(const z3::expr& expr, tara::variable_set& result);

  // hb_enc::integer _hb_encoding = hb_enc::integer(c);

  //
  // functions for internal fu-malik maxsat
  //
  void assert_soft_constraints( z3::solver&s , std::vector<z3::expr>& cnstrs,
                                std::vector<z3::expr>& aux_vars );
  z3::expr at_most_one( z3::expr_vector& vars );
  int fu_malik_maxsat_step( z3::solver &s, std::vector<z3::expr>& soft,
                            std::vector<z3::expr>& aux_vars );

  z3::model fu_malik_maxsat( z3::expr hard, std::vector<z3::expr>& soft );
  //-----------------------------

public:

  struct expr_hash {
    size_t operator () (const z3::expr& a) const {
      Z3_ast ap = a;
      size_t hash = std::hash<Z3_ast>()(ap);
      return hash;
    }
  };

  struct expr_equal :
    std::binary_function <z3::expr,z3::expr,bool> {
    bool operator() (const z3::expr& x, const z3::expr& y) const {
      return z3::eq( x, y );
      // return std::equal_to<std::z3::expr>()(x->e_v->name, y->e_v->name);
    }
  };

  // bool isEq( z3::expr& a, z3::expr&b ) {
  //   Z3_ast ap = a;
  //   Z3_ast bp = b;
  //   return ap == bp;
  // }

  // struct exprComp {
  //   bool operator() (const z3::expr& a, const z3::expr& b) const
  //   {
  //     Z3_ast ap = a;
  //     Z3_ast bp = b;
  //     return ap < bp;
  //   }
  // };

  static std::string opSymbol(Z3_decl_kind decl);

  z3::solver create_solver() const;
  static z3::solver create_solver(z3::context& ctx);

  void printFormula(const z3::expr& expr, std::ostream& out);
  z3::expr parseFormula(std::string str, const input::variable_set& vars);
  z3::expr parseFormula(std::string str, const std::vector<std::string>& names, const std::vector<z3::expr>& declarations);
  tara::variable_set translate_variables(input::variable_set vars);

  // getting the information which happens-before relations are important
  static tara::variable_set get_variables(const z3::expr& expr);

  static std::vector<z3::expr> decompose(z3::expr conj, Z3_decl_kind kind);
  static void decompose(z3::expr conj, Z3_decl_kind kind, std::vector< z3::expr >& result);

  // solver calls
  bool is_unsat( z3::expr ) const;
  bool entails( z3::expr e1, z3::expr e2 ) const;

  // test functions
  bool is_false( z3::expr ) const;
  bool is_true( z3::expr ) const;
  bool is_const( z3::expr& b );
  bool is_bool_const( z3::expr& );
  bool matched_sort( const z3::expr& l, const z3::expr& r );

  static bool is_op(const z3::expr& e, const Z3_decl_kind dk_op);
  static bool is_implies(const z3::expr& e);
  static bool is_neg    (const z3::expr& e);
  static bool is_and    (const z3::expr& e);
  static bool is_or     (const z3::expr& e);

  //get/make expression functions
  z3::expr get_fresh_bool( std::string suff = "" );
  z3::expr get_fresh_int(  std::string suff = "" );
  std::string get_top_func_name( z3::expr& b );
  int get_numeral_int(const z3::expr& i) const;
  Z3_decl_kind get_decl_kind( z3::expr e );
  z3::sort mk_sort(const char* s) const { return c.uninterpreted_sort(s); }
  z3::expr mk_true() const { return c.bool_val(true ); }
  z3::expr mk_false() const { return c.bool_val(false); }
  z3::expr mk_emptyexpr(){ return z3::expr(c); }
  z3::expr switch_sort( z3::expr& b, z3::sort& s );
  z3::sort get_variable_sort(const input::variable& var);

  static bool is_true_in_model( const z3::model& m, z3::expr e );
  static bool is_false_in_model( const z3::model& m, z3::expr e );

  //utilities
  z3::model maxsat( z3::expr hard, std::vector<z3::expr>& soft );

  template <class value>
  static bool min_unsat(z3::solver& sol, std::list<value>& items,
                        std::function <z3::expr(value)> translate,
                        bool potentially_input_sat = false);
  template <class value>
  static void remove_implied( const z3::expr& fixed,
                              std::list< value >& items,
                              std::function< z3::expr(value) > translate);
  z3::expr simplify( z3::expr e );
  z3::expr simplify_or_vector( std::vector<z3::expr>& o_vec );

};
  // we need this xor, since the default xor in c++ interface is for bvxor
  inline z3::expr _xor( z3::expr const & a, z3::expr const & b ) {
    check_context(a, b);
    Z3_ast r = Z3_mk_xor(a.ctx(), a, b);
    return z3::expr(a.ctx(), r);
  }


}
  //----------------
  //support for gdb
  void debug_print( std::ostream& out, const z3::expr& );
  void debug_print( const z3::expr& );
  void debug_print( const std::list<z3::expr>& );
  void debug_print( const z3::model& );
  void debug_print( const std::vector< z3::expr >& es );
  void debug_print( const z3::model& m );
  //----------------
}

#endif // Z3INTERF_H
