#ifndef GEN_IR_H
#define GEN_IR_H

#include <string>
#include "types.h"
#include "bp.hpp"

class GenIR {
    public:
        int next_reg = 0;
        int next_label = 0;

        GenIR() = default;
        ~GenIR() = default;
        string new_reg();
        string new_label();
        string new_glob_reg();
        void gen_binop(Exp &res, const Exp &exp1, const Exp &exp2, const string &op);
        void gen_boolop(Exp &res, const Exp &exp1, const Exp &exp2, const string &op, const string &label);
        void gen_notop(Exp &res, const Exp &exp);
        void gen_relop(Exp &res, const Exp &exp1, const Exp &exp2, const string &op);
        void gen_label(const string &label);    
        void gen_int_and_byte(Exp &exp);
        void gen_bool(Exp &exp);
        void gen_string(Exp &exp);
        void gen_id(Exp &exp);
        string gen_load(string head, int offset);
        void gen_store(string head, int offset, string reg);
        void gen_call(Call &call);
        shared_ptr<Exp> gen_bool_exp(Exp &exp);
        void gen_assign(Exp &exp, int offset);
        void gen_return(Exp& exp, bool is_reg);
        void gen_funcdecl(string id, string return_type, vector<shared_ptr<Formaldecl>> params);
        string allocate_function_frame();
        void gen_close_func(Rettype *rettype);
        void gen_nextlist_label(Exp *exp);
        void gen_init();
        Exp* gen_bool_exp2(Exp *exp);
};

#endif // GEN_IR_H