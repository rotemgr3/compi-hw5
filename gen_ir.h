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

};

#endif // GEN_IR_H