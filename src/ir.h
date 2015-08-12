#ifndef IR_H
#define IR_H

#include "type.h"
#include "symbol.h"
#include "util/list.h"

#include <stddef.h>

/* A reference to some storage location or direct value, used in intermediate
 * representation of expressions.
 */
struct var
{
    const struct typetree *type;
    const struct symbol *symbol;

    enum {
        /* l-value or r-value reference to symbol, which must have some storage
         * location. Offset evaluate to *(&symbol + offset). Offset in bytes,
         * not pointer arithmetic. */
        DIRECT,
        /* l-value or r-value reference to *(symbol + offset). Symbol must have
         * pointer type. Offset in bytes, not pointer arithmetic. */
        DEREF,
        /* r-value immediate, with the type specified. Symbol is NULL. */
        IMMEDIATE
    } kind;

    union immediate {
        signed char i1;
        signed short i2;
        signed int i4;
        signed long i8;
        unsigned char u1;
        unsigned short u2;
        unsigned int u4;
        unsigned long u8;
    } value;

    /* Represent string constant value, or label, for IMMEDIATE values. If type
     * is char [], this is the literal string constant. If type is char *, this
     * is the label representing the string, as in '.LC1'. Pointers can have a
     * constant offset, representing address constants such as .LC1+3. */
    const char *string;

    int offset;
    int lvalue;
};

/* A direct reference to given symbol.
 */
struct var var_direct(const struct symbol *sym);

/* A string value of type [] char.
 */
struct var var_string(const char *str);

/* A constant value of integer type.
 */
struct var var_int(int value);

/* A zero constant value of integer type.
 */
struct var var_zero(int size);

/* A value with no type.
 */
struct var var_void(void);

/* Create a variable of the given type, returning a direct reference to a new
 * symbol.
 */
struct var create_var(const struct typetree *type);

/* Three address code operation types.
 */
enum optype
{
    IR_PARAM,    /* param a    */

    IR_ASSIGN,   /* a = b      */
    IR_DEREF,    /* a = *b     */
    IR_ADDR,     /* a = &b     */
    IR_CALL,     /* a = b()    */
    IR_CAST,     /* a = (T) b  */

    IR_OP_ADD,   /* a = b + c  */
    IR_OP_SUB,   /* a = b - c  */
    IR_OP_MUL,   /* a = b * c  */
    IR_OP_DIV,   /* a = b / c  */
    IR_OP_MOD,   /* a = b % c  */
    IR_OP_AND,   /* a = b & c  */
    IR_OP_OR,    /* a = b | c  */
    IR_OP_XOR,   /* a = b ^ c  */
    IR_OP_SHL,   /* a = b << c */
    IR_OP_SHR,   /* a = b >> c */

    IR_OP_EQ,    /* a = b == c */
    IR_OP_GE,    /* a = b >= c */
    IR_OP_GT,    /* a = b > c  */

    /* Call va_start(a), setting reg_save_area and overflow_arg_area. This,
     * together with va_arg assumes some details about memory layout that can
     * only be known by backend, thus the need for these operations. */
    IR_VA_START,

    /* Call a = va_arg(b, T), with type T taken from a. Intercepted as call to
     * __builtin_va_arg in parser. */
    IR_VA_ARG
};

/* Find the number of operands to a given operation type, using the fact that
 * enumeration constants are sorted by operand count. 
 */
#define NOPERANDS(t) ((t) > 5 ? 2 : (t) > 1)

#define IS_COMPARISON(t) ((t) > 15)

/* CFG block.
 */
struct block
{
    /* A unique jump target label. */
    const char *label;

    /* Realloc-able list of 3-address code operations. */
    struct op {
        enum optype type;
        struct var a;
        struct var b;
        struct var c;
    } *code;

    /* Number of ir operations. */
    int n;

    /* Toggle last statement was return, meaning expr is valid. There are cases
     * where we reach end of control in a non-void function, but not wanting to
     * return a value. For example when exit has been called. */
    int has_return_value;

    /* Value to evaluate in branch conditions, or return value. Also used for
     * return value from expression parsing rules, as a convenience. The
     * decision on whether this block is a branch or not is done purely based
     * on the jump target list. */
    struct var expr;

    /* Branch targets.
     * - (NULL, NULL): Terminal node, return expr from function.
     * - (x, NULL)   : Unconditional jump, f.ex break, goto, or bottom of loop.
     * - (x, y)      : False and true branch targets, respectively.
     */
    const struct block *jump[2];
};

/* Represents an external declaration list or a function definition.
 */
struct decl
{
    /* Function symbol or NULL if list of declarations. */
    const struct symbol *fun;
    struct block *head;
    struct block *body;

    /* Number of bytes to allocate to local variables on stack. */
    int locals_size;

    /* Store all symbols associated with a function declaration. */
    struct list *params;
    struct list *locals;

    /* Store all associated nodes in a list to simplify deallocation. */
    struct block **nodes;
    size_t size;
    size_t capacity;
};

/* Initialize a new control flow graph structure.
 */
struct decl *cfg_create(void);

/* Initialize a CFG block with a unique jump label, and associate it with the
 * provided decl object. Blocks and functions have the same lifecycle, and 
 * should only be freed by calling cfg_finalize.
 */
struct block *cfg_block_init(struct decl *);

/* Add a 3-address code operation to the block. Code is kept in a separate list
 * for each block.
 */
void cfg_ir_append(struct block *, struct op);

/* Release all resources related to the control flow graph. Calls free on all
 * blocks and their labels, and finally the struct decl object itself.
 */
void cfg_finalize(struct decl *);

#endif
