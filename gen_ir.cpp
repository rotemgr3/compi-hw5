#include "symbol_table.h"
#include "gen_ir.h"
#include <string>

extern CodeBuffer buffer;
extern SymbolTableStack symbol_table_stack;

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

void GenIR::gen_int_and_byte(Exp &exp) {
    buffer.emit(exp.reg + " = add i32 " + exp.value + ", 0");
}

void GenIR::gen_string(Exp &exp) {
    string str = exp.value;
    str.pop_back();
    string reg = new_glob_reg();
    string get_ptr = "getelementptr [" + to_string(str.length()) + " x i8], [" + to_string(str.length()) + " x i8]* " + reg + ", i32 0, i32 0";
    str += + "\\00\"";
    buffer.emitGlobal(reg + " = constant [" + to_string(str.length()) + " x i8] c" + str );
    reg.replace(0, 1, "%");
    buffer.emit(reg + ".ptr = " + get_ptr);
    exp.reg = reg + ".ptr";
}

void GenIR::gen_bool(Exp &exp) {
    int address = buffer.emit("br label @");
    if (exp.value == "true") {
        exp.true_list = buffer.makelist(pair<int, BranchLabelIndex>(address, FIRST));
    } else {
        exp.false_list = buffer.makelist(pair<int, BranchLabelIndex>(address, SECOND));
    }
}

void GenIR::gen_id(Exp &exp) {
    auto symbol = symbol_table_stack.get_symbol(exp.value);
    if (symbol->offset < 0) {
        exp.reg = "%" + std::to_string(((-1) * symbol->offset - 1));
    } else {
        exp.reg = gen_load(symbol_table_stack.get_current_symbol_table()->head, symbol->offset);
    }

    if (exp.type == "bool") {
        string cond = new_reg();
        buffer.emit(cond + " = icmp eq i32 0, " + exp.reg);
        int address = buffer.emit("br i1 " + cond + ", label @, label @");
        exp.true_list = buffer.makelist(pair<int, BranchLabelIndex>(address, SECOND));
        exp.false_list = buffer.makelist(pair<int, BranchLabelIndex>(address, FIRST));
    }
}

string GenIR::gen_load(string head, int offset) {
    string ptr_reg = new_reg();
    buffer.emit(ptr_reg + " = getelementptr i32, i32* " + head + ", i32 " + std::to_string(offset));
    string ret_reg = new_reg();
    buffer.emit(ret_reg + " = load i32, i32* " + ptr_reg);
    return ret_reg;
}

void GenIR::gen_call(Call &call) {
    call.reg = new_reg();
    string arg_str = "";
    for (auto exp : call.exp_list->expressions) {
        if (exp->type == "string") {
            arg_str += "i8*";
        } else {
            arg_str += "i32";
        }
        arg_str += " " + exp->reg;
        if (exp != call.exp_list->expressions.back())
            arg_str += ", ";
    }
    
    if (call.ret_type == "void")
        buffer.emit("call void @" + call.id + "(" + arg_str + ")");
    else if(call.ret_type == "string"){
        buffer.emit(call.reg + " = call  i8* @" + call.id + "(" + arg_str + ")");
    }
    else if(call.ret_type == "bool") {
        buffer.emit(call.reg + " = call i32 @" + call.id + "(" + arg_str + ")");
        string cond = new_reg();
        buffer.emit(cond + " = icmp eq i32 0, " + call.reg);
        int address = buffer.emit("br i1 " + cond + ", label @, label @");
        call.true_list = buffer.makelist(pair<int, BranchLabelIndex>(address, SECOND));
        call.false_list = buffer.makelist(pair<int, BranchLabelIndex>(address, FIRST));
    } else {
        buffer.emit(call.reg + " = call  i32 @" + call.id + "(" + arg_str + ")");
    }
}