#include "types.h"
#include "symbol_table.h"
#include "hw3_output.hpp"
#include "gen_ir.h"
#include <string.h>
#include <set>


extern SymbolTableStack symbol_table_stack;
extern int yylineno;
extern GenIR gen_ir;
extern CodeBuffer buffer;

Label::Label() : Node() {
    text = gen_ir.new_label();
}

Program::Program() {
    symbol_table_stack.verify_main();
    symbol_table_stack.pop_symbol_table();
}

Funcdecl::Funcdecl(Override* override, Rettype* return_type, Node* id, Formals* params) {
    is_override = override->is_override;
    ret_type = make_shared<Rettype>(*return_type);
    this->id = id->text;
    formals = params->formals_list->list;

    for (auto formal : formals) {
        if (formal->id == id->text) {
            output::errorDef(yylineno, formal->id);
            exit(0);
        }
    }

    symbol_table_stack.push_function_symbol(make_shared<Funcdecl>(*this));
}

Funcdecl::Funcdecl(shared_ptr<Override> override, shared_ptr<Rettype> return_type, shared_ptr<Node> id, shared_ptr<Formals> params) {
    is_override = override->is_override;
    ret_type = return_type;
    this->id = id->text;
    formals = params->formals_list->list;

     for (auto formal : formals) {
        if (formal->id == id->text) {
            output::errorDef(yylineno, formal->id);
            exit(0);
        }
    }

    symbol_table_stack.push_function_symbol(make_shared<Funcdecl>(*this), true);
}

Formalslist::Formalslist(Formaldecl* formaldecl) : list() {
    list.push_back(make_shared<Formaldecl>(*formaldecl));
}

Formalslist::Formalslist(shared_ptr<Formaldecl> formaldecl) : list() {
    list.push_back(formaldecl);
}

Formalslist::Formalslist(Formaldecl* formaldecl, Formalslist* formalslist) : list() {
    list.push_back(make_shared<Formaldecl>(*formaldecl));
    if (formalslist != nullptr)
        list.insert(list.end(), formalslist->list.begin(), formalslist->list.end());

    set<string> names;
    for (auto formal : list) {
        if (names.find(formal->id) != names.end()) {
            output::errorDef(yylineno, formal->id);
            exit(0);
        }
        names.insert(formal->id);
    }
}

Formaldecl::Formaldecl(Type* type, Node* id) : type(type), id(id->text) {
    if (symbol_table_stack.get_symbol(id->text) != nullptr) {
        output::errorDef(yylineno, id->text);
        exit(0);
    }
}

Exp::Exp(Exp* exp) : type(exp->type), value(exp->value), is_var(exp->is_var), 
                    reg(exp->reg), true_list(exp->true_list), false_list(exp->false_list), next_list(exp->next_list) {}

Exp::Exp(Exp* exp1, Node* op, Exp* exp2, Label* label){
    if (exp1->is_var){
        symbol_table_stack.verify_symbol(exp1->value);
    }
    if (exp2->is_var){
        symbol_table_stack.verify_symbol(exp2->value);
    }
    if (op->text == "+" || op->text == "-" || op->text == "*" || op->text == "/") {
        if (exp1->type != "int" && exp1->type != "byte") {
            output::errorMismatch(yylineno);
            exit(0);
        }
        if (exp2->type != "int" && exp2->type != "byte") {
            output::errorMismatch(yylineno);
            exit(0);
        }

        if (exp1->type == "int" || exp2->type == "int") {
            type = "int";
        }
        else {
            type = "byte";
        }
        gen_ir.gen_binop(*this, *exp1, *exp2, op->text);
    }
    else if (op->text == "and" || op->text == "or") {
        if (exp1->type != "bool") {
            output::errorMismatch(yylineno);
            exit(0);
        }
        if (exp2->type != "bool") {
            output::errorMismatch(yylineno);
            exit(0);
        }
        type = "bool";
        gen_ir.gen_boolop(*this, *exp1, *exp2, op->text, label->text);
    }
    else { // ==|!=|<|>|<=|>=   
        if (exp1->type != "int" && exp1->type != "byte") {
            output::errorMismatch(yylineno);
            exit(0);
        }
        if (exp2->type != "int" && exp2->type != "byte") {
            output::errorMismatch(yylineno);
            exit(0);
        }
        type = "bool";
        gen_ir.gen_relop(*this, *exp1, *exp2, op->text);
    }
}

// str = id or num or string or true or false
Exp::Exp(Node* str) : value(str->text) {
    if (str->text == "true" || str->text == "false"){
        type = "bool";
        value = str->text;
        gen_ir.gen_bool(*this);
        return;
    }
    if (str->text[0] == '\"') {
        type = "string";
        gen_ir.gen_string(*this);
        return;
    }
    if (isdigit(str->text[0])) {
        type = "int";
        reg = gen_ir.new_reg();
        gen_ir.gen_int_and_byte(*this);
        return;
    }
    symbol_table_stack.verify_symbol(str->text);
    type = symbol_table_stack.get_symbol(str->text)->type;
    is_var = true;
    gen_ir.gen_id(*this);
}

Exp::Exp(Call* call) {
    symbol_table_stack.verify_symbol(call->id);
    type = call->ret_type;
    reg = call->reg;
    true_list = call->true_list;
    false_list = call->false_list;
}

Exp::Exp(Node* str1, Node* byte){
    type = "byte";
    int value = stoi(str1->text);
    if (value > 255) {
        output::errorByteTooLarge(yylineno, str1->text);
        exit(0);
    }
    reg = gen_ir.new_reg();
    gen_ir.gen_int_and_byte(*this);
}

// str = not
Exp::Exp(Node* str1, Exp* exp){
    if (exp->type != "bool") {
        output::errorMismatch(yylineno);
        exit(0);
    }
    type = "bool";
    gen_ir.gen_notop(*this, *exp);
}

Exp::Exp(Type* type, Exp* exp){
    if (exp->is_var){
        symbol_table_stack.verify_symbol(exp->value);
    }
    if (type->type == "int" || type->type == "byte") {
        if (exp->type != "int" && exp->type != "byte") {
            output::errorMismatch(yylineno);
            exit(0);
        }
        this->type = type->type;
        this->value = exp->value;
        this->reg = exp->reg;
        return;
    }
    output::errorMismatch(yylineno);
    exit(0);
}

Statement::Statement(Type* type, Node* id) {
    symbol_table_stack.push_symbol(type->type, id->text);
}

Statement::Statement(Type* type, Node* id, Exp* exp){
    if (exp->is_var){
        symbol_table_stack.verify_symbol(exp->value);
    }
    if (type->type != exp->type) {
        if (!(type->type == "int" && exp->type == "byte")) {
            output::errorMismatch(yylineno);
            exit(0);
        }  
    }
    symbol_table_stack.push_symbol(type->type, id->text);
}

// str = id or return
Statement::Statement(Node* str, Exp* exp) {
    if (str->text == "return") {
        if (exp->is_var){
            symbol_table_stack.verify_symbol(exp->value);
        }
        if (symbol_table_stack.verify_return_type("void")) {
            output::errorMismatch(yylineno);
            exit(0);
        }
        if(symbol_table_stack.verify_return_type("int") && exp->type == "byte"){
            return;
        }
        if (!(symbol_table_stack.verify_return_type(exp->type))) {
            output::errorMismatch(yylineno);
            exit(0);
        }  
    }
    else {
        symbol_table_stack.verify_symbol(str->text);
        auto symbol = symbol_table_stack.get_symbol(str->text);
        if (symbol->type == "function") {
            output::errorUndef(yylineno, str->text);
            exit(0);
        }
        if (symbol->type != exp->type) {
            if (!(symbol->type == "int" && exp->type == "byte")) {
                output::errorMismatch(yylineno);
                exit(0);
            }  
        }
    }
}

// str = break or continue or return
Statement::Statement(Node* str) {
    if (str->text == "return") {
        if (!symbol_table_stack.verify_return_type("void")) {
            output::errorMismatch(yylineno);
            exit(0);
        }
    }
    else if (str->text == "break") {
        if (!symbol_table_stack.is_loop()) {
            output::errorUnexpectedBreak(yylineno);
            exit(0);
        }
    }
    // str = continue
    else {
        if (!symbol_table_stack.is_loop()) {
            output::errorUnexpectedContinue(yylineno);
            exit(0);
        }
    }
}

Call::Call(Node* id) : id(id->text), exp_list(make_shared<Explist>()) {
    ret_type = symbol_table_stack.match_function_symbol(id->text, exp_list->expressions)->ret_type->type->type;
    gen_ir.gen_call(*this);
}

Call::Call(Node* id, Explist* exp_list) : id(id->text), exp_list(exp_list) {
    ret_type = symbol_table_stack.match_function_symbol(id->text, exp_list->expressions)->ret_type->type->type;
    gen_ir.gen_call(*this);
}

Explist::Explist(Exp* exp, Explist* exp_list) : expressions(){
    expressions.push_back(make_shared<Exp>(*exp));
    if (exp_list != nullptr) {
        expressions.insert(expressions.end(), exp_list->expressions.begin(), exp_list->expressions.end());
    }
}

Explist::Explist(Exp* exp) : expressions(){
    expressions.push_back(make_shared<Exp>(*exp));
}