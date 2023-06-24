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
    buffer.emitGlobal(reg + " = constant [" + to_string(str.length()) + " x i8] c" + str + "\\00\"");
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

void GenIR::gen_store(string head, int offset, string reg) {
    string ptr_reg = new_reg();
    buffer.emit(ptr_reg + " = getelementptr i32, i32* " + head + ", i32 " + std::to_string(offset));
    buffer.emit("store i32 " + reg + ", i32* " + ptr_reg);
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

void GenIR::gen_assign(Exp &exp, int offset) {
    if (exp.type == "bool") {
        auto new_exp = gen_bool_exp(exp);
        gen_store(symbol_table_stack.get_current_symbol_table()->head, offset, new_exp->reg);
    } else {
        gen_store(symbol_table_stack.get_current_symbol_table()->head, offset, exp.reg);
    }
}

shared_ptr<Exp> GenIR::gen_bool_exp(Exp &exp) {
    // if (exp.type != "bool") {
    //     return make_shared<Exp>(exp);
    // }

    shared_ptr<Exp> new_exp = make_shared<Exp>();
    new_exp->reg = new_reg();
    new_exp->type = "bool";
    string true_list = new_label();
    string false_list = new_label();
    string next_list = new_label();

    buffer.emit("br label %" + true_list);
    buffer.emit(true_list + ":");
    int addr1 = buffer.emit("br label @");

    buffer.emit(false_list + ":");
    int addr2 = buffer.emit("br label @");

    buffer.emit(next_list + ":");

    AddrList next = buffer.merge(buffer.makelist(pair<int, BranchLabelIndex>(addr1, FIRST)),
                               buffer.makelist(pair<int, BranchLabelIndex>(addr2, FIRST)));

    buffer.bpatch(exp.true_list, true_list);
    buffer.bpatch(exp.false_list, false_list);
    buffer.bpatch(next, next_list);
    buffer.emit(new_exp->reg + " = phi i32 [ 1, %" + true_list +"], [0, %" + false_list + "]");

    return new_exp;
}

void GenIR::gen_return(Exp &exp, bool print_reg) {
    string str = print_reg ? exp.reg : exp.value;
    if (exp.type == "string") {
        buffer.emit("ret i8* " + str);
    } else {
        buffer.emit("ret i32 " + str);
    }
}

void GenIR::gen_funcdecl(string id, string return_type, vector<shared_ptr<Formaldecl>> params) {
    string arg_str = "";
    for (auto param : params) {
        if (param->type->type == "string") {
            arg_str += "i8*";
        } else {
            arg_str += "i32";
        }
        if (param != params.back())
            arg_str += ", ";
    }
    string ret_type_str;
    if (return_type == "void") {
        ret_type_str = "void";
    } else if (return_type == "string") {
        ret_type_str = "i8*";
    } else {
        ret_type_str = "i32";
    }
    buffer.emit("define " + ret_type_str + " @" + id + "(" + arg_str + "){");
}

string GenIR::allocate_function_frame() {
    string head = new_reg();
    buffer.emit(head + " = alloca i32, i32 50");
    return head;
}

void GenIR::gen_close_func(Rettype *rettype) {
    string str = rettype->type->type == "void" ? "void" : "i32 0";
    buffer.emit("ret " + str + " }");
}

void GenIR::gen_nextlist_label(Exp *exp) {
    int addr = buffer.emit("br label @");
    exp->next_list = buffer.merge(buffer.makelist(pair<int, BranchLabelIndex>(addr, FIRST)), exp->next_list);

}

void GenIR::gen_init() {
    buffer.emit("@.DIV_BY_ZERO_ERROR = internal constant [23 x i8] c\"Error division by zero\\00\"");

    buffer.emit("define void @check_division(i32) {");
    buffer.emit("%valid = icmp eq i32 %0, 0");
    buffer.emit("br i1 %valid, label %ILLEGAL, label %LEGAL");
    buffer.emit("ILLEGAL:");
    buffer.emit("call void @printzero(i8* getelementptr([23 x i8], [23 x i8]* @.DIV_BY_ZERO_ERROR, i32 0, i32 0))");
    buffer.emit("call void @exit(i32 0)");
    buffer.emit("ret void");
    buffer.emit("LEGAL:");
    buffer.emit("ret void");
    buffer.emit("}");
}