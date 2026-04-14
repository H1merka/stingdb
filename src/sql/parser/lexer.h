#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <string_view>

namespace stingdb::sql {

// Типы лексем (Lexer Tokens)
enum class TokenType {
    INVALID = 0,
    END_OF_FILE,
    IDENTIFIER,
    NUMBER,
    STRING_LITERAL,
    // Ключевые слова
    SELECT, FROM, WHERE, INSERT, INTO, VALUES, UPDATE, SET, DELETE,
    // Операторы
    PLUS, MINUS, STAR, SLASH, EQUALS, NOT_EQUALS, GREATER, LESS, AND, OR,
    // Пунктуация
    COMMA, SEMICOLON, LEFT_PAREN, RIGHT_PAREN
};

struct Token {
    TokenType type;
    std::string_view val;
    int line;
    int column;
};

/**
 * Лексический анализатор (Lexer).
 * Осуществляет предварительный проход по строке SQL, разбивая ее на потоки токенов.
 * Использование std::string_view (zero-copy) гарантирует высочайшую производительность лексера,
 * так как мы избегаем аллокаций std::string на каждый идентификатор.
 */
class Lexer {
public:
    explicit Lexer(std::string_view query) : query_(query), pos_(0), line_(1), col_(1) {}

    // Извлекает следующий токен из потока
    Token NextToken();

    // Заглядывает вперед без сдвига указателя
    Token PeekToken();

private:
    std::string_view query_;
    std::size_t pos_;
    int line_;
    int col_;

    char PeekChar() const { return (pos_ < query_.size()) ? query_[pos_] : '\0'; }
    char NextChar() { col_++; return query_[pos_++]; }
    void SkipWhitespace();
    
    Token ParseIdentifier();
    Token ParseNumber();
    Token ParseStringLiteral();
};

} // namespace stingdb::sql