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

#include "program.h"
#include "cssa_exception.h"
#include "helpers/helpers.h"
#include "helpers/int_to_string.h"
#include <string.h>
#include "helpers/z3interf.h"
using namespace tara;
using namespace tara::cssa;
using namespace tara::helpers;

using namespace std;

z3::expr program::build_rf_val(std::shared_ptr<cssa::instruction> l1,
                                shared_ptr<instruction> l2,
                                z3::context & C, std::string str, bool pre) {

  z3::expr exit=C.bool_val(false);
  z3::expr s = C.bool_const("s");

  for(variable v2: l2->variables_read) {
    if(check_correct_global_variable(v2,str)) {
      if( pre ) {
        for(variable v1: new_globals) {
          if(check_correct_global_variable(v1,str)) {
            z3::expr conjecture= implies (s,(z3::expr)v2==(z3::expr)v1);
            return conjecture;
          }
        }
      }else {
        for(variable v1: l1->variables_write) {
          // ??? unguarded 
          // if(check_correct_global_variable(v1,str)) {
          z3::expr conjecture= implies (s,(z3::expr)v2==(z3::expr)v1);
          return conjecture;
        }
      }
    }
  }

  //std::cout<<"\ninstr_to_var\t"<<instr_to_var[l2]->name<<"\n";
  //std::cout<<"\nvariable_written\t"<<variable_written[str]->instr<<"\n";

  return exit;
}



z3::expr program::build_rf_grf( std::shared_ptr<cssa::instruction> l1,
                                std::shared_ptr<cssa::instruction> l2,
                                z3::context & C, std::string str, bool pre,
                                const input::program& input ) {
  shared_ptr<hb_enc::location> start_location = input.start_name();
  z3::expr s=build_rf_val(l1,l2,C,str,pre);
  z3::expr c=C.bool_const("c");

  if(pre==0) {
    c=_hb_encoding.make_hb(l1->loc,l2->loc);
  }else {
    c=_hb_encoding.make_hb(start_location,l2->loc);
  }

  z3::expr conjecture=implies(s,c);
  return conjecture;
}

z3::expr program::build_rf_some(std::shared_ptr<cssa::instruction> l1,
                                std::shared_ptr<cssa::instruction> l2,
                                std::shared_ptr<cssa::instruction> l3,
                                z3::context & C, std::string str, bool pre) {
  z3::expr s1=C.bool_const("s1");
  z3::expr s2=C.bool_const("s2");

  z3::expr conjecture2=build_rf_val(l3,l1,C,str,false);
  z3::expr conjecture1=C.bool_val(true);
  if( pre ) {
    z3::expr conjecture1=build_rf_val(l2,l1,C,str,true);
  }else {
    z3::expr conjecture1=build_rf_val(l2,l1,C,str,false);
  }

  z3::expr conjecture=(conjecture1 || conjecture2);
  return conjecture;
}





z3::expr program::build_ws(std::shared_ptr<cssa::instruction> l1,
                           std::shared_ptr<cssa::instruction> l2,
                           z3::context & C, bool pre,
                           const input::program& input) {
  shared_ptr<hb_enc::location> start_location = input.start_name();
  z3::expr c1=C.bool_const("c1");
  z3::expr c2=C.bool_const("c2");
  //std::cout<<"\nhere 1 \n";
  if(pre==0) {

    c1=_hb_encoding.make_hb(l1->loc,l2->loc);
    c2=_hb_encoding.make_hb(l2->loc,l1->loc);
  }
  else {
    //std::cout<<"\nhere 3 \n";
    c1=_hb_encoding.make_hb(start_location,l2->loc);
    //std::cout<<"\nhere 4 \n";
    c2=_hb_encoding.make_hb(l2->loc,start_location);
    //std::cout<<"\nhere 5 \n";
  }

  z3::expr conjecture=implies(!c1,c2);
  //std::cout<<"\nhere 6 \n";
  return conjecture;
}


z3::expr program::build_fr( std::shared_ptr<cssa::instruction> l1,
                            std::shared_ptr<cssa::instruction> l2,
                            std::shared_ptr<cssa::instruction> l3,
                            z3::context & C,std::string str,bool pre,
                            bool pre_where, const input::program& input) {
  shared_ptr<hb_enc::location> start_location = input.start_name();
  z3::expr c1=C.bool_const("c1");
  z3::expr c2=C.bool_const("c2");
  z3::expr s=C.bool_const("s");;
  if(pre_where==true) {
    s=build_rf_val(l1,l2,C,str,!pre);
  }
  else {
    s=build_rf_val(l1,l2,C,str,pre);
  }

  //pre tells whether there is an initialisation instruction
  if(pre==0) {
      c1=_hb_encoding.make_hb(l1->loc,l3->loc);
      c2=_hb_encoding.make_hb(l2->loc,l3->loc);
  } else {
    if(pre_where==0) {
      // pre_where tells whether l1 or l3 has initialisations instruction {
      c1=_hb_encoding.make_hb(start_location,l3->loc);
      c2=_hb_encoding.make_hb(l2->loc,l3->loc);
    } else {
      c1=_hb_encoding.make_hb(l1->loc,start_location);
      c2=_hb_encoding.make_hb(l2->loc,start_location);
    }
  }

  z3::expr conjecture=implies((s && c1),c2);
  return conjecture;
}

// ---------------------------------------------------------------------------
// Build symbolic event structure
//
// void program::wmm_build_ses(const input::program& input) {
//   z3::context& c = _z3.c;

//   bool flag=0;
//   unsigned int count_global_occur;
//   unordered_set<std::shared_ptr<instruction>> RF_SOME;
//   unordered_set<std::shared_ptr<instruction>> RF_WS;
//   unordered_map<std::shared_ptr<instruction>,bool> if_pre;
//   std::shared_ptr<cssa::instruction> write3; //for rf_ws
//   std::shared_ptr<cssa::instruction> write4; //for_rf_ws
//   std::shared_ptr<cssa::instruction> write5; // for fr
//   std::shared_ptr<cssa::instruction> write6; // for fr
//   z3::expr conjec1=c.bool_val(true);
//   z3::expr conjec2=c.bool_val(true);
//   z3::expr conjec3=c.bool_val(true);
//   z3::expr conjec4=c.bool_val(true);
//   z3::expr conjec5=c.bool_val(true);
//   z3::expr conjec6=c.bool_val(true);
//   z3::expr conjec7=c.bool_val(true);
//   for(std::string v1:input.globals) {

//     RF_WS.clear();
//     if_pre.clear();
//     count_global_occur=0;
//     std::cout<<"\nglobal variables "<<v1;
//     for(unsigned int k=0;k<threads.size();k++) {
//       for(unsigned int n=0;n<input.threads[k].size();n++) {
//         for(variable v2:(threads[k]->instructions[n])->variables_read_copy) {
//           RF_SOME.clear();
//           flag=0;
          
//           std::cout<<"\nread outside:\t"<<v2<<"\n";
//           if(check_correct_global_variable(v2,v1)) {

//             for(unsigned int k2=0;k2<threads.size();k2++) {
//               // thread& thread = *threads[k2];
//               for(unsigned int n2=0;n2<input.threads[k2].size();n2++) {
//                 //std::cout<<"thread[i].instr\t"<<thread[n2].instr<<"\n";

//                 for(variable v3:(threads[k2]->instructions[n2])->variables_write) {
//                   std::cout<<"\nwrite outside\t"<<v3<<"\n";
//                   if(check_correct_global_variable(v3,v1)) {
//                     std::cout<<"\ninside\n";
//                     if( k2 != k ) {
//                       RF_SOME.insert(threads[k2]->instructions[n2]);
//                       RF_WS.insert(threads[k2]->instructions[n2]);
//                       count_global_occur++;
//                       // std::cout<<"\nread:\t"<<v2<<"\n";
//                       // std::cout<<"\nwrite\t"<<v3<<"\n";
//                       conjec1=build_rf_val( threads[k2]->instructions[n2],
//                                             threads[k]->instructions[n],
//                                             c, v1, false);
//                       rf_val=rf_val && conjec1;
//                       //std::cout<<"\nconjec1 in 1\t"<<conjec1<<"\n";
//                       //std::cout<<"\nrf_val\t"<<rf_val<<"\n";

//                       conjec2=build_rf_grf(threads[k2]->instructions[n2],
//                                            threads[k]->instructions[n],
//                                            c,v1,false,input);
//                       rf_grf=rf_grf && conjec2;
//                       //std::cout<<"\nconjec2 in 1\t"<<conjec2<<"\n";
//                       //std::cout<<"\nrf_grf\t"<<rf_grf<<"\n";

//                     } else if(n2<n) {
//                       RF_SOME.insert(threads[k2]->instructions[n2]);
//                       RF_WS.insert(threads[k2]->instructions[n2]);
//                       count_global_occur++;
//                       // std::cout<<"\nread: "<<v2<<"\n";
//                       // std::cout<<"\nwrite\t"<<v3<<"\n";

//                       conjec1=build_rf_val(threads[k2]->instructions[n2],
//                                            threads[k]->instructions[n],
//                                            c,v1,false);
//                       rf_val=rf_val && conjec1;
//                       // std::cout<<"\nconjec1 in 2\t"<<conjec1<<"\n";
//                       // std::cout<<"\nrf_val\t"<<rf_val<<"\n";

//                       conjec2=build_rf_grf( threads[k2]->instructions[n2],
//                                             threads[k]->instructions[n],
//                                             c,v1,false,input );
//                       rf_grf=rf_grf && conjec2;
//                       // std::cout<<"\nconjec2 in 2\t"<<conjec2<<"\n";
//                       // std::cout<<"\nrf_grf\t"<<rf_grf<<"\n";

//                     }
//                   }
//                 }

//                 if(flag!=1) {
//                   for(variable vi:new_globals) {
//                     if(check_correct_global_variable(vi,v1)) {
//                       RF_SOME.insert(threads[k]->instructions[n]);
//                       RF_WS.insert(threads[k]->instructions[n]);
//                       if_pre[threads[k]->instructions[n]]=1;

//                       count_global_occur++;
//                       // std::cout<<"\nread: "<<v2<<"\n";
//                       // std::cout<<"\nwrite initial\t"<<vi<<"\n";

//                       conjec1=build_rf_val( threads[k]->instructions[n],
//                                             threads[k]->instructions[n],
//                                             c,v1,true);
//                       rf_val=rf_val && conjec1;
//                       // std::cout<<"\nconjec1 in 3\t"<<conjec1<<"\n";
//                       // std::cout<<"\nrf_val\t"<<rf_val<<"\n";

//                       conjec2=build_rf_grf(threads[k]->instructions[n],
//                                            threads[k]->instructions[n],
//                                            c,v1,true,input);
//                       rf_grf=rf_grf && conjec2;
//                       // std::cout<<"\nconjec2 in 3\t"<<conjec2<<"\n";
//                       // std::cout<<"\nrf_grf\t"<<rf_grf<<"\n";

//                       flag=1;
//                       break;
//                     }
//                   }
//                 }
//               }
//             }
//           }

//           if( count_global_occur > 1 ) {

//             unsigned int counter=1;

//             // auto itr=RF_SOME.begin();
//             std::shared_ptr<cssa::instruction> write1; // for rf_some
//             std::shared_ptr<cssa::instruction> write2; // for rf_some

//             for(auto itr=RF_SOME.begin();itr!=RF_SOME.end();itr++) {
//               // std::cout<<"\ncheck itr\t"<<(*itr)->name<<"\n";
//               // std::cout<<"\nif_pre\t"<<if_pre[(*itr)]<<"\n";

//               if(counter%2==1) {
//                 write1=*itr;
//                 if(counter!=1) {
//                   if((if_pre[write1]!=1)&&(if_pre[write2]!=1)) {
//                     conjec3=build_rf_some( threads[k]->instructions[n],
//                                            write1,write2,c,v1,false);
//  //std::cout<<"\nconjec3 in 1\t"<<conjec3<<"\n";
//  // std::cout<<"\nread in some (if) pre=false\n"<<threads[k]->instructions[n]->name;
//  // std::cout<<"\nwrite1 in some (if) pre=false\n"<<write1->name;
//  // std::cout<<"\nwrite2 in some (if) pre=false\n"<<write2->name;
//                   }
//                   else if(if_pre[write2]==1) {
//                     conjec3=build_rf_some( threads[k]->instructions[n],
//                                            write2,write1,c,v1,true);
//  //std::cout<<"\nconjec3 in 2\t"<<conjec3<<"\n";
//  // std::cout<<"\nread in some (if) pre=false\n"<<threads[k]->instructions[n]->name;
//  // std::cout<<"\nwrite1 in some (if) pre=false\n"<<write1->name;
//  // std::cout<<"\nwrite2 in some (if) pre=false\n"<<write2->name;
//                   }
//                   else if(if_pre[write1]==1) {
//                     conjec3=build_rf_some( threads[k]->instructions[n],
//                                            write1,write2,c,v1,true);
//                     //std::cout<<"\nconjec3 in 3\t"<<conjec3<<"\n";
//                     // std::cout<<"\nread in some (else) pre=true\n"<<threads[k]->instructions[n]->name;
//                     // std::cout<<"\nwrite1 in some (else) pre=true\n"<<write1->name;
//                     // std::cout<<"\nwrite2 in some (else) pre=true\n"<<write2->name;
//                   }

//                 }
//               }
//               else if(counter%2==0) {

//                 write2= *itr;

//                 if((if_pre[write1]!=1)&&(if_pre[write2]!=1)) {
//                   conjec3=build_rf_some(threads[k]->instructions[n],
//                                         write1,write2,c,v1,false);
//                   //std::cout<<"\nconjec3 in 1\t"<<conjec3<<"\n";
//                   // std::cout<<"\nread in some (if) pre=false\n"<<threads[k]->instructions[n]->name;
//                   // std::cout<<"\nwrite1 in some (if) pre=false\n"<<write1->name;
//                   // std::cout<<"\nwrite2 in some (if) pre=false\n"<<write2->name;

//                 }
//                 else if(if_pre[write2]==1) {
//                   conjec3=build_rf_some(threads[k]->instructions[n],
//                                         write2,write1,c,v1,true);
//                   //std::cout<<"\nconjec3 in 2\t"<<conjec3<<"\n";
//                   // std::cout<<"\nread in some (if) pre=false\n"<<threads[k]->instructions[n]->name;
//                   // std::cout<<"\nwrite1 in some (if) pre=false\n"<<write1->name;
//                   // std::cout<<"\nwrite2 in some (if) pre=false\n"<<write2->name;
//                 }
//                 else if(if_pre[write1]==1) {
//                   conjec3=build_rf_some(threads[k]->instructions[n],
//                                         write1,write2,c,v1,true);
//                   //std::cout<<"\nconjec3 in 3\t"<<conjec3<<"\n";
//                   // std::cout<<"\nread in some (else) pre=true\n"<<threads[k]->instructions[n]->name;
//                   // std::cout<<"\nwrite1 in some (else) pre=true\n"<<write1->name;
//                   // std::cout<<"\nwrite2 in some (else) pre=true\n"<<write2->name;
//                 }
//               }
//               counter++;
//             }

//           }
//           rf_some= rf_some && conjec3;
//           //std::cout<<"\nrf_some\t"<<rf_some<<"\n";
//           for(auto iterator1=RF_SOME.begin();iterator1!=RF_SOME.end();) {
//             write5=*iterator1;
//             iterator1++;
//             for(auto iterator2=iterator1;iterator2!=RF_SOME.end();iterator2++) {
//               write6=*iterator2;
//               if((if_pre[write5]!=1)&&(if_pre[write6]!=1)) {
//                 // std::cout<<"........\nwrite1 + first if\t"<<write5->name<<"\n";
//                 // std::cout<<"\nread1 + first if\t"<<threads[k]->instructions[n]->name<<"\n";
//                 // std::cout<<"\nwrite2 + first if\t"<<write6->name<<"\n..........";
//                 conjec6=build_fr(write5,threads[k]->instructions[n],write6,
//                                  c,v1,false,false,input);
//                 conjec7=build_fr(write6,threads[k]->instructions[n],write5,
//                                  c,v1,false,false,input);
//                 fr = fr && conjec6 && conjec7;
//                 //std::cout<<"\nfr in 1\t"<<fr<<"\n";
//               }
//               else if(if_pre[write6]==1) {
//                 // std::cout<<"........\nwrite1 + second if\t"<<write5->name<<"\n";
//                 // std::cout<<"\nread1 + second if\t"<<threads[k]->instructions[n]->name<<"\n";
//                 // std::cout<<"\nwrite2 + second if\t"<<write6->name<<"\n........";
//                 //if(if_pre[write5]!=1)
//                 conjec6=build_fr(write5,threads[k]->instructions[n],write6,
//                                  c,v1,true,true,input);
//                 //if(if_pre[write6]!=1)
//                 conjec7=build_fr(write6,threads[k]->instructions[n],write5,
//                                  c,v1,true,false,input);
//                 fr = fr && conjec6 && conjec7;
//                 //std::cout<<"\nfr in 2\t"<<fr<<"\n";
//               }
//               else if(if_pre[write5]==1) {
//                 // std::cout<<".......\nwrite1 + third if\t"<<write5->name<<"\n";
//                 // std::cout<<"\nread1 + third if\t"<<threads[k]->instructions[n]->name<<"\n";
//                 // std::cout<<"\nwrite2 + third if\t"<<write6->name<<"\n......";
//                 conjec6=build_fr( write5,threads[k]->instructions[n],write6,
//                                   c,v1,true,false,input );
//                 conjec7=build_fr( write6,threads[k]->instructions[n],write5,
//                                   c,v1,true,true,input );
//                 fr = fr && conjec6 && conjec7;
//                 //std::cout<<"\nfr in 3\t"<<fr<<"\n";
//               }
//               else {
//                 continue;
//               }
//             }
//           }

//         }
//       }
//     }
//     for(auto itr1=RF_WS.begin();itr1!=RF_WS.end();) {
//       write3=*itr1;

//       itr1++;
//       for(auto itr2=itr1;itr2!=RF_SOME.end();itr2++) {
//         write4=*itr2;
//         //std::cout<<"\ncheck itr\t"<<write4->name<<"\n";
//         if((if_pre[write4]!=1)&&(if_pre[write3]!=1)) {
//           // std::cout<<"\nwrite3 name "<<write3->name<<"\n";
//           // std::cout<<"\nwrite4 name "<<write4->name<<"\n";
//           conjec4=build_ws(write4,write3,c,false,input);
//           rf_ws= rf_ws && conjec4;
//           //std::cout<<"\nrf_ws in 1\t"<<rf_ws<<"\n";
//         }
//         else if((if_pre[write4]==1)) {
//           // std::cout<<"\nwrite3 name "<<write3->name<<"\n";
//           // std::cout<<"write4 name: initialisation instruction\n";
//           conjec4=build_ws(write4,write3,c,true,input);
//           rf_ws= rf_ws && conjec4;
//           //std::cout<<"\nrf_ws in 2\t"<<rf_ws<<"\n";
//         }
//         else if((if_pre[write3]==1)) {
//           // std::cout<<"\nwrite4 name "<<write4->name<<"\n";
//           // std::cout<<"write3 name: initialisation instruction\n";
//           conjec4=build_ws(write3,write4,c,true,input);
//           rf_ws= rf_ws && conjec4;
//           //std::cout<<"\nrf_ws in 3\t"<<rf_ws<<"\n";
//         }
//         else {
//           continue;
//         }
//       }
//     }
//   }
//   std::cout<<"\nrf_val\t"<<rf_val<<"\n";
//   std::cout<<"\nrf_grf\t"<<rf_grf<<"\n";
//   std::cout<<"\nrf_ws\t"<<rf_ws<<"\n";
//   std::cout<<"\nrf_some\t"<<rf_some<<"\n";
//   std::cout<<"\nfr\t"<<fr<<"\n";
// }


// void program::wmm_build_threads(const input::program& input) {
//   z3::context& c = _z3.c;
//   vector<pi_needed> pis;

//   // for counting occurences of each variable
//   unordered_map<string, int> count_occur;

//   for (string v: input.globals) {
//     count_occur[v]=0;
//   }

//   // maps variable to the last place they were assigned
//   unordered_map<string, string> thread_vars;

//   for (unsigned t=0; t<input.threads.size(); t++) {
//     // last reference to global variable in this thread (if any)
//     unordered_map<string, shared_ptr<instruction>> global_in_thread;

//     // set all local vars to "pre"
//     for (string v: input.threads[t].locals) {
//       thread_vars[v] = "pre";
//       count_occur[v]=0;
//     }

//     z3::expr path_constraint = c.bool_val(true);
//     variable_set path_constraint_variables;
//     thread& thread = *threads[t];

//     for (unsigned i=0; i<input.threads[t].size(); i++) {
//       if ( shared_ptr<input::instruction_z3> instr =
//            dynamic_pointer_cast<input::instruction_z3>(input.threads[t][i]) ) {
//         z3::expr_vector src(c);
//         z3::expr_vector dst(c);
//         for(const variable& v: instr->variables()) {
//           variable nname(c);

//           if (!is_primed(v)) {
//             //unprimmed case
//             if (is_global(v)) {
//               count_occur[v]++;
//               nname =v + int_to_string(count_occur[v]);
//               std::shared_ptr<tara::cssa::variable> var_ptr=make_shared<tara::cssa::variable>(nname);
//               pis.push_back(pi_needed(nname, v, global_in_thread[v], t, thread[i].loc));
//             }else{
//               nname = v + int_to_string(count_occur[v]) ;
//               std::shared_ptr<tara::cssa::variable> var_ptr=make_shared<tara::cssa::variable>(nname);
//               // check if we are reading an initial value
//               if (thread_vars[v] == "pre") {
//                 initial_variables.insert(nname);
//               }
//             }

//             thread[i].variables_read.insert(nname); // this variable is read by this instruction
//             thread[i].variables_read_orig.insert(v);
//           }else {
//             //primmed case
//             variable v1 = get_unprimed(v); //converting the primed to unprimed by removing the dot
//             count_occur[v1]++;
//             if(thread_vars[v1]=="pre") {    // every lvalued variable gets incremented by 1 and then the local variables are decremented by 1
//               count_occur[v1]--;
//             }
//             if (is_global(v1)) {
//               //nname = v1 + "#" + thread[i].loc->name;
//               nname = v1 + int_to_string(count_occur[v1]);
//               std::shared_ptr<tara::cssa::variable> var_ptr=make_shared<tara::cssa::variable>(nname);

//             }else{
//               //nname = thread.name + "." + v1 + "#" + thread[i].loc->name;
//               nname = v1 + int_to_string(count_occur[v1]);
//               std::shared_ptr<tara::cssa::variable> var_ptr=make_shared<tara::cssa::variable>(nname);
//               //instr_to_var[threads[t]->instructions[i]]=var_ptr;
//             }
//             thread[i].variables_write.insert(nname); // this variable is written by this instruction
//             thread[i].variables_write_orig.insert(v1);
//             // map the instruction to the variable
//             variable_written[nname] = thread.instructions[i];
//           }
//           src.push_back(v);
//           dst.push_back(nname);
//         }

//         // tread havoked variables the same
//         for(const variable& v: instr->havok_vars) {
//           variable nname(c);
//           variable v1 = get_unprimed(v);
//           thread[i].havok_vars.insert(v1);
//           if (is_global(v1)) {
//             nname = v1 + "#" + thread[i].loc->name;
//           } else {
//             nname = thread.name + "." + v1 + "#" + thread[i].loc->name;
//           }
//           // this variable is written by this instruction
//           thread[i].variables_write.insert(nname);
//           thread[i].variables_write_orig.insert(v1);
//           // map the instruction to the variable
//           variable_written[nname] = thread.instructions[i];
//           src.push_back(v);
//           dst.push_back(nname);
//           // add havok to initial variables
//           initial_variables.insert(nname);
//         }

//         // replace variables in expr
//         z3::expr newi = instr->instr.substitute(src,dst);
//         // deal with the path-constraint
//         thread[i].instr = newi;
//         if (thread[i].type == instruction_type::assume) {
//           path_constraint = path_constraint && newi;
//           path_constraint_variables.insert( thread[i].variables_read.begin(),
//                                             thread[i].variables_read.end() );
//         }
//         thread[i].path_constraint = path_constraint;
//         thread[i].variables_read.insert( path_constraint_variables.begin(),
//                                          path_constraint_variables.end() );
//         if (instr->type == instruction_type::regular) {
//           phi_vd = phi_vd && newi;
//         }else if (instr->type == instruction_type::assert) {
//           // add this assertion, but protect it with the path-constraint
//           phi_prp = phi_prp && implies(path_constraint,newi);
//           // add used variables
//           assertion_instructions.insert(thread.instructions[i]);
//         }
//         // increase referenced variables
//         //for(variable v: thread[i].variables_write_orig) {
//         for(variable v: thread[i].variables_write) {
//           thread_vars[v] = thread[i].loc->name;
//           if (is_global(v)) {
//             global_in_thread[v] = thread.instructions[i];
//             thread.global_var_assign[v].push_back(thread.instructions[i]);
//           }
//         }
//         // for(variable v:thread[i].variables_read)
//         // {
//         //   std::cout<<"\nnname.name["<<i<<"]: "<<v.name<<"\n";
//         // }

//         // for(auto itr=pis.begin();itr!=pis.end();itr++) {
//         //   if(itr->last_local!=NULL) {
//         //     //std::cout<<" pis itr"<<(itr)->last_local->instr<<"\n";
//         //   }
//         // }
//       }else{
//         throw cssa_exception("Instruction must be Z3");
//       }
//     }
//     phi_fea = phi_fea && path_constraint;
//   }

//   //variables_read_copy(variables_read);
//   for(unsigned int k=0;k<threads.size();k++) {
//     for(unsigned int n=0;n<input.threads[k].size();n++) {
//       for(variable vn:(threads[k]->instructions[n])->variables_read_orig) {
//         for(variable vo:(threads[k]->instructions[n])->variables_read) {
//           if(check_correct_global_variable(vo,vn.name)) {
//             (threads[k]->instructions[n])->variables_read_copy.insert(vo);
//           }
//         }
//         //if((threads[k]->instructions[n])->name)
//         //
//       }
//     }
//   }

//   shared_ptr<input::instruction_z3> pre_instr =
//     dynamic_pointer_cast<input::instruction_z3>(input.precondition);

//   // wmm_build_pis(pis, input);
// }

// void program::wmm_build_pis(vector< program::pi_needed >& pis,
//                             const input::program& input) {
//   z3::context& c = _z3.c;
//   for (pi_needed pi : pis) {
//     variable nname = pi.name;
//     vector<pi_function_part> pi_parts;
//     // get a list of all locations in question
//     vector<shared_ptr<const instruction>> locs;
//     if (pi.last_local != nullptr) {
//       locs.push_back(pi.last_local);
//     }
//     for (unsigned t = 0; t<threads.size(); t++) {
//       if (t!=pi.thread) {
//         locs.insert( locs.end(),
//                      threads[t]->global_var_assign[pi.orig_name].begin(),
//                      threads[t]->global_var_assign[pi.orig_name].end() );
//       }
//     }

//     z3::expr p = c.bool_val(false);
//     z3::expr p_hb = c.bool_val(true);
//     // reading from pre part only if the variable was never assigned
//     // in this thread we can read from pre
//     if (pi.last_local == nullptr) {
//       //p = p || (z3::expr)nname == (pi.orig_name + "#pre");
//       p = p || (z3::expr)nname == (pi.orig_name + "0");
//       for(const shared_ptr<const instruction>& lj: locs) {
//         assert (pi.loc->thread!=lj->loc->thread);
//         p_hb = p_hb && (_hb_encoding.make_hb(pi.loc,lj->loc));
//       }
//       pi_parts.push_back(pi_function_part(p_hb));
//       p = p && p_hb;
//       // add to initial variables list
//       //initial_variables.insert(pi.orig_name + "#pre");
//       initial_variables.insert(pi.orig_name + "0");
//     }

//     // for all other locations
//     for (const shared_ptr<const instruction>& li : locs) {
//       p_hb = c.bool_val(true);
//       variable oname = pi.orig_name + "#" + li->loc->name;
//       variable_set vars = li->variables_read; // variables used by this part of the pi function, init with variables of the path constraint
//       vars.insert(oname);
//       z3::expr p1 = (z3::expr)nname == oname;
//       p1 = p1 && li->path_constraint;
//       if (!pi.last_local || li->loc != pi.last_local->loc) { // ignore if this is the last location this was written
//         assert (pi.loc->thread!=li->loc->thread);
//         p_hb = p_hb && _hb_encoding.make_hb(li->loc, pi.loc);
//       }
//       for(const shared_ptr<const instruction>& lj: locs) {
//         if (li->loc != lj->loc) {
//           z3::expr before_write(_z3.c);
//           z3::expr after_read(_z3.c);
//           if (*(li->in_thread) == *(lj->in_thread)) {
//             // we can immediatelly determine if this is true
//             assert(li->loc != lj->loc);
//             before_write =_z3.c.bool_val(lj->loc->instr_no < li->loc->instr_no);
//           } else {
//             assert (li->loc->thread!=lj->loc->thread);
//             before_write = _hb_encoding.make_hb(lj->loc,li->loc);
//           }
//           if (!pi.last_local || lj->loc != pi.last_local->loc) { // ignore if this is the last location this was written
//             assert (pi.loc->thread!=lj->loc->thread);
//             after_read = _hb_encoding.make_hb(pi.loc,lj->loc);
//           } else
//             after_read = _z3.c.bool_val(pi.loc->instr_no < lj->loc->instr_no);
//           p_hb = p_hb && (before_write || after_read);
//         }
//       }
//       p = p || (p1 && p_hb);
//       pi_parts.push_back(pi_function_part(vars,p_hb));
//     }
//     phi_pi = phi_pi && p;
//     pi_functions[nname] = pi_parts;
//   }

// }

// why there is such a change (pre -> 0)?? -- needs rewrite
// use the original code
void program::wmm_build_pre(const input::program& input) {
  if (shared_ptr<input::instruction_z3> instr = dynamic_pointer_cast<input::instruction_z3>(input.precondition)) {
    z3::expr_vector src(_z3.c);
    z3::expr_vector dst(_z3.c);
    for(const variable& v: instr->variables()) {
      variable nname(_z3.c);
      if (!is_primed(v)) {
        //nname = v + "#pre";
        nname = v + "0";
        new_globals.insert(nname);
      } else {
        throw cssa_exception("Primed variable in precondition");
      }
      src.push_back(v);
      dst.push_back(nname);
    }
    // replace variables in expr
    phi_pre = instr->instr.substitute(src,dst);
  } else {
    throw cssa_exception("Instruction must be Z3");
  }
}


//
// build preserved programing order
//
// void program::wmm_build_hb(const input::program& input) {
//   z3::expr_vector locations(_z3.c);
//   // start location is needed to ensure all locations are mentioned in phi_po
//   shared_ptr<hb_enc::location> start_location = input.start_name();
//   locations.push_back(*start_location);
  
//   for (unsigned t=0; t<input.threads.size(); t++) {
//     thread& thread = *threads[t];
//     shared_ptr<hb_enc::location> prev;
//     for (unsigned j=0; j<input.threads[t].size(); j++) {
//       if (shared_ptr<input::instruction_z3> instr = dynamic_pointer_cast<input::instruction_z3>(input.threads[t][j])) {
//         shared_ptr<hb_enc::location> loc = instr->location();
//         locations.push_back(*loc);
        
//         std::shared_ptr<instruction> ninstr = make_shared<instruction>(_z3, loc, &thread, instr->name, instr->type, instr->instr);
//         thread.add_instruction(ninstr);
//       } else {
//         throw cssa_exception("Instruction must be Z3");
//       }
//     }
//   }
  
//   // make a distinct attribute
//   phi_distinct = distinct(locations);
  
//   for(unsigned t=0; t<threads.size(); t++) {
//     thread& thread = *threads[t];
//     unsigned j=0;
//     phi_po = phi_po && _hb_encoding.make_hb(start_location, thread[0].loc);
//     for(j=0; j<thread.size()-1; j++) {
//       phi_po = phi_po && _hb_encoding.make_hb(thread[j].loc, thread[j+1].loc);
//     }
//   }
  
//   // add atomic sections
//   for (input::location_pair as : input.atomic_sections) {
//     hb_enc::location_ptr loc1 = _hb_encoding.get_location(get<0>(as));
//     hb_enc::location_ptr loc2 = _hb_encoding.get_location(get<1>(as));
//     phi_po = phi_po && _hb_encoding.make_as(loc1, loc2);
//   }
//   // add happens-befores
//   for (input::location_pair hb : input.happens_befores) {
//     hb_enc::location_ptr loc1 = _hb_encoding.get_location(get<0>(hb));
//     hb_enc::location_ptr loc2 = _hb_encoding.get_location(get<1>(hb));
//     phi_po = phi_po && _hb_encoding.make_hb(loc1, loc2);
//   }
//   phi_po = phi_po && phi_distinct;
// }

//----------------------------------------------------------------------------
// New code
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// wmm configuration

bool program::is_wmm() const
{
  return mm != mm_t::sc;
}

bool program::is_mm_sc() const
{
  return mm == mm_t::sc;
}

bool program::is_mm_tso() const
{
  return mm == mm_t::sc;
}

bool program::is_mm_pso() const
{
  return mm == mm_t::pso;
}

bool program::is_mm_power() const
{
  return mm == mm_t::power;
}

void program::set_mm(mm_t _mm)
{
  mm = _mm;
}


//----------------------------------------------------------------------------
// utilities for symbolic events

symbolic_event::symbolic_event( z3::context& ctx, unsigned _tid,
                                const variable& _v, const variable& _prog_v,
                                hb_enc::location_ptr _loc, event_kind_t _et )
  : tid(_tid)
  , v(_v)
  , prog_v( _prog_v )
  , loc(_loc)
  , et( _et )
  , guard(ctx)
{
  assert( et == event_kind_t::r || et == event_kind_t::w );

  std::string et_name = et == event_kind_t::r ? "R" : "W";
  std::string event_name = et_name + "#" + v.name + "@"+ loc->name;
  // shared_ptr<hb_enc::location>
  e_v = make_shared<hb_enc::location>( ctx, event_name, true);

  // For each instruction we need to create a set of symbolic event
}

symbolic_event::symbolic_event( z3::context& ctx,
                                hb_enc::location_ptr _loc, event_kind_t _et )
  : tid(0)
  , v("dummy",ctx)
  , prog_v("dummy",ctx)
  , loc(_loc)
  , et( _et )
  , guard(ctx)
{
  assert( et == event_kind_t::i );
  std::string event_name = "init#" +loc->name;
  // shared_ptr<hb_enc::location>
  e_v = make_shared<hb_enc::location>( ctx, event_name, true);

  // For each instruction we need to create a set of symbolic event
}

z3::expr program::wmm_mk_hb(const cssa::se_ptr& before,
                             const cssa::se_ptr& after) {
  return _hb_encoding.make_hb( before->e_v, after->e_v );
}


z3::expr program::wmm_mk_hbs(const cssa::se_set& before,
                             const cssa::se_set& after) {
  z3::expr hbs = _z3.c.bool_val(true);
  for( auto& bf : before ) {
    for( auto& af : after ) {
      wmm_mk_hb( bf, af );
    }
  }
  return hbs;
}

// ghb = guarded hb
z3::expr program::wmm_mk_ghb( const cssa::se_ptr& before,
                                     const cssa::se_ptr& after ) {
  return implies( before->guard && after->guard, wmm_mk_hb( before, after ) );
}


//----------------------------------------------------------------------------
// utilities

void program::unsupported_mm() {
  std::string msg = "unsupported memory model: " + string_of_mm( mm ) + "!!";
  throw cssa_exception( msg.c_str() );
}

bool program::in_grf( const cssa::se_ptr& wr, const cssa::se_ptr& rd ) {
  if( is_mm_sc() ) {
    return true;
  }else if( is_mm_tso() ) {
    return wr->tid != rd->tid; //only events with diff threds are part of grf
  }else{
    unsupported_mm();
    return false;
  }
}

void program::wmm_build_ses( const input::program& input ) {
  // For each global variable we need to construct
  // - rf-val boolean bit that declares that declares reading
  // - rf-grf
  // - rf-some
  // - ws
  // - fr
  // - ppo x

  z3::context& c = _z3.c;

  for( const variable& v1:globals ) {
    const auto& rds = wr_events[v1];
    const auto& wrs = wr_events[v1];
    for( const se_ptr& rd : rds ) {
      z3::expr some_rfs = _z3.c.bool_val(false);;
      const variable& rd_v = rd->v;
      for( const se_ptr& wr : wrs ) {
        const variable& wr_v = wr->v;
        std::string bname = wr->name()+"-"+rd->name();
        z3::expr b = c.bool_const(  bname.c_str() );
        some_rfs = some_rfs || b;
        z3::expr eq = ( (z3::expr)rd_v == (z3::expr)wr_v );
        wf = wf && implies( b, wr->guard && eq);
        // read from
        z3::expr new_rf = implies( b, wmm_mk_ghb( wr, rd ) );
        rf = rf && new_rf;
        //global read from
        if( in_grf( wr, rd) ) grf = grf && new_rf;
        // from read
        for( const se_ptr& wr1 : wrs ) {
          if( wr1->name() != wr->name() ) {
            fr = fr && implies( b && wmm_mk_ghb(wr, wr1) && wr1->guard,
                                wmm_mk_ghb( rd, wr1 ) );
          }
        }
      }
      wf = wf && implies( rd->guard, rf_some );
    }

    for( const se_ptr& wr1 : wrs ) {
      for( const se_ptr& wr2 : wrs ) {
        // write serialization constraints
        // Do we need the following constraints
        if( wr1->tid != wr2->tid ) {
          ws = ws && ( wmm_mk_ghb( wr1, wr2 ) || wmm_mk_ghb( wr2, wr1 ) );
        }
      }
    }
  }


  std::cout<<"wf : \n"<<wf<<"\n";
  std::cout<<"rf : \n"<<rf<<"\n";
  std::cout<<"grf: \n"<<grf<<"\n";
  std::cout<<"fr : \n"<<fr<<"\n";
  std::cout<<"ws : \n"<<ws<<"\n";


  // bool flag=0;
  // unsigned int count_global_occur;
  // unordered_set<std::shared_ptr<instruction>> RF_SOME;
  // unordered_set<std::shared_ptr<instruction>> RF_WS;
  // unordered_map<std::shared_ptr<instruction>,bool> if_pre;
  // std::shared_ptr<cssa::instruction> write3; //for rf_ws
  // std::shared_ptr<cssa::instruction> write4; //for_rf_ws
  // std::shared_ptr<cssa::instruction> write5; // for fr
  // std::shared_ptr<cssa::instruction> write6; // for fr
  // z3::expr conjec3=c.bool_val(true);


  // for(std::string v1:input.globals) {

  //   RF_WS.clear();
  //   if_pre.clear();
  //   count_global_occur=0;

  //   for(unsigned int k=0;k<threads.size();k++) {
  //     for(unsigned int n=0;n<input.threads[k].size();n++) {
  //       auto& instr1 = threads[k]->instructions[n];
  //       for(variable v2:(instr1)->variables_read_copy) {
  //         RF_SOME.clear();
  //         flag=0;
  //         if(check_correct_global_variable(v2,v1)) {

  //           for( unsigned int k2=0; k2 < threads.size(); k2++ ) {
  //             // thread& thread = *threads[k2];
  //             for( unsigned int n2=0; n2 < input.threads[k2].size(); n2++ ) {
  //               auto& instr2 = threads[k2]->instructions[n2];

  //               for(variable v3:(instr2)->variables_write) {
  //                 if(check_correct_global_variable(v3,v1)) {
  //                   if( k2 != k ) {
  //                     RF_SOME.insert( instr2 );
  //                     RF_WS.insert  ( instr2 );
  //                     count_global_occur++;
  //                     rf_val=rf_val && build_rf_val(instr2, instr1, c,v1,false);
  //                     rf_grf=rf_grf
  //                       && build_rf_grf( instr2, instr1, c,v1,false,input);

  //                   } else if(n2<n) {
  //                     RF_SOME.insert( instr2 );
  //                     RF_WS.insert(   instr2 );
  //                     count_global_occur++;
  //                     rf_val=rf_val && build_rf_val(instr2, instr1, c,v1,false);
  //                     rf_grf=rf_grf
  //                       && build_rf_grf(instr2, instr1, c,v1,false,input );
  //                   }
  //                 }
  //               }

  //               if(flag!=1) {
  //                 for(variable vi:new_globals) {
  //                   if(check_correct_global_variable(vi,v1)) {
  //                     RF_SOME.insert(instr1);
  //                     RF_WS.insert(instr1);
  //                     if_pre[instr1]=1;

  //                     count_global_occur++;

  //                     rf_val=rf_val && build_rf_val( instr1, instr1, c,v1,true);
  //                     rf_grf=rf_grf &&
  //                       build_rf_grf(instr1, instr1, c,v1,true,input);
  //                     flag=1;
  //                     break;
  //                   }
  //                 }
  //               }
  //             }
  //           }
  //         }

  //         if( count_global_occur > 1 ) {

  //           unsigned int counter=1;

  //           // auto itr=RF_SOME.begin();
  //           std::shared_ptr<cssa::instruction> write1, write2; // for rf_some

  //           for(auto itr=RF_SOME.begin();itr!=RF_SOME.end();itr++) {

  //             if(counter%2==1) {
  //               write1=*itr;
  //               if(counter!=1) {
  //                 if((if_pre[write1]!=1)&&(if_pre[write2]!=1)) {
  //                   conjec3=build_rf_some( instr1, write1,write2,c,v1,false);
  //                 }
  //                 else if(if_pre[write2]==1) {
  //                   conjec3=build_rf_some( instr1, write2,write1,c,v1,true);
  //                 }
  //                 else if(if_pre[write1]==1) {
  //                   conjec3=build_rf_some( instr1, write1,write2,c,v1,true);
  //                 }
  //               }
  //             }
  //             else if(counter%2==0) {

  //               write2= *itr;
  //               if((if_pre[write1]!=1)&&(if_pre[write2]!=1)) {
  //                 conjec3=build_rf_some( instr1, write1, write2, c, v1, false);
  //               }
  //               else if(if_pre[write2]==1) {
  //                 conjec3=build_rf_some( instr1, write2, write1, c, v1, true);
  //               }
  //               else if(if_pre[write1]==1) {
  //                 conjec3=build_rf_some( instr1, write1, write2, c, v1, true);
  //               }
  //             }
  //             counter++;
  //           }
  //         }
  //         rf_some= rf_some && conjec3;
  //         //std::cout<<"\nrf_some\t"<<rf_some<<"\n";
  //         for(auto iterator1=RF_SOME.begin();iterator1!=RF_SOME.end();) {
  //           write5=*iterator1;
  //           iterator1++;
  //           for(auto iterator2=iterator1;iterator2!=RF_SOME.end();iterator2++) {
  //             write6=*iterator2;
  //             if((if_pre[write5]!=1)&&(if_pre[write6]!=1)) {
  //               fr = fr &&
  //                 build_fr(write5,instr1,write6, c,v1,false,false,input) &&
  //                 build_fr(write6,instr1,write5, c,v1,false,false,input);
  //             }
  //             else if(if_pre[write6]==1) {
  //               fr = fr &&
  //                 build_fr(write5,instr1,write6, c,v1,true,true,input) &&
  //               //if(if_pre[write6]!=1)
  //                 build_fr(write6,instr1,write5, c,v1,true,false,input);
  //             }
  //             else if(if_pre[write5]==1) {
  //               fr = fr &&
  //                 build_fr( write5,instr1,write6, c,v1,true,false,input ) &&
  //                 build_fr( write6,instr1,write5, c,v1,true,true,input );
  //             }
  //             else {
  //               continue;
  //             }
  //           }
  //         }

  //       }
  //     }
  //   }

  //   for(auto itr1=RF_WS.begin();itr1!=RF_WS.end();) {
  //     write3=*itr1;

  //     itr1++;
  //     for( auto itr2=itr1; itr2!=RF_SOME.end(); itr2++ ) {
  //       write4=*itr2;
  //       if((if_pre[write4]!=1)&&(if_pre[write3]!=1)) {
  //         rf_ws= rf_ws && build_ws(write4,write3,c,false,input);
  //       }
  //       else if((if_pre[write4]==1)) {
  //         rf_ws= rf_ws && build_ws(write4,write3,c,true,input);
  //       }
  //       else if((if_pre[write3]==1)) {
  //         rf_ws= rf_ws && build_ws(write3,write4,c,true,input);
  //       }
  //       else {
  //         continue;
  //       }
  //     }
  //   }
  // }
  // std::cout<<"\nrf_val\t"<<rf_val<<"\n";
  // std::cout<<"\nrf_grf\t"<<rf_grf<<"\n";
  // std::cout<<"\nrf_ws\t"<<rf_ws<<"\n";
  // std::cout<<"\nrf_some\t"<<rf_some<<"\n";
  // std::cout<<"\nfr\t"<<fr<<"\n";
}

//----------------------------------------------------------------------------
// build preserved programming order
//

void program::wmm_build_cssa_thread(const input::program& input) {

  //
  // start location is needed to ensure all locations are mentioned in phi_ppo
  //
  std::shared_ptr<hb_enc::location> init_location = input.start_name();
  se_ptr init_se = mk_se_ptr( _z3.c, init_location, event_kind_t::i );
  init_loc.insert(init_se);

  for( unsigned t=0; t < input.threads.size(); t++ ) {
    thread& thread = *threads[t];
    assert( thread.size() == 0 );

    shared_ptr<hb_enc::location> prev;
    for( unsigned j=0; j < input.threads[t].size(); j++ ) {
      if( shared_ptr<input::instruction_z3> instr =
          dynamic_pointer_cast<input::instruction_z3>(input.threads[t][j]) ) {

        shared_ptr<hb_enc::location> loc = instr->location();
        auto ninstr = make_shared<instruction>( _z3, loc, &thread, instr->name,
                                                instr->type, instr->instr );
        thread.add_instruction(ninstr);
      }else{
        throw cssa_exception("Instruction must be Z3");
      }
    }
  }
}

void program::wmm_mk_distinct_events() {

  z3::expr_vector locations(_z3.c);
  for( auto& init_se : init_loc ) {
    locations.push_back( *(init_se->e_v) );
  }

  for( unsigned t=0; t < threads.size(); t++ ) {
    thread& thread = *threads[t];
    assert( thread.size() == 0 );
    for( unsigned j=0; j < thread.size(); j++ ) {
      for( auto& rd_event : thread[j].rds )
        locations.push_back( *(rd_event->e_v) );
      for( auto& wr_event : thread[j].wrs )
        locations.push_back( *(wr_event->e_v) );
    }
  }
  phi_distinct = distinct(locations);

  // _hb_encoding().make_locations(locations); //todo: do we need to add these
  // make a distinct attribute
  // May be a problem in wmm case??
}

void program::wmm_build_ppo() {

  for( unsigned t=0; t<threads.size(); t++ ) {
    thread& thread = *threads[t];
    unsigned j=0;
    if( is_mm_sc() ) {
      unsigned last_j = thread.size();
      for( j=0; j<thread.size()-1; j++ ) {
        phi_po = phi_po && wmm_mk_hbs( thread[j].rds, thread[j].wrs );
        if( thread[j].rds.size() > 0 || thread[j].wrs.size() > 0 ) {
          auto& after =
            thread[j].rds.size() > 0 ? thread[j].rds : thread[last_j].wrs;
          if( last_j > j ) {
            phi_po = phi_po && wmm_mk_hbs( init_loc, after );
          }else{
            auto& before = thread[last_j].wrs.size() > 0 ?
              thread[last_j].wrs : thread[last_j].rds;
            phi_po = phi_po && wmm_mk_hbs( before, after );
          }
          last_j = j;
        }
        phi_po = phi_po && _hb_encoding.make_hb(thread[j].loc, thread[j+1].loc);
      }
      //TODO: deal with atomic section and prespecified happens befores
      // add atomic sections
      // for (input::location_pair as : input.atomic_sections) {
      //   hb_enc::location_ptr loc1 = _hb_encoding.get_location(get<0>(as));
      //   hb_enc::location_ptr loc2 = _hb_encoding.get_location(get<1>(as));
      //   phi_po = phi_po && _hb_encoding.make_as(loc1, loc2);
      // }
      // // add happens-befores
      // for (input::location_pair hb : input.happens_befores) {
      //   hb_enc::location_ptr loc1 = _hb_encoding.get_location(get<0>(hb));
      //   hb_enc::location_ptr loc2 = _hb_encoding.get_location(get<1>(hb));
      //   phi_po = phi_po && _hb_encoding.make_hb(loc1, loc2);
      // }
    }else if( is_mm_tso() ) {
      unsigned last_rd = thread.size(), last_wr = thread.size();
      bool rd_wr_sync = true;
      for( j=0; j<thread.size()-1; j++ ) {
        if( thread[j].rds.size() > 0 || thread[j].wrs.size() > 0 ) {
          phi_po = phi_po && wmm_mk_hbs( thread[j].rds, thread[j].wrs );
          if( last_rd > j && thread[j].rds.size() > 0 ) {
            phi_po = phi_po && wmm_mk_hbs( init_loc, thread[j].rds );
            last_rd = j;
            if( thread[j].wrs.size() > 0 ) last_wr = j;
            rd_wr_sync = (thread[j].wrs.size() > 0);
          }else{
            if( thread[j].rds.size() > 0 ) {
              phi_po = phi_po && wmm_mk_hbs(thread[last_rd].rds, thread[j].rds);
              last_rd = j;
            }
            if( thread[j].wrs.size() > 0 ) {
              if( last_wr < j )
                phi_po =phi_po && wmm_mk_hbs(thread[last_wr].wrs,thread[j].wrs);
              else
                // if(rd_wr_sync) // todo : potential optimization
                  phi_po = phi_po && wmm_mk_hbs( init_loc, thread[j].wrs );
              last_wr = j;
            }
            if( thread[j].rds.size() > 0 ) {
              rd_wr_sync = (thread[j].wrs.size() > 0);
            }else{ // thread[j].wrs.size() > 0
              if( !rd_wr_sync ) {
                //sync it
                phi_po=phi_po && wmm_mk_hbs(thread[last_rd].rds, thread[j].wrs);
              }
              rd_wr_sync = true;
            }
          }
        }
      }
      //
    }else{
      unsupported_mm();
    }
  }

  phi_po = phi_po && phi_distinct;
}

void program::wmm_build_ssa( const input::program& input ) {
  z3::context& c = _z3.c;
  vector<pi_needed> pis;


  // for counting occurences of each variable
  unordered_map<string, int> count_occur;

  for (string v: input.globals) {
    count_occur[v]=0;
  }

  // maps variable to the last place they were assigned
  unordered_map<string, string> thread_vars;

  for( unsigned t=0; t < input.threads.size(); t++ ) {
    // last reference to global variable in this thread (if any)
    unordered_map<string, shared_ptr<instruction> > global_in_thread;

    // set all local vars to "pre"
    for (string v: input.threads[t].locals) {
      thread_vars[v] = "pre";
      count_occur[v]=0;
    }

    z3::expr path_constraint = c.bool_val(true);
    variable_set path_constraint_variables;
    thread& thread = *threads[t];

    for (unsigned i=0; i<input.threads[t].size(); i++) {
      if ( shared_ptr<input::instruction_z3> instr =
           dynamic_pointer_cast<input::instruction_z3>(input.threads[t][i]) ) {
        z3::expr_vector src(c), dst(c);
        for( const variable& v: instr->variables() ) {
          variable nname(c);

          if( is_primed(v) ) {
            //primmed case
            variable v1 = get_unprimed(v); //converting the primed to unprimed
            count_occur[v1]++;
            if( thread_vars[v1]=="pre" ) {
              // every lvalued variable gets incremented by 1 and then
              // the local variables are decremented by 1
              count_occur[v1]--;
            }
            if( is_global(v1) ) {
              //nname = v1 + "#" + thread[i].loc->name;
              nname = v1 + int_to_string(count_occur[v1]);
              //insert write symbolic event
              se_ptr wr = mk_se_ptr(c,t,nname,v1,thread[i].loc,event_kind_t::w);
              thread[i].wrs.insert( wr );
              wr_events[v1].insert( wr );
              // auto var_ptr= make_shared<tara::cssa::variable>(nname);
            }else{
              //nname = thread.name + "." + v1 + "#" + thread[i].loc->name;
              nname = v1 + int_to_string(count_occur[v1]);
              // auto var_ptr = make_shared<tara::cssa::variable>(nname);
              //instr_to_var[threads[t]->instructions[i]]=var_ptr;
            }
            // the following variable is written by this instruction
            thread[i].variables_write.insert(nname);
            thread[i].variables_write_orig.insert(v1);
            // map the instruction to the variable
            variable_written[nname] = thread.instructions[i];
          }else{
            //unprimmed case
            if ( is_global(v) ) {
              count_occur[v]++;
              nname =v + int_to_string(count_occur[v]);
              // auto var_ptr = make_shared<tara::cssa::variable>(nname);
              se_ptr rd = mk_se_ptr(c,t,nname,v,thread[i].loc,event_kind_t::r);
              thread[i].rds.insert( rd );
              rd_events[v].insert( rd );

              pis.push_back( pi_needed(nname, v, global_in_thread[v],
                                       t, thread[i].loc) );
            }else{
              nname = v + int_to_string(count_occur[v]) ;
              auto var_ptr= make_shared<tara::cssa::variable>(nname);
              // check if we are reading an initial value
              if (thread_vars[v] == "pre") {
                initial_variables.insert(nname);
              }
            }
            // the following variable is read by this instruction
            thread[i].variables_read.insert(nname);
            thread[i].variables_read_orig.insert(v);
          }
          src.push_back(v);
          dst.push_back(nname);
        }

        // thread havoced variables the same
        for(const variable& v: instr->havok_vars) {
          variable nname(c);
          variable v1 = get_unprimed(v);
          thread[i].havok_vars.insert(v1);
          if( is_global(v1) ) {
            nname = v1 + "#" + thread[i].loc->name;
            se_ptr wr = mk_se_ptr(c,t,nname,v1,thread[i].loc,event_kind_t::w );
            thread[i].wrs.insert( wr );
            wr_events[v1].insert( wr );
          } else {
            nname = thread.name + "." + v1 + "#" + thread[i].loc->name;
          }
          // this variable is written by this instruction
          thread[i].variables_write.insert(nname);
          thread[i].variables_write_orig.insert(v1);
          // map the instruction to the variable
          variable_written[nname] = thread.instructions[i];
          src.push_back(v);
          dst.push_back(nname);
          // add havok to initial variables
          initial_variables.insert(nname);
        }

        for( se_ptr rd : thread[i].rds ) {
          rd->guard = path_constraint;
        }

        // replace variables in expr
        z3::expr newi = instr->instr.substitute(src,dst);
        // deal with the path-constraint
        thread[i].instr = newi;
        if (thread[i].type == instruction_type::assume) {
          path_constraint = path_constraint && newi;
          path_constraint_variables.insert( thread[i].variables_read.begin(),
                                            thread[i].variables_read.end() );
        }
        thread[i].path_constraint = path_constraint;
        thread[i].variables_read.insert( path_constraint_variables.begin(),
                                         path_constraint_variables.end() );
        if (instr->type == instruction_type::regular) {
          phi_vd = phi_vd && newi;
        }else if (instr->type == instruction_type::assert) {
          // add this assertion, but protect it with the path-constraint
          phi_prp = phi_prp && implies(path_constraint,newi);
          // add used variables
          assertion_instructions.insert(thread.instructions[i]);
        }

        for( se_ptr wr : thread[i].wrs ) {
          wr->guard = path_constraint;
        }

        // increase referenced variables
        //for(variable v: thread[i].variables_write_orig) {
        for(variable v: thread[i].variables_write) {
          thread_vars[v] = thread[i].loc->name;
          if (is_global(v)) {
            global_in_thread[v] = thread.instructions[i];
            thread.global_var_assign[v].push_back(thread.instructions[i]);
          }
        }
      }else{
        throw cssa_exception("Instruction must be Z3");
      }
    }
    phi_fea = phi_fea && path_constraint;
  }

  //variables_read_copy(variables_read);
  // for( unsigned int k=0;k < threads.size(); k++ ) {
  //   for( unsigned int n=0;n<input.threads[k].size();n++ ) {
  //     for( variable vn:(threads[k]->instructions[n])->variables_read_orig ) {
  //       for(variable vo:(threads[k]->instructions[n])->variables_read) {
  //         if(check_correct_global_variable(vo,vn.name)) {
  //           (threads[k]->instructions[n])->variables_read_copy.insert(vo);
  //         }
  //       }
  //     }
  //   }
  // }

  // shared_ptr<input::instruction_z3> pre_instr =
  //   dynamic_pointer_cast<input::instruction_z3>(input.precondition);
}

void program::wmm(const input::program& input) {
  // set memory model
  set_mm( input.get_mm() );
  // construct thread scaleton
  wmm_build_cssa_thread(input);
  wmm_build_pre(input);
  // build ssa
  wmm_build_ssa(input);
  // declares are read/write events on globals are distinct
  wmm_mk_distinct_events();
  // build hb formulas to encode the preserved program order
  wmm_build_ppo();
  // build symbolic event structure
  wmm_build_ses(input);
}


