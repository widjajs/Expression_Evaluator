#include "../includes/compiler.h"
#include "../includes/object.h"

Parser_t parser;
Chunk_t *cur_chunk;

static void go_next();
static void expression();
static void consume(TokenType_t type, const char *msg);
static void report_error(Token_t *token, const char *msg);
static void stop_compiler();

static void number();
static void grouping();
static void unary();
static void binary();
static void literal();
static void string();
static void let();
static void parse_precedence(Precedence_t prec);

static bool match(TokenType_t type);
static void statement();
static void declaration();
static int parse_let(const char *msg);

bool compile(const char *code, Chunk_t *chunk) {
    init_scanner(code);
    cur_chunk = chunk;
    parser.has_error = false;
    parser.is_panicking = false;
    go_next();
    while (!match(TOKEN_END_FILE)) {
        declaration();
    }
    stop_compiler();
    return !parser.has_error;
}

// ===================================================================================================

static Chunk_t *get_cur_chunk() {
    return cur_chunk;
}

static void emit_byte(uint8_t byte) {
    write_chunk(get_cur_chunk(), byte, parser.prev.line);
}

// convenience function for writing opcode followed by 1-byte operand
static void emit_bytes(uint8_t byte_1, uint8_t byte_2) {
    emit_byte(byte_1);
    emit_byte(byte_2);
}

static void stop_compiler() {
    emit_byte(OP_RETURN);
#ifdef DEBUG_PRINT_CODE
    if (!parser.has_error) {
        disassemble_chunk(get_cur_chunk(), "Code");
    }
#endif
}

// ===================================================================================================

ParseRule_t rules[] = {
    [TOKEN_OPEN_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_CLOSE_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_OPEN_CURLY] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLOSE_CURLY] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUB] = {unary, binary, PREC_ADD_SUB},
    [TOKEN_ADD] = {NULL, binary, PREC_ADD_SUB},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_DIV] = {NULL, binary, PREC_MUL_DIV},
    [TOKEN_MUL] = {NULL, binary, PREC_MUL_DIV},
    [TOKEN_NOT] = {unary, NULL, PREC_NONE},
    [TOKEN_NOT_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, binary, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER_THAN] = {NULL, binary, PREC_COMPARE},
    [TOKEN_GREATER_THAN_EQUAL] = {NULL, binary, PREC_COMPARE},
    [TOKEN_LESS_THAN] = {NULL, binary, PREC_COMPARE},
    [TOKEN_LESS_THAN_EQUAL] = {NULL, binary, PREC_COMPARE},
    [TOKEN_IDENTIFIER] = {let, NULL, PREC_NONE},
    [TOKEN_STR] = {string, NULL, PREC_NONE},
    [TOKEN_NUM] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUNC] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NONE] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_LET] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_END_FILE] = {NULL, NULL, PREC_NONE},
};

static void expression() {
    parse_precedence(PREC_ASSIGN);
}

static void print_statement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expected ';'. Got empty :(");
    emit_byte(OP_PRINT);
}

static void expression_statement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expected ';'. Put the semicolon please!");
    emit_byte(OP_POP);
}

void define_let(int global_id) {
    if (global_id <= 255) {
        emit_bytes(OP_DEFINE_GLOBAL, global_id);
    } else {
        emit_byte(OP_DEFINE_GLOBAL_LONG);
        emit_byte(global_id & 0xFF);         // lowest 8 bits
        emit_byte((global_id >> 8) & 0xFF);  // middle 8 bits
        emit_byte((global_id >> 16) & 0xFF); // front 8 bits
    }
}

static void let_declaration() {
    int global_id = parse_let("Expected variable name. LET's put a great name :)");

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emit_byte(OP_NONE);
    }
    consume(TOKEN_SEMICOLON, "Expected ';'. Put the semicolon please!");
    define_let(global_id);
}

// get us out of panic mode by consuming till the next semicolon
static void synchronize() {
    parser.is_panicking = false;
    while (parser.cur.type != TOKEN_END_FILE) {
        if (parser.prev.type == TOKEN_SEMICOLON) {
            return;
        }
        if (parser.cur.type == TOKEN_RETURN) {
            return;
        }
        go_next();
    }
}

static void declaration() {
    if (match(TOKEN_LET)) {
        let_declaration();
    } else {
        statement();
    }
    if (parser.is_panicking) {
        synchronize();
    }
}

static bool match(TokenType_t type) {
    if (parser.cur.type == type) {
        go_next();
        return true;
    }
    return false;
}

static void statement() {
    if (match(TOKEN_PRINT)) {
        print_statement();
    } else {
        expression_statement();
    }
}

static void string() {
    write_constant(get_cur_chunk(),
                   DECL_OBJ_VAL(allocate_str(parser.prev.start + 1, parser.prev.length - 2)),
                   parser.prev.line);
}

static void named_let(Token_t name) {
    int arg = add_constant(get_cur_chunk(),
                           DECL_OBJ_VAL(allocate_str(parser.prev.start, parser.prev.length)));
    if (arg <= 255) {
        emit_bytes(OP_GET_GLOBAL, arg);
    } else {
        emit_byte(OP_GET_GLOBAL_LONG); // new opcode for long globals
        emit_byte(arg & 0xFF);         // lowest 8 bits
        emit_byte((arg >> 8) & 0xFF);  // middle 8 bits
        emit_byte((arg >> 16) & 0xFF); // highest 8 bits
    }
}

static void let() {
    named_let(parser.prev);
}

// ===================================================================================================

static void parse_precedence(Precedence_t prec) {
    go_next();
    ParseFunc_t prefix_rule = rules[parser.prev.type].prefix_rule;
    if (prefix_rule == NULL) {
        report_error(&parser.prev, "Expected expression");
        return;
    }

    prefix_rule();

    while (prec <= rules[parser.cur.type].precedence) {
        go_next();
        ParseFunc_t infix_rule = rules[parser.prev.type].infix_rule;
        infix_rule();
    }
}

static int parse_let(const char *msg) {
    // parse variable and add constant byte to chunk
    consume(TOKEN_IDENTIFIER, msg);
    return add_constant(get_cur_chunk(),
                        DECL_OBJ_VAL(allocate_str(parser.prev.start, parser.prev.length)));
}

static void literal() {
    switch (parser.prev.type) {
        case TOKEN_FALSE:
            emit_byte(OP_FALSE);
            break;
        case TOKEN_TRUE:
            emit_byte(OP_TRUE);
            break;
        case TOKEN_NONE:
            emit_byte(OP_NONE);
            break;
        default:
            return;
    }
}

static void number() {
    double val = strtod(parser.prev.start, NULL);
    write_constant(get_cur_chunk(), DECL_NUM_VAL(val), parser.prev.line);
}

static void grouping() {
    expression();
    consume(TOKEN_CLOSE_PAREN, "Expect ')' after expression");
}

static void unary() {
    TokenType_t op_type = parser.prev.type;
    parse_precedence(PREC_UNARY);

    // negate operator emitted last bc we need value first so we have smtg to negate
    switch (op_type) {
        case TOKEN_NOT:
            emit_byte(OP_NOT);
            break;
        case TOKEN_SUB:
            emit_byte(OP_NEGATE);
            break;
        default:
            return;
    }
}

static void binary() {
    // left operator
    TokenType_t op_type = parser.prev.type;

    // parse right expression
    ParseRule_t *rule = &rules[op_type];
    parse_precedence((Precedence_t)(rule->precedence + 1));

    // write the op instruction
    switch (op_type) {
        case TOKEN_NOT_EQUAL:
            emit_bytes(OP_EQUAL, OP_NOT);
            break;
        case TOKEN_LESS_THAN:
            emit_byte(OP_LESS_THAN);
            break;
        case TOKEN_LESS_THAN_EQUAL:
            emit_bytes(OP_GREATER_THAN, OP_NOT);
            break;
        case TOKEN_GREATER_THAN:
            emit_byte(OP_GREATER_THAN);
            break;
        case TOKEN_GREATER_THAN_EQUAL:
            emit_bytes(OP_LESS_THAN, OP_NOT);
            break;
        case TOKEN_EQUAL_EQUAL:
            emit_byte(OP_EQUAL);
            break;
        case TOKEN_ADD:
            emit_byte(OP_ADD);
            break;
        case TOKEN_SUB:
            emit_byte(OP_SUB);
            break;
        case TOKEN_MUL:
            emit_byte(OP_MUL);
            break;
        case TOKEN_DIV:
            emit_byte(OP_DIV);
            break;
        default:
            return;
    }
}

// ===================================================================================================

static void consume(TokenType_t type, const char *msg) {
    if (parser.cur.type == type) {
        go_next();
        return;
    }
    report_error(&parser.cur, msg);
}

static void go_next() {
    parser.prev = parser.cur;
    while (true) {
        parser.cur = scan_token();
        if (parser.cur.type != TOKEN_ERROR) {
            break;
        }
        report_error(&parser.cur, parser.cur.start);
    }
}

static void report_error(Token_t *token, const char *msg) {
    if (parser.is_panicking) {
        // if parser is panicking (err was found earlier) just ignore the errors and keep going
        return;
    }
    parser.is_panicking = true;
    fprintf(stderr, "[line %d] Error", token->line);
    if (token->type == TOKEN_END_FILE) {
        fprintf(stderr, " end of file");
    } else if (token->type != TOKEN_ERROR) {
        // error tokens are not stored in entirety so only print the lexme if token != error
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", msg);
    parser.has_error = true;
}
