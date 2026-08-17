#ifndef LEXER_H
#define LEXER_H

typedef enum {
    // Keywords
    TOKEN_IMPORT,
    TOKEN_CLASS,
    TOKEN_STRUCT,
    TOKEN_SELF,
    TOKEN_FN,
    TOKEN_LET,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_IN,
    TOKEN_RETURN,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_PRINT,
    TOKEN_ALLOC,
    TOKEN_LIST_NEW,
    TOKEN_TYPE_INT,
    TOKEN_TYPE_FLOAT,
    TOKEN_TYPE_CHAR,
    TOKEN_TYPE_BOOL,
    TOKEN_TYPE_STRING,
    TOKEN_TYPE_VOID,
    TOKEN_TRUE,
    TOKEN_FALSE,

    // Identifiers and Literals
    TOKEN_IDENT,
    TOKEN_INT_LIT,
    TOKEN_FLOAT_LIT,
    TOKEN_STRING_LIT,
    TOKEN_CHAR_LIT,

    // Symbols & Operators
    TOKEN_LPAREN,     // (
    TOKEN_RPAREN,     // )
    TOKEN_LBRACE,     // {
    TOKEN_RBRACE,     // }
    TOKEN_LBRACKET,   // [
    TOKEN_RBRACKET,   // ]
    TOKEN_COMMA,      // ,
    TOKEN_COLON,      // :
    TOKEN_SEMICOLON,  // ;
    TOKEN_ARROW,      // ->
    TOKEN_ASSIGN,     // =
    TOKEN_EQ,         // ==
    TOKEN_NE,         // !=
    TOKEN_LT,         // <
    TOKEN_GT,         // >
    TOKEN_LE,         // <=
    TOKEN_GE,         // >=
    TOKEN_PLUS,       // +
    TOKEN_MINUS,      // -
    TOKEN_STAR,       // *
    TOKEN_SLASH,      // /
    TOKEN_PERCENT,    // %
    TOKEN_NOT,        // !
    TOKEN_AND,        // &&
    TOKEN_OR,         // ||
    TOKEN_DOT,        // .
    TOKEN_AMP,        // &

    // Special
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char *lexeme;
    int line;
    int col;
} Token;

typedef struct {
    Token *tokens;
    int count;
    int capacity;
} TokenArray;

TokenArray lex_source(const char *source);
void free_tokens(TokenArray *array);
const char *token_type_to_string(TokenType type);

#endif // LEXER_H
