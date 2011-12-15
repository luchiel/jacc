#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "generator.h"
#include "memory.h"
#include "symtable.h"
#include "parser.h"

struct asm_command commands[] = {
#define COMMAND(name, repr, op_count) { #repr, op_count },
#include "commands.def"
#undef COMMAND
};

struct asm_operand registers[] = {
#define REGISTER(name, repr) { AOT_REGISTER, {}},
#include "registers.def"
#undef REGISTER
};

#define REGISTER(name, repr) struct asm_operand * repr = & registers[ART_##name];
#include "registers.def"
#undef REGISTER

code_t cur_code;
int label_counter;

static int print_operand(struct asm_operand *op)
{
    if (op == NULL) {
        return 0;
    }

    switch (op->type) {
    case AOT_MEMORY:
    {
        int cnt = 0;
        printf("[");
        cnt += print_operand(op->data.memory.base);
        if (op->data.memory.index != NULL) {
            if (cnt) printf(" + ");
            cnt += print_operand(op->data.memory.index);
            if (op->data.memory.scale != 1) {
                printf("*%d", op->data.memory.scale);
            }
        }

        if (op->data.memory.offset != NULL) {
            if (cnt) printf(" + ");
            cnt += print_operand(op->data.memory.offset);
        }

        printf("]");
        break;
    }
    case AOT_REGISTER:
        printf("%s", op->data.register_name);
        break;
    case AOT_CONSTANT:
        printf("%d", op->data.value);
        break;
    case AOT_LABEL:
        printf("_%s", op->data.label);
        break;
    case AOT_SIZE:
        switch (op->data.size.size) {
        case AOS_BYTE: printf("byte "); break;
        case AOS_WORD: printf("word "); break;
        case AOS_DWORD: printf("dword "); break;
        case AOS_QWORD: printf("qword "); break;
        }
        print_operand(op->data.size.subop);
        break;
    }
    return 1;
}

static void print_opcode(struct asm_opcode *opcode)
{
    switch (opcode->type) {
    case ACT_COMMAND:
    {
        int i;
        struct asm_command *cmd = opcode->data.command.cmd;
        printf("\t%s", cmd->name);
        for (i = 0; i < cmd->op_count; i++) {
            printf(i == 0 ? " " : ", ");
            print_operand(opcode->data.command.ops[i]);
        }
        printf("\n");
        break;
    }
    case ACT_NOP:
        break;
    case ACT_TEXT:
    case ACT_DATA:
        printf("%s\n", opcode->data.text);
        break;
    }
}

static void add_opcode(struct asm_opcode *opcode)
{
    int new_size = 0;
    if (cur_code->opcode_count == 0) {
        new_size = 64;
    } else if (cur_code->opcode_count + 1 > cur_code->opcode_list_size) {
        new_size = cur_code->opcode_list_size * 2;
    }

    if (new_size) {
        cur_code->opcode_list_size = new_size;
        cur_code->opcodes = jacc_realloc(cur_code->opcodes, new_size * sizeof(*cur_code->opcodes));
    }
    cur_code->opcodes[cur_code->opcode_count] = opcode;
    cur_code->opcode_count++;
}

static void emit(enum asm_command_type cmd, ...)
{
    struct asm_opcode *opcode = jacc_malloc(sizeof(*opcode));
    opcode->type = ACT_COMMAND;
    opcode->data.command.cmd = &commands[cmd];

    va_list args;
    va_start(args, cmd);
    int i;
    for (i = 0; i < commands[cmd].op_count; i++) {
        opcode->data.command.ops[i] = va_arg(args, struct asm_operand *);
    }
    va_end(args);

    add_opcode(opcode);
}

static void emit_text(char *format, ...)
{
    struct asm_opcode *opcode = jacc_malloc(sizeof(*opcode));
    opcode->type = ACT_TEXT;

    char *buffer = jacc_malloc(256);
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);

    opcode->data.text = buffer;
    add_opcode(opcode);
}

static void emit_label(char *name)
{
    emit_text("_%s:", name);
}

static void emit_data(char *data)
{
    struct asm_opcode *opcode = jacc_malloc(sizeof(*opcode));
    opcode->type = ACT_DATA;
    opcode->data.text = data;
    add_opcode(opcode);
}

static struct asm_operand *constant(int value)
{
    struct asm_operand *operand = jacc_malloc(sizeof(*operand));
    operand->type = AOT_CONSTANT;
    operand->data.value = value;
    return operand;
}

static struct asm_operand *label(const char *label)
{
    struct asm_operand *operand = jacc_malloc(sizeof(*operand));
    operand->type = AOT_LABEL;
    operand->data.label = label;
    return operand;
}

static struct asm_operand *memory(struct asm_operand *base, struct asm_operand *offset, struct asm_operand *index, int scale)
{
    struct asm_operand *operand = jacc_malloc(sizeof(*operand));
    operand->type = AOT_MEMORY;
    operand->data.memory.base = base;
    operand->data.memory.offset = offset;
    operand->data.memory.index = index;
    operand->data.memory.scale = scale;
    return operand;
}

static struct asm_operand *deref(struct asm_operand *base)
{
    return memory(base, NULL, NULL, 1);
}

static struct asm_operand *size_spec(enum asm_operand_size size, struct asm_operand *subop)
{
    struct asm_operand *operand = jacc_malloc(sizeof(*operand));
    operand->type = AOT_SIZE;
    operand->data.size.size = size;
    operand->data.size.subop = subop;
    return operand;
}

static struct asm_operand *dword(struct asm_operand *subop)
{
    return size_spec(AOS_DWORD, subop);
}

static char *gen_label()
{
    char *buf = jacc_malloc(8);
    sprintf(buf, "@%d", label_counter);
    label_counter++;
    return buf;
}

static void generate_expr(struct node *expr);

static struct asm_operand *lvalue(struct node *expr)
{
    switch (expr->type) {
    case NT_VARIABLE:
    {
        struct var_node *var = (struct var_node*)expr;
        switch (var->symbol->type) {
        case ST_PARAMETER:
            return dword(memory(ebp, constant(var->symbol->offset + 8), NULL, 1));
        case ST_VARIABLE:
            return dword(memory(ebp, constant(var->symbol->offset - 4), NULL, 1));
        case ST_GLOBAL_VARIABLE:
            if (var->symbol->label == NULL) {
                var->symbol->label = gen_label();
                char *buf = jacc_malloc(30);
                sprintf(buf, "_%s db %d dup(0)", var->symbol->label, var->symbol->size);
                emit_data(buf);
            }
            return dword(deref(label(var->symbol->label)));
        default:
            emit_text("; unhandled var symbol %d", var->symbol->type);
        }
        break;
    }
    case NT_DEREFERENCE:
        generate_expr(expr->ops[0]);
        emit(ASM_POP, eax);
        return deref(eax);
    case NT_MEMBER:
        emit(ASM_LEA, eax, lvalue(expr->ops[0]));
        return memory(eax, constant(expr->ops[1]->type_sym->offset), NULL, 1);
    default:
        emit_text("; '%s' is not lvalue", parser_node_info(expr)->repr);
    }
    return deref(constant(0));
}

static void generate_int_cmp(enum asm_command_type cmd)
{
    emit(ASM_XOR, ecx, ecx);
    emit(ASM_CMP, eax, ebx);
    emit(cmd, cl);
    emit(ASM_MOV, eax, ecx);
}

static void generate_int_logical_op(enum asm_command_type cmd)
{
    char *lbl = gen_label();
    emit(ASM_XOR, ecx, ecx);
    emit(ASM_TEST, eax, eax);
    emit(cmd, label(lbl));
    emit(ASM_TEST, ebx, ebx);
    emit_label(lbl);
    emit(ASM_SETNZ, cl);
    emit(ASM_MOV, eax, ecx);
}

static void generate_unary_int_op(struct node *expr)
{
    generate_expr(expr->ops[0]);
    emit(ASM_POP, eax);
    switch (expr->type) {
    case NT_LOGICAL_NEGATION:
        emit(ASM_XOR, ecx, ecx);
        emit(ASM_TEST, eax, eax);
        emit(ASM_SETZ, cl);
        emit(ASM_MOV, eax, eax);
        break;
    case NT_COMPLEMENT: emit(ASM_NOT, eax); break;
    case NT_NEGATION: emit(ASM_NEG, eax); break;
    case NT_IDENTITY: break;
    default:
        emit_text("; unknown unary node %s", parser_node_info(expr)->repr);
    }
    emit(ASM_PUSH, eax);
}

static void generate_binary_int_op(struct node *expr)
{
    generate_expr(expr->ops[0]);
    generate_expr(expr->ops[1]);
    emit(ASM_POP, ebx);
    emit(ASM_POP, eax);

    switch (expr->type) {
    case NT_ADD: emit(ASM_ADD, eax, ebx); break;
    case NT_SUB: emit(ASM_SUB, eax, ebx); break;
    case NT_MUL: emit(ASM_IMUL, ebx); break;
    case NT_LSHIFT:
        emit(ASM_MOV, ecx, ebx);
        emit(ASM_SAL, eax, cl);
        break;
    case NT_RSHIFT:
        emit(ASM_MOV, ecx, ebx);
        emit(ASM_SAR, eax, cl);
        break;
    case NT_DIV:
    case NT_MOD:
        emit(ASM_CDQ);
        emit(ASM_IDIV, ebx);
        if (expr->type == NT_MOD) emit(ASM_MOV, eax, edx);
        break;
    case NT_EQ: generate_int_cmp(ASM_SETE); break;
    case NT_NE: generate_int_cmp(ASM_SETNE); break;
    case NT_LE: generate_int_cmp(ASM_SETLE); break;
    case NT_LT: generate_int_cmp(ASM_SETL); break;
    case NT_GE: generate_int_cmp(ASM_SETGE); break;
    case NT_GT: generate_int_cmp(ASM_SETG); break;
    case NT_BIT_XOR: emit(ASM_XOR, eax, ebx); break;
    case NT_BIT_OR: emit(ASM_OR, eax, ebx); break;
    case NT_BIT_AND: emit(ASM_AND, eax, ebx); break;
    case NT_AND: generate_int_logical_op(ASM_JZ); break;
    case NT_OR: generate_int_logical_op(ASM_JNZ); break;
    default:
        emit_text("; unknown binary node %s", parser_node_info(expr)->repr);
    }
    emit(ASM_PUSH, eax);
}

static void generate_lvalue(struct node *expr)
{
    emit(ASM_LEA, eax, lvalue(expr));
    emit(ASM_PUSH, eax);
}

static void generate_statement(struct node *expr)
{
    switch (expr->type) {
    case NT_IF:
    {
        char *l1 = gen_label(), *l2 = gen_label();
        generate_expr(expr->ops[0]);
        emit(ASM_POP, eax);
        emit(ASM_TEST, eax, eax);
        emit(ASM_JZ, label(l1));
        generate_expr(expr->ops[1]);
        emit(ASM_JMP, label(l2));
        emit_label(l1);
        generate_expr(expr->ops[2]);
        emit_label(l2);
        break;
    }
    default:
        emit_text("; unknown statement %s", parser_node_info(expr)->repr);
    }
}

static void generate_expr(struct node *expr)
{
    switch (expr->type) {
    case NT_LIST:
    {
        struct list_node *list = (struct list_node*)expr;
        int i = 0;
        for (; i < list->size; i++) {
            generate_expr(list->items[i]);
        }
        return;
    }
    case NT_CALL:
    {
        struct list_node *list = (struct list_node*)expr->ops[1];
        int i = list->size - 1, size = 0;
        for (; i >= 0; i--) {
            generate_expr(list->items[i]);
            size += list->items[i]->type_sym->size;
        }
        struct asm_operand *target = label(expr->ops[0]->type_sym->name);
        if ((expr->ops[0]->type_sym->flags & SF_EXTERN) == SF_EXTERN) {
            target = deref(target);
        }
        emit(ASM_CALL, target); // @todo function pointers
        emit(ASM_ADD, esp, constant(size));
        return;
    }
    case NT_STRING:
    {
        const char *str = ((struct string_node*)expr)->value;
        int str_len = strlen(str);

        char *str_label = gen_label();
        char *buf = jacc_malloc(15 + 4 * str_len);
        char *ptr = buf;

        ptr += sprintf(ptr, "_%s db ", str_label);
        int i = 0;
        for (; i <= str_len; i++) {
            ptr += sprintf(ptr, i == 0 ? "%d" : ",%d", str[i]);
        }

        emit_data(buf);
        emit(ASM_PUSH, label(str_label));
        return;
    }
    case NT_MEMBER:
        emit(ASM_MOV, eax, lvalue(expr));
        emit(ASM_PUSH, eax);
        return;
    case NT_TERNARY:
    {
        char *l1 = gen_label(), *l2 = gen_label();
        generate_expr(expr->ops[0]);
        emit(ASM_POP, eax);
        emit(ASM_TEST, eax, eax);
        emit(ASM_JZ, label(l1));
        generate_expr(expr->ops[1]);
        emit(ASM_JMP, label(l2));
        emit_label(l1);
        generate_expr(expr->ops[2]);
        emit_label(l2);
        return;
    }
    case NT_ASSIGN:
    {
        generate_lvalue(expr->ops[0]);
        generate_expr(expr->ops[1]);
        emit(ASM_POP, eax);
        emit(ASM_POP, ebx);
        emit(ASM_MOV, deref(ebx), eax);
        emit(ASM_PUSH, eax);
        return;
    }
    case NT_CAST:
        generate_expr(expr->ops[0]);
        return;
    case NT_REFERENCE:
        emit(ASM_LEA, eax, lvalue(expr->ops[0]));
        emit(ASM_PUSH, eax);
        return;
    case NT_DEREFERENCE:
        generate_expr(expr->ops[0]);
        emit(ASM_POP, eax);
        emit(ASM_PUSH, dword(deref(eax)));
        return;
    case NT_VARIABLE:
        emit(ASM_PUSH, lvalue(expr));
        return;
    case NT_INT:
        emit(ASM_PUSH, constant(((struct int_node*)expr)->value));
        return;
    case NT_ADD:
    {
        struct symbol *s1 = resolve_alias(expr->ops[0]->type_sym);
        if (is_ptr_type(s1)) {
            generate_expr(expr->ops[0]);
            generate_expr(expr->ops[1]);
            emit(ASM_POP, eax);
            emit(ASM_MOV, ebx, constant(s1->base_type->size));
            emit(ASM_MUL, ebx);
            emit(ASM_POP, ebx);
            emit(ASM_ADD, eax, ebx);
            emit(ASM_PUSH, eax);
            return;
        }
        break;
    }
    }

    switch (parser_node_info(expr)->cat) {
    case NC_UNARY: generate_unary_int_op(expr); return;
    case NC_BINARY: generate_binary_int_op(expr); return;
    case NC_STATEMENT: generate_statement(expr); return;
    }

    emit_text("; unknown node %s", parser_node_info(expr)->repr);
}

static void generate_function(struct symbol *func)
{
    if ((func->flags & SF_EXTERN) == SF_EXTERN) {
        return;
    }

    int is_main = strcmp(func->name, "main") == 0;

    emit_text("; start %s", func->name);

    emit_text("_%s:", func->name);

    emit(ASM_PUSH, ebp);
    emit(ASM_MOV, ebp, esp);
    if (func->locals_size != 0) {
        emit(ASM_SUB, esp, constant(func->locals_size));
    }

    generate_expr(func->expr);

    if (func->locals_size != 0) {
        emit(ASM_ADD, esp, constant(func->locals_size));
    }

    emit(ASM_MOV, esp, ebp);
    emit(ASM_POP, ebp);

    if (is_main) {
        emit(ASM_PUSH, constant(0));
        emit(ASM_CALL, deref(label("ExitProcess")));
    } else {
        emit(ASM_RET);
    }

    emit_text("; end %s\n", func->name);
}

extern void generator_init()
{
#define REGISTER(name, repr) registers[ART_##name].data.register_name = #repr;
#include "registers.def"
#undef REGISTER

    label_counter = 0;
}

extern void generator_destroy()
{

}

extern code_t generator_process(symtable_t symtable)
{
    code_t code = jacc_malloc(sizeof(*code));
    code->opcode_count = 0;
    code->opcode_list_size = 0;
    code->opcodes = NULL;

    cur_code = code;

    symtable_iter_t iter = symtable_first(symtable);
    for (; iter != NULL; iter = symtable_iter_next(iter)) {
        struct symbol *symbol = symtable_iter_value(iter);
        if (symbol->type == ST_FUNCTION) {
            generate_function(symbol);
        }
    }
    return code;
}

extern void generator_print_code(code_t code)
{
    int i;
    printf("format PE console\nentry _main\n");
    printf("include '%%fasm%%/include/win32a.inc'\n\n");
    printf("section '.text' code executable\n");
    for (i = 0; i < code->opcode_count; i++) {
        if (code->opcodes[i]->type != ACT_DATA) {
            print_opcode(code->opcodes[i]);
        }
    }

    printf("section '.data' data readable writable\n\n");
    printf("placeholder db 0\n");
    for (i = 0; i < code->opcode_count; i++) {
        if (code->opcodes[i]->type == ACT_DATA) {
            print_opcode(code->opcodes[i]);
        }
    }

    printf("\nsection '.idata' data readable import\n");
    printf("library kernel32, 'kernel32.dll', msvcrt, 'msvcrt.dll'\n");
    printf("import kernel32, _ExitProcess, 'ExitProcess'\n");
    printf("import msvcrt, _printf, 'printf'\n");
}

static void free_memory_subop(struct asm_operand *operand)
{
    if (operand != NULL && operand->type == AOT_CONSTANT) {
        jacc_free(operand);
    }
}

extern void generator_free_operand_data(struct asm_operand *operand)
{
    switch (operand->type) {
    case AOT_MEMORY:
        free_memory_subop(operand->data.memory.base);
        free_memory_subop(operand->data.memory.index);
        free_memory_subop(operand->data.memory.offset);
        break;
    case AOT_SIZE:
        generator_free_operand_data(operand->data.size.subop);
        break;
    }
}

extern void generator_free_opcode_data(struct asm_opcode *opcode)
{
    switch (opcode->type) {
    case ACT_COMMAND:
    {
        int i;
        for (i = 0; i < opcode->data.command.cmd->op_count; i++) {
            generator_free_operand_data(opcode->data.command.ops[i]);
        }
        break;
    }
    case ACT_TEXT:
    case ACT_DATA:
        jacc_free(opcode->data.text);
        break;
    }
}

extern void generator_free_code(code_t code)
{
    if (code == NULL) {
        return;
    }

    int i;
    for (i = 0; i < code->opcode_count; i++) {
        generator_free_opcode_data(code->opcodes[i]);
        jacc_free(code->opcodes[i]);
    }
    jacc_free(code->opcodes);
    jacc_free(code);
}
