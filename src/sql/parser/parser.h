#pragma once

#include "sql/parser/lexer.h"
#include <memory>
#include <vector>
#include <string>

namespace stingdb::sql {

class Parser {
public:
    explicit Parser(std::string_view query) : lexer_(query) {}

    // // Метод рекурсивного спуска (Top-Down Parsing), создающий AST.
    // Если происходит синтаксическая ошибка, бросается exception (std::runtime_error).
    // std::unique_ptr<ASTNode> ParseQuery();

private:
   Lexer lexer_;

   // Набор правил для рекурсивного спуска:
   // std::unique_ptr<SelectStatement> ParseSelect();
   // std::unique_ptr<InsertStatement> ParseInsert();
   // std::unique_ptr<Expression>     ParseExpression(int precedence = 0);
};

} // namespace stingdb::sql