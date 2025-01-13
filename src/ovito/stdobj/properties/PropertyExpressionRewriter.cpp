////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#include "PropertyExpressionRewriter.h"

#include <algorithm>

namespace Ovito::PropertyExpressionRewriter {

/******************************************************************************
 * Tokenize an expression
 ******************************************************************************/
[[nodiscard]] QStringList tokenizeExpression(QString expression)
{
    // Regular expressions for tokens - split on expected operators, parenthesis, and group everything inside "...".
    const static QRegularExpression regex(QStringLiteral(R"((".*?"|'.*?'|==|!=|>=|<=|\?|:|\(|\)|&&|\|\||[\w\._]+|\S))"));
    OVITO_ASSERT(regex.isValid());

    // Tokenize
    QStringList tokens;
    for(const auto& match : regex.globalMatch(expression)) {
        OVITO_ASSERT(match.hasMatch());
        tokens << match.captured();
    }
    return tokens;
}

/******************************************************************************
 * Convert an operation (Op) enum to its string representation.
 ******************************************************************************/
inline QString OpToString(Op op)
{
    switch(op) {
        case(Op::AND): return QStringLiteral("&&");
        case(Op::OR): return QStringLiteral("||");
        case(Op::EQ): return QStringLiteral("==");
        case(Op::NEQ): return QStringLiteral("!=");
        case(Op::GE): return QStringLiteral(">");
        case(Op::GEQ): return QStringLiteral(">=");
        case(Op::LE): return QStringLiteral("<");
        case(Op::LEQ): return QStringLiteral("<=");
        case(Op::QM): return QStringLiteral("?");
        case(Op::COL): return QStringLiteral(":");
        case(Op::ADD): return QStringLiteral("+");
        case(Op::SUB): return QStringLiteral("-");
        case(Op::MULT): return QStringLiteral("*");
        case(Op::DIV): return QStringLiteral("/");
        case(Op::POW): return QStringLiteral("^");
        default: OVITO_ASSERT(false);
    }
    return {};
}

/******************************************************************************
 * Convert a string to operation (Op) enum.
 ******************************************************************************/
inline Op StringToOp(const QString& str)
{
    if(str == QStringLiteral("&&"))
        return Op::AND;
    else if(str == QStringLiteral("||"))
        return Op::OR;
    else if(str == QStringLiteral("=="))
        return Op::EQ;
    else if(str == QStringLiteral("!="))
        return Op::NEQ;
    else if(str == QStringLiteral(">"))
        return Op::GE;
    else if(str == QStringLiteral(">="))
        return Op::GEQ;
    else if(str == QStringLiteral("<"))
        return Op::LE;
    else if(str == QStringLiteral("<="))
        return Op::LEQ;
    else if(str == QStringLiteral("?"))
        return Op::QM;
    else if(str == QStringLiteral(":"))
        return Op::COL;
    else if(str == QStringLiteral("+"))
        return Op::ADD;
    else if(str == QStringLiteral("-"))
        return Op::SUB;
    else if(str == QStringLiteral("*"))
        return Op::MULT;
    else if(str == QStringLiteral("/"))
        return Op::DIV;
    else if(str == QStringLiteral("^"))
        return Op::POW;
    OVITO_ASSERT(false);
    return Op::NONE;
}

/******************************************************************************
 * Represents an identifier, e.g. 'ParticleType', 'fcc', 1, ...
 * Can be created from a QString or a QStringlist of length 1
 * Note: no data is guaranteed to be stored -> original source must be kept alive
 ******************************************************************************/
struct Identifier : ASTNode {
public:
    explicit Identifier(const QString* name) : ASTNode{ASTNodeType::IDENTIFIER}, _nameStr{name} { OVITO_ASSERT(name); }

    explicit Identifier(const QStringList* names) : ASTNode{ASTNodeType::IDENTIFIER}, _namesList{names}
    {
        OVITO_ASSERT(names);
        OVITO_ASSERT(names->size() == 1);
    }

    explicit Identifier(const QString& string) : ASTNode{ASTNodeType::IDENTIFIER}, _namesList{&_names}, _names{string} {}

    // Returns the name as QString from either the QString or the QStringlist
    const QString& name() const
    {
        OVITO_ASSERT((bool)_namesList ^ (bool)_nameStr);
        if(_namesList) {
            OVITO_ASSERT(_namesList->size() == 1);
            return _namesList->first();
        }
        else {
            OVITO_ASSERT(_nameStr);
            return *_nameStr;
        }
    }

    // Returns the name as QStringList from either the QString or the QStringlist
    const QStringList& namesList() const
    {
        OVITO_ASSERT((bool)_namesList ^ (bool)_nameStr);
        if(_namesList) {
            OVITO_ASSERT(_namesList && _namesList->size() == 1);
            return *_namesList;
        }
        else {
            if(_names.size() == 1) {
                return _names;
            }
            else {
                _names << *_nameStr;
                return _names;
            }
        }
    }

private:
    // User provided QString
    const QString* _nameStr = nullptr;
    // User provided QStringList
    const QStringList* _namesList = nullptr;
    // QStringList used as intermediate storage when user only provided a QString but requests a QStringList& output
    mutable QStringList _names;
};

/******************************************************************************
 * Represents a multi-valued identifier, e.g., "Ni" -> (4, 5, 6)
 ******************************************************************************/
struct MultiIdentifier : ASTNode {
    // Names (as strings)
    const QStringList* names;

    explicit MultiIdentifier(const QStringList* names) : ASTNode{ASTNodeType::MULTIIDENTIFIER}, names{names}
    {
        OVITO_ASSERT(names);
        OVITO_ASSERT(names->size() > 1);
    }
};

/******************************************************************************
 * Represents a unary operation, e.g. -X or +X.
 ******************************************************************************/
struct UnaryOp : ASTNode {
    // Operation ( Either + or -)
    const Op op;
    const std::unique_ptr<ASTNode> right;

    explicit UnaryOp(Op op, std::unique_ptr<ASTNode>&& right) : ASTNode{ASTNodeType::UNARYOP}, op{op}, right{std::move(right)} {}
};

/******************************************************************************
 * Represents a binary operation, e.g. (left; ==, !=, >, or >=, ...; right)
 ******************************************************************************/
struct BinaryOp : ASTNode {
    // Left branch
    const std::unique_ptr<ASTNode> left;
    // Operation
    const Op op;
    // Right branch
    const std::unique_ptr<ASTNode> right;

    explicit BinaryOp(std::unique_ptr<ASTNode>&& left, Op op, std::unique_ptr<ASTNode>&& right)
        : ASTNode{ASTNodeType::BINARYOP}, left{std::move(left)}, op{op}, right{std::move(right)}
    {
    }
};

/******************************************************************************
 * Represents a ternary expression: condition ? true_expr : false_expr
 ******************************************************************************/
struct TernaryOp : ASTNode {
    // Condition
    const std::unique_ptr<ASTNode> condition;
    // True branch
    const std::unique_ptr<ASTNode> trueExpr;
    // False branch
    const std::unique_ptr<ASTNode> falseExpr;

    TernaryOp(std::unique_ptr<ASTNode>&& condition, std::unique_ptr<ASTNode>&& trueExpr, std::unique_ptr<ASTNode>&& falseExpr)
        : ASTNode{ASTNodeType::TERNARYOP}, condition{std::move(condition)}, trueExpr{std::move(trueExpr)}, falseExpr{std::move(falseExpr)}
    {
    }
};

/******************************************************************************
 * Represents a function call name(args, ...)
 ******************************************************************************/
struct FunctionCall : ASTNode {
    // Name (Identifier)
    const std::unique_ptr<ASTNode> name;
    // Operation
    std::vector<std::unique_ptr<ASTNode>> args;

    explicit FunctionCall(std::unique_ptr<ASTNode>&& name, std::vector<std::unique_ptr<ASTNode>>&& args)
        : ASTNode{ASTNodeType::FUNC}, name{std::move(name)}, args{std::move(args)}
    {
    }
};

/******************************************************************************
 * Checks whether a branch of an AST contains nodes of the specific type
 ******************************************************************************/
[[nodiscard]] bool BranchContainsType(const ASTNode* start, ASTNodeType type)
{
    OVITO_ASSERT(start);
    if(!start) {
        return false;
    }
    switch(start->type) {
        case ASTNodeType::IDENTIFIER: {
            [[fallthrough]];
        }
        case ASTNodeType::MULTIIDENTIFIER: {
            return start->type == type;
        }
        case ASTNodeType::UNARYOP: {
            const UnaryOp* node = static_cast<const UnaryOp*>(start);
            OVITO_ASSERT(node);
            return BranchContainsType(node->right.get(), type);
        }
        case ASTNodeType::BINARYOP: {
            const BinaryOp* node = static_cast<const BinaryOp*>(start);
            OVITO_ASSERT(node);
            return BranchContainsType(node->left.get(), type) || BranchContainsType(node->right.get(), type);
        }
        case ASTNodeType::TERNARYOP: {
            const TernaryOp* node = static_cast<const TernaryOp*>(start);
            OVITO_ASSERT(node);
            return BranchContainsType(node->condition.get(), type) || BranchContainsType(node->trueExpr.get(), type) ||
                   BranchContainsType(node->falseExpr.get(), type);
        }
        case ASTNodeType::FUNC: {
            const FunctionCall* node = static_cast<const FunctionCall*>(start);
            OVITO_ASSERT(node);
            return BranchContainsType(node->name.get(), type) ||
                   std::ranges::any_of(node->args, [type](const auto& item) { return BranchContainsType(item.get(), type); });
        }
        default: {
            return false;
        }
    }
}

/******************************************************************************
 * Parse a list of tokens into an AST
 * The lifetime of tokens MUST exceed the usage of the AST as strings are not copied
 ******************************************************************************/
[[nodiscard]] std::unique_ptr<ASTNode> Parser::parse(const QStringList* tokens)
{
    _tokens = tokens;
    _index = 0;
    std::unique_ptr<ASTNode> ast = parseExpression();
    if(_index != _tokens->size()) {
        throw Exception(QStringLiteral("Extra characters after valid expression at position %1.").arg(_index));
    }
    return ast;
}

/******************************************************************************
 * Return the current token without consuming it.
 ******************************************************************************/
[[nodiscard]] const QString* Parser::peek() const
{
    if(_index < _tokens->size()) {
        return &((*_tokens)[_index]);
    }
    return nullptr;
}

/******************************************************************************
 * Consume and return the current token.
 ******************************************************************************/
[[nodiscard]] const QString* Parser::consume()
{
    const QString* token = peek();
    ++_index;
    return token;
}

/******************************************************************************
 * If the current token is in 'expected', consume and return it else return null.
 ******************************************************************************/
[[nodiscard]] std::optional<Op> Parser::match(std::initializer_list<QStringView> expected)
{
    if(const QString* token = peek()) {
        const QString& tokenValue = *token;
        for(const auto& ex : expected) {
            if(tokenValue == ex) {
                ++_index;
                return StringToOp(tokenValue);
            }
        }
    }
    return {};
}

/******************************************************************************
 * Parse the current expression.
 ******************************************************************************/
[[nodiscard]] std::unique_ptr<ASTNode> Parser::parseExpression()
{
    // Top-level parse: we parse a ternary expression (which includes lower level expressions).
    return parseTernary();
}

/******************************************************************************
 * Parse ternary expression: OrExpr ( '?' TernaryExpr ':' TernaryExpr )
 ******************************************************************************/
[[nodiscard]] std::unique_ptr<ASTNode> Parser::parseTernary()
{
    std::unique_ptr<ASTNode> condition = parseOrExpression();
    if(match({QStringLiteral("?")})) {
        std::unique_ptr<ASTNode> trueExpr = parseExpression();
        if(!match({QStringLiteral(":")})) {
            throw Exception(QStringLiteral("Missing ':' in ternary expression at position %1.").arg(_index));
        }
        std::unique_ptr<ASTNode> falseExpr = parseExpression();
        return std::make_unique<TernaryOp>(std::move(condition), std::move(trueExpr), std::move(falseExpr));
    }
    return condition;
};

/******************************************************************************
 * Parse OrExpr -> AndExpr ( '||' AndExpr )
 ******************************************************************************/
[[nodiscard]] std::unique_ptr<ASTNode> Parser::parseOrExpression()
{
    std::unique_ptr<ASTNode> left = parseAndExpression();
    while(match({QStringLiteral("||")})) {
        std::unique_ptr<ASTNode> right = parseAndExpression();
        left = std::make_unique<BinaryOp>(std::move(left), Op::OR, std::move(right));
    }
    // Return branch
    return left;
}

/******************************************************************************
 * Parse AndExpr -> Comparison ( '&&' Comparison )
 ******************************************************************************/
[[nodiscard]] std::unique_ptr<ASTNode> Parser::parseAndExpression()
{
    // AndExpr -> Comparison ( '&&' Comparison )*
    std::unique_ptr<ASTNode> left = parseComparison();
    while(match({QStringLiteral("&&")})) {
        std::unique_ptr<ASTNode> right = parseComparison();
        left = std::make_unique<BinaryOp>(std::move(left), Op::AND, std::move(right));
    }
    // Return branch
    return left;
}

/******************************************************************************
 * Parse left and right side of exactly one comparison operator
 ******************************************************************************/
[[nodiscard]] std::unique_ptr<ASTNode> Parser::parseComparison()
{
    std::unique_ptr<ASTNode> left = parseMathOperation();
    if(std::optional<Op> op = match({QStringLiteral("=="), QStringLiteral("!="), QStringLiteral(">"), QStringLiteral("<"),
                                     QStringLiteral(">="), QStringLiteral("<=")})) {
        std::unique_ptr<ASTNode> right = parseMathOperation();
        return std::make_unique<BinaryOp>(std::move(left), op.value(), std::move(right));
    }
    // No comparison operator => just primary
    return left;
}

/******************************************************************************
 * Parse left and right side of one or more chained math operations
 ******************************************************************************/
[[nodiscard]] std::unique_ptr<ASTNode> Parser::parseMathOperation()
{
    std::unique_ptr<ASTNode> left = parsePrimary();
    // match one or more math operators
    while(std::optional<Op> op =
              match({QStringLiteral("+"), QStringLiteral("-"), QStringLiteral("*"), QStringLiteral("/"), QStringLiteral("^")})) {
        std::unique_ptr<ASTNode> right = parsePrimary(left.get());
        left = std::make_unique<BinaryOp>(std::move(left), op.value(), std::move(right));
    }
    return left;
}

/******************************************************************************
 * Parse a Primary: Primary -> ( '+' | '-' ) Primary | '(' Expression ')' | Identifier | FunctionCall
 ******************************************************************************/
[[nodiscard]] std::unique_ptr<ASTNode> Parser::parsePrimary(ASTNode* left)
{
    // Consume first token
    const QString* token = consume();
    if(!token) {
        throw Exception(QStringLiteral("Unexpected end of expression."));
    }

    const QString& tokenValue = *token;

    // 1) Check for unary +/-
    if(tokenValue == QStringLiteral("-") || tokenValue == QStringLiteral("+")) {
        return std::make_unique<UnaryOp>(StringToOp(tokenValue), parseExpression());
    }

    // 2) Check for Parenthesized expressions
    if(tokenValue == QStringLiteral("(")) {
        // Parse inside expression
        std::unique_ptr<ASTNode> node = parseExpression();
        // Validate and consume ')'
        if(!peek() || *consume() != QStringLiteral(")")) {
            throw Exception(QStringLiteral("Missing closing parenthesis in expression at position %1.").arg(_index));
        }
        return node;
    }

    // 3) If the next token is '(' => function call
    if(peek() && *peek() == QStringLiteral("(")) {
        // Consume '('. Cast to void to ignore [[nodiscard]]
        (void)consume();

        // Store args
        std::vector<std::unique_ptr<ASTNode>> args;

        // Parse function arguments until ')' is found
        while(peek() && *peek() != QStringLiteral(")")) {
            // Duplicate ','
            if(!peek() || *peek() == QStringLiteral(",")) {
                throw Exception(QStringLiteral("Invalid arguments in function call: %1. Expected value, found ',' instead at position %2.")
                                    .arg(tokenValue)
                                    .arg(_index));
            }
            args.emplace_back(parseExpression());

            if(peek() && *peek() != QStringLiteral(",")) {
                break;
            }
            // Consume the ','
            (void)consume();
        }

        // Validate and consume ')'
        if(!peek() || *consume() != QStringLiteral(")")) {
            throw Exception(QStringLiteral("Missing closing parenthesis in function call: %1 at position %2.").arg(tokenValue).arg(_index));
        }

        return std::make_unique<FunctionCall>(std::make_unique<Identifier>(tokenValue), std::move(args));
    }

    // 4.0) Identifier or a known mapped token.
    if(_mapping.contains(tokenValue)) {
        // Identifiers like StructureType or ParticleType.
        return std::make_unique<Identifier>(token);
    }

    // 4.1) Disentangle identifier or multi identifier from map.
    const QStringList* match = nullptr;
    if(left && left->type == ASTNodeType::IDENTIFIER) {
        // Use information from left hand side expression
        const Identifier* leftNode = static_cast<const Identifier*>(left);
        // Search left node name in _mapping to determine inner map
        if(const auto oit = _mapping.find(leftNode->name()); oit != _mapping.end()) {
            const InnerMapType& innerMap = oit->second;
            if(const auto it = innerMap.find(tokenValue); it != innerMap.end()) {
                match = &(it->second);
            }
        }
    }
    else {
        // No information from left hand side expression
        // Search mappings in inner dicts
        // -> first match is determined to be correct
        // -> all subsequent findings need to match that first result
        for(const auto& [_, innerMap] : _mapping) {
            if(const auto it = innerMap.find(tokenValue); it != innerMap.end()) {
                if(!match) {
                    // first match
                    match = &(it->second);
                }
                else {
                    // subsequent matches
                    if(*match != it->second) {
                        throw Exception(
                            QStringLiteral("Ambiguous type name %1 at position %2. The type name maps to different numerical values in "
                                           "different properties: (%3) != (%4). Use a numerical value instead to avoid this error.")
                                .arg(tokenValue)
                                .arg(_index)
                                .arg(match->join(", "))
                                .arg(it->second.join(", ")));
                    }
                }
            }
        }
    }
    if(!match) {
        // Use inf as an invalid type-id
        if(tokenValue.startsWith("\"") && tokenValue.endsWith("\"")) {
            return std::make_unique<Identifier>(QStringLiteral("inf"));
        }
        // Otherwise just return the token
        return std::make_unique<Identifier>(tokenValue);
    }
    if(match->size() == 1) {
        return std::make_unique<Identifier>(match);
    }
    return std::make_unique<MultiIdentifier>(match);
}

/******************************************************************************
 * Convert an AST back into a string, replacing tag values with integer indices
 * as specified in '_mappings'.
 * Tokens and mapping from the ast generation MUST still be valid.
 ******************************************************************************/
[[nodiscard]] QString ASTWriter::write(const ASTNode* astNode)
{
    OVITO_ASSERT(astNode);

    switch(astNode->type) {
        case ASTNodeType::UNARYOP: {
            const UnaryOp* node = static_cast<const UnaryOp*>(astNode);
            OVITO_ASSERT(node);

            // Unary +/- only supported on identifiers
            if(BranchContainsType(node->right.get(), ASTNodeType::MULTIIDENTIFIER)) {
                throw Exception(QStringLiteral("A multi valued type name cannot be used with a unary + or - sign."));
            }
            // Assemble expression
            return QStringLiteral("%1%2").arg(OpToString(node->op)).arg(write(node->right.get()));
        }
        case ASTNodeType::IDENTIFIER: {
            const Identifier* node = static_cast<const Identifier*>(astNode);
            OVITO_ASSERT(node);

            const QString& name = node->name();

            // Figure out if it's a known tag-key (e.g. StructureType or ParticleType, ...)
            // or a tag-value (e.g. fcc, Ni, ...), or neither.
            for(const auto& [key, innerMap] : _mapping) {
                // Check tag-keys
                if(key == name) {
                    return name;
                }

                // Check values
                if(const auto it = innerMap.find(name); it != innerMap.end()) {
                    OVITO_ASSERT(it->second.size() == 1);
                    return it->second[0];
                }
            }
            // Identifier not found in mapping; return as is
            return name;
        }
        case ASTNodeType::MULTIIDENTIFIER: {
            throw Exception(QStringLiteral("A multi valued type name cannot be used on the left hand side of an expression."));
        }
        case ASTNodeType::BINARYOP: {
            const BinaryOp* node = static_cast<const BinaryOp*>(astNode);
            OVITO_ASSERT(node);

            if(!node->left || !node->right) {
                throw Exception(QStringLiteral("Malformed or empty expression in 'BinaryOp' is empty!"));
            }

            // Distribute / expand ternary
            if(node->right->type == ASTNodeType::TERNARYOP) {
                const TernaryOp* rightNode = static_cast<const TernaryOp*>(node->right.get());
                return handleBinaryWithTernary(write(node->left.get()), rightNode);
            }
            if(node->left->type == ASTNodeType::TERNARYOP) {
                throw Exception(QStringList("Ternary expression on the left is not supported."));
            }
            if(node->left->type == ASTNodeType::MULTIIDENTIFIER) {
                throw Exception(QStringList("A multi valued type name cannot be used on the left hand side of an expression."));
            }

            QString leftStr = write(node->left.get());
            // If left is a known tag key and right is a multi-value (like "(4 || 5 || 6)"),
            // we expand the condition.  E.g. "ParticleType == (4 || 5 || 6)" ->
            // "(ParticleType == 4 || ParticleType == 5 || ParticleType == 6)"
            // Similarly for '!=', '>', ... => "&&".

            if(node->right->type == ASTNodeType::MULTIIDENTIFIER) {
                const MultiIdentifier* rightNode = static_cast<const MultiIdentifier*>(node->right.get());
                OVITO_ASSERT(rightNode->names);
                const QStringList& vals = *(rightNode->names);

                // Collect expressions
                _scratch.clear();
                for(const auto& v : vals) {
                    _scratch << QStringLiteral("(%1%2%3)").arg(leftStr).arg(OpToString(node->op)).arg(v);
                }
                // Expand differently depending on the operator
                switch(node->op) {
                    case Op::EQ: {
                        // E.g. "(ParticleType == 4 || ParticleType == 5 || ParticleType == 6)"
                        return QStringLiteral("(%1)").arg(_scratch.join("||"));
                    }
                    case Op::NEQ:
                        // E.g. ParticleType != (4 || 5 || 6) => "(ParticleType != 4 && ParticleType != 5 && ParticleType != 6)"
                        // or ParticleType > (4 || 5 || 6) => "(ParticleType > 4 && ParticleType > 5 && ParticleType > 6)"
                        return QStringLiteral("(%1)").arg(_scratch.join("&&"));
                    default: {
                        throw Exception(
                            QStringLiteral("A non-unique type mapping cannot be used with a %1 operator.").arg(OpToString(node->op)));
                    }
                }
            }
            else {
                // Generate expressions
                QString rightStr = write(node->right.get());
                return QStringLiteral("%1%2%3").arg(leftStr).arg(OpToString(node->op)).arg(rightStr);
            }
            // There should always be an early return or throw
            OVITO_ASSERT(false);
            return "";
        }
        case ASTNodeType::TERNARYOP: {
            // Recurse each branch
            const TernaryOp* node = static_cast<const TernaryOp*>(astNode);
            OVITO_ASSERT(node);

            if(!node->condition || !node->trueExpr || !node->falseExpr) {
                throw Exception(QStringLiteral("Malformed or empty expression in 'TernaryOp'!"));
            }

            // Generate expressions
            QString conditionStr = write(node->condition.get());
            QString trueStr = write(node->trueExpr.get());
            QString falseStr = write(node->falseExpr.get());
            return QStringLiteral("(%1?%2:%3)").arg(conditionStr).arg(trueStr).arg(falseStr);
        }
        case ASTNodeType::FUNC: {
            const FunctionCall* node = static_cast<const FunctionCall*>(astNode);
            OVITO_ASSERT(node);
            if(BranchContainsType(node, ASTNodeType::MULTIIDENTIFIER)) {
                throw Exception(QStringLiteral("A non-unique type mapping cannot be in a %1 function call.").arg(write(node->name.get())));
            }
            QString funcName = write(node->name.get());
            _scratch.clear();

            // Handle bonds function
            if(funcName == "Bond") {
                // Input validation
                if(node->args.size() != 3) {
                    throw Exception(
                        QStringLiteral("The Bond() function requires exactly 3 arguments. Found %1 arguments.").arg(node->args.size()));
                }
                if(node->args[0] && node->args[0]->type != ASTNodeType::IDENTIFIER) {
                    throw Exception(QStringLiteral("The first argument of the Bond() function needs to be a particle property."));
                }
                if(!node->args[1] ||
                   (node->args[1]->type != ASTNodeType::IDENTIFIER && node->args[1]->type != ASTNodeType::MULTIIDENTIFIER)) {
                    throw Exception(QStringLiteral("The second argument of the Bond() function needs to be an Identifier."));
                }
                if(!node->args[2] ||
                   (node->args[2]->type != ASTNodeType::IDENTIFIER && node->args[2]->type != ASTNodeType::MULTIIDENTIFIER)) {
                    throw Exception(QStringLiteral("The third argument of the Bond() function needs to be an Identifier."));
                }

                // Get left and right expressions
                const QString& prop = static_cast<const Identifier*>(node->args[0].get())->name();
                const QStringList* leftList = (node->args[1]->type == ASTNodeType::IDENTIFIER)
                                                  ? &(static_cast<const Identifier*>(node->args[1].get())->namesList())
                                                  : static_cast<const MultiIdentifier*>(node->args[1].get())->names;
                OVITO_ASSERT(leftList);
                const QStringList* rightList = (node->args[2]->type == ASTNodeType::IDENTIFIER)
                                                   ? &(static_cast<const Identifier*>(node->args[2].get())->namesList())
                                                   : static_cast<const MultiIdentifier*>(node->args[2].get())->names;
                OVITO_ASSERT(rightList);
                // Write bond selection expression
                for(const auto& left : *leftList) {
                    for(const auto& right : *rightList) {
                        _scratch << QStringLiteral("@1.%1==%2&&@2.%1==%3").arg(prop).arg(left).arg(right);
                        _scratch << QStringLiteral("@1.%1==%2&&@2.%1==%3").arg(prop).arg(right).arg(left);
                    }
                }
                return QStringLiteral("(%1)").arg(_scratch.join("||"));
            }

            // Handle generic functions
            for(const auto& arg : node->args) {
                _scratch << write(arg.get());
            }
            return QStringLiteral("%1(%2)").arg(funcName).arg(_scratch.join(","));
        }
        default: {
            OVITO_ASSERT(false);
            return "";
        }
    }
}

/*
 * Convert an identifier to its string list representation
 * Return the internal list of identifiers from an Identifier or MultiIdentifier.
 * For all other types nullptr is returned
 */
[[nodiscard]] const QStringList* ASTWriter::expressionToValuesList(const ASTNode* astNode)
{
    OVITO_ASSERT(astNode);
    if(astNode->type == ASTNodeType::MULTIIDENTIFIER) {
        const MultiIdentifier* node = static_cast<const MultiIdentifier*>(astNode);
        OVITO_ASSERT(node->names);
        return (node->names);
    }
    else if(astNode->type == ASTNodeType::IDENTIFIER) {
        const Identifier* node = static_cast<const Identifier*>(astNode);
        return &(node->namesList());
    }
    else {
        return nullptr;
    }
}

/*
 * Distribute potentially multi valued ternary
 */
[[nodiscard]] QString ASTWriter::handleBinaryWithTernary(const QString& leftString, const ASTNode* rightNode)
{
    OVITO_ASSERT(rightNode->type == ASTNodeType::TERNARYOP);
    const TernaryOp* node = static_cast<const TernaryOp*>(rightNode);

    // Recursively process the conditionString
    QString conditionStr = write(node->condition.get());

    // Convert true and false expressions to QStringlist
    // Either read QStringList from Identifier and MultiIdentifier nodes
    // or create a new QStringList containing all other possible expressions
    QStringList trueExprList;
    const QStringList* trueExpr = expressionToValuesList(node->trueExpr.get());
    if(!trueExpr) {
        trueExprList << write(node->trueExpr.get());
        trueExpr = &trueExprList;
    }
    QStringList falseExprList;
    const QStringList* falseExpr = expressionToValuesList(node->falseExpr.get());
    if(!falseExpr) {
        falseExprList << write(node->falseExpr.get());
        falseExpr = &falseExprList;
    }

    // Generate expression
    _scratch.clear();
    for(const auto& t : *trueExpr) {
        for(const auto& f : *falseExpr) {
            // (left_str == (condition_str ? t : f))
            _scratch << QStringLiteral("(%1==(%2?%3:%4))").arg(leftString).arg(conditionStr).arg(t).arg(f);
        }
    }
    // join them with OR
    return QStringLiteral("(%1)").arg(_scratch.join("||"));
}

#ifdef OVITO_DEBUG
/*
 * Debug print for Identifier.
 */
QDebug operator<<(QDebug dbg, const Identifier& i) { return dbg.nospace() << "Identifier(" << i.name() << ")"; }
QDebug operator<<(QDebug dbg, const Identifier* i) { return dbg.nospace() << *i; }

/*
 * Debug print for MultiIdentifier.
 */
QDebug operator<<(QDebug dbg, const MultiIdentifier& m) { return dbg.nospace() << "MultiIdentifier(" << *(m.names) << ")"; }
QDebug operator<<(QDebug dbg, const MultiIdentifier* m) { return dbg.nospace() << *m; }

/*
 * Debug print for BinaryOp.
 */
QDebug operator<<(QDebug dbg, const BinaryOp& b)
{
    return dbg.nospace() << "BinaryOp(" << b.left.get() << OpToString(b.op) << b.right.get() << ")";
}
QDebug operator<<(QDebug dbg, const BinaryOp* b) { return dbg.nospace() << *b; }

/*
 * Debug print for TernaryOp.
 */
QDebug operator<<(QDebug dbg, const TernaryOp& t)
{
    return dbg.nospace() << "TernaryOp(" << t.condition.get() << " ? " << t.trueExpr.get() << " : " << t.falseExpr.get() << ")";
}
QDebug operator<<(QDebug dbg, const TernaryOp* t) { return dbg.nospace() << *t; }

/*
 * Debug print for UnaryOp.
 */
QDebug operator<<(QDebug dbg, const UnaryOp& u) { return dbg.nospace() << "UnaryOp(" << OpToString(u.op) << u.right.get() << ")"; }
QDebug operator<<(QDebug dbg, const UnaryOp* u) { return dbg.nospace() << *u; }

/*
 * Debug print for FunctionCall.
 */
QDebug operator<<(QDebug dbg, const FunctionCall& f)
{
    dbg.nospace() << "FunctionCall(" << f.name.get();
    for(const auto& a : f.args) {
        dbg.nospace() << a.get() << ",";
    }
    return dbg.nospace() << ")";
}
QDebug operator<<(QDebug dbg, const FunctionCall* f) { return dbg.nospace() << *f; }

/*
 * Debug print for ASTNode.
 */
QDebug operator<<(QDebug dbg, const ASTNode* ast)
{
    if(ast) {
        switch(ast->type) {
            case ASTNodeType::IDENTIFIER: dbg.nospace() << *static_cast<const Identifier*>(ast); break;
            case ASTNodeType::MULTIIDENTIFIER: dbg.nospace() << *static_cast<const MultiIdentifier*>(ast); break;
            case ASTNodeType::UNARYOP: dbg.nospace() << *static_cast<const UnaryOp*>(ast); break;
            case ASTNodeType::FUNC: dbg.nospace() << *static_cast<const FunctionCall*>(ast); break;
            case ASTNodeType::BINARYOP: dbg.nospace() << *static_cast<const BinaryOp*>(ast); break;
            case ASTNodeType::TERNARYOP: dbg.nospace() << *static_cast<const TernaryOp*>(ast); break;
            default: dbg.nospace() << "UNKNOWN TYPE!"; break;
        }
    }
    return dbg.nospace();
}
QDebug operator<<(QDebug dbg, const ASTNode& ast) { return dbg.nospace() << &ast; }

void ASTNode::debugPrint() const { qDebug() << this; }
#endif

}  // namespace Ovito::PropertyExpressionRewriter