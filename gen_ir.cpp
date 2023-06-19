#include "symbol_table.h"
#include "gen_ir.h"
#include <string>

extern CodeBuffer buffer;
extern SymbolTableStack tables;

using namespace std;

string GenIR::new_reg() {
    string reg = "%var_" + to_string(next_reg);
    next_reg++;
    return reg;
}

string GenIR::new_glob_reg() {
    string reg = "@var_" + to_string(next_reg);
    next_reg++;
    return reg;
}

string GenIR::new_label() {
    string new_label = "label_" + to_string(next_label);
    next_label++;
    return new_label;
}

void GenIR::gen_binop(Exp &res, const Exp &exp1, const Exp &exp2, const string &op) {
    res.reg = new_reg();
    string llvm_op;
    if (op == "+") {
        llvm_op = "add";
    }
    else if (op == "-") {
        llvm_op = "sub";
    }
    else if (op == "*") {
        llvm_op = "mul";
    }
    else if (op == "/") {
        if (res.type == "int") {
            llvm_op = "sdiv";
        }
        else { //res->type = "byte"
            llvm_op = "udiv";
        }
        // Check devision by zero
        buffer.emit("call void @check_division(i32 " + exp2.reg + ")");
    }
    // Emit the instruction
    buffer.emit(res.reg + " = " + llvm_op + " i32 " + exp1.reg + ", " + exp2.reg);

    // Handle byte type
    if (res.type == "byte") {
        string tmp = res.reg;
        res.reg = new_reg();
        buffer.emit(res.reg + " = and i32 255, " + tmp);
    }
}

void GenIR::gen_boolop(Exp &res, const Exp &exp1, const Exp &exp2, const string &op, const string &label) {
    if (op == "and") {
        buffer.bpatch(exp1.true_list, label);
        res.true_list = AddrList(exp2.true_list);
        res.false_list = buffer.merge(exp1.false_list, exp2.false_list);
    }
    // op == "or" 
    else  {
        buffer.bpatch(exp1.false_list, label);
        res.true_list = buffer.merge(exp1.true_list, exp2.true_list);
        res.false_list = AddrList(exp2.false_list);
    }
}

void GenIR::gen_notop(Exp &res, const Exp &exp) {
    res.true_list = AddrList(exp.false_list);
    res.false_list = AddrList(exp.true_list);
}

void GenIR::gen_label(const string &label) {
    // Not sure about this:
    // buffer.emit("br label %" + label);
    buffer.emit(label + ":");
}

void GenIR::gen_relop(Exp &res, const Exp &exp1, const Exp &exp2, const string &op) {
    res.reg = new_reg();
    string llvm_op;
    if (op == "==") {
        llvm_op = "eq";
    } else if (op == "!=") {
        llvm_op = "ne";
    } else if (op == ">") {
        llvm_op = "sgt";
    } else if (op == ">=") {
        llvm_op = "sge";
    } else if (op == "<") {
        llvm_op = "slt";
    } else {
        llvm_op = "sle";
    }

    buffer.emit(res.reg + " = icmp " + llvm_op + " i32 " + exp1.reg + ", " + exp2.reg);
    int address = buffer.emit("br i1 " + res.reg + ", label @, label @");
    res.true_list = buffer.makelist(pair<int, BranchLabelIndex>(address, FIRST));
    res.false_list = buffer.makelist(pair<int, BranchLabelIndex>(address, SECOND));
}


