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

namespace Ovito::PropertyExpressionRewriter {

/******************************************************************************
 * Tokenize an expression
 ******************************************************************************/
[[nodiscard]] QStringList tokenizeExpression(QString expression)
{
    // Regularize " and ' characters
    expression.replace('\'', '\"');

    const static QRegularExpression regex(QRegularExpression(QStringLiteral(R"((==|!=|>=|<=|\?|:|\(|\)|&&|\|\||[\w\._'\"]+|\S))")));
    OVITO_ASSERT(regex.isValid());

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
        default: OVITO_ASSERT(false);
    }
    return QStringLiteral("");
}

/******************************************************************************
 * Convert a string to operation (Op) enum.
 ******************************************************************************/
inline Op StringToOp(const QString& str)
{
    if(str == "&&")
        return Op::AND;
    else if(str == "||")
        return Op::OR;
    else if(str == "==")
        return Op::EQ;
    else if(str == "!=")
        return Op::NEQ;
    else if(str == ">")
        return Op::GE;
    else if(str == ">=")
        return Op::GEQ;
    else if(str == "<")
        return Op::LE;
    else if(str == "<=")
        return Op::LEQ;
    else if(str == "?")
        return Op::QM;
    else if(str == ":")
        return Op::COL;
    else
        OVITO_ASSERT(false);
}

/*
 * Represents an identifier, e.g. 'ParticleType', 'fcc', 1, ...
 * Can be created from a QString or a QStringlist of length 1
 * Note: no data is guaranteed to be stored -> original source must be kept alive
 */
struct Identifier : ASTNode {
public:
    explicit Identifier(const QString* name) : ASTNode{ASTNodeType::IDENTIFIER}, _nameStr{name}, _namesList{nullptr} { OVITO_ASSERT(name); }

    explicit Identifier(const QStringList* names) : ASTNode{ASTNodeType::IDENTIFIER}, _nameStr{nullptr}, _namesList{names}
    {
        OVITO_ASSERT(names);
        OVITO_ASSERT(names->size() == 1);
    }

    explicit Identifier(const QString& string) : ASTNode{ASTNodeType::IDENTIFIER}, _nameStr{nullptr}, _namesList{&_names}, _names{string} {}

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
    const QString* _nameStr;
    // User provided QStringList
    const QStringList* _namesList;
    // QStringList used as intermediate storage when user only provided a QString but requests a QStringList& output
    mutable QStringList _names;
};

/*
 * Represents a multi-valued identifier, e.g., "Ni" -> (4, 5, 6)
 */
struct MultiIdentifier : ASTNode {
    // Names (as strings)
    const QStringList* names;

    explicit MultiIdentifier(const QStringList* names) : ASTNode{ASTNodeType::MULTIIDENTIFIER}, names{names}
    {
        OVITO_ASSERT(names);
        OVITO_ASSERT(names->size() > 1);
    }
};

/*
 * Represents a binary operation, e.g. (left; ==, !=, >, or >=, ...; right)
 */
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

/*
 * Represents a ternary expression: condition ? true_expr : false_expr
 */
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
 * Parse a list of tokens into an AST
 * The lifetime of tokens MUST exceed the usage of the AST as strings are not copied
 ******************************************************************************/
[[nodiscard]] std::unique_ptr<ASTNode> Parser::parse(const QStringList* tokens)
{
    _tokens = tokens;
    _index = 0;
    std::unique_ptr<ASTNode> ast = parseExpression();
    if(_index != _tokens->size()) {
        throw Exception(QStringLiteral("Extra characters after valid expression."));
    }
    return ast;
}

/******************************************************************************
 *  Return the current token without consuming it.
 ******************************************************************************/
[[nodiscard]] const QString* Parser::peek() const
{
    if(_index < _tokens->size()) {
        return &((*_tokens)[_index]);
    }
    return nullptr;
}

/******************************************************************************
 *  Consume and return the current token.
 ******************************************************************************/
[[nodiscard]] const QString* Parser::consume()
{
    const QString* token = peek();
    ++_index;
    return token;
}

/******************************************************************************
 *  If the current token is in 'expected', consume and return it else return null.
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
            throw Exception(QStringLiteral("Missing ':' in ternary expression."));
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
    std::unique_ptr<ASTNode> left = parsePrimary();

    if(std::optional<Op> op = match({QStringLiteral("=="), QStringLiteral("!="), QStringLiteral(">"), QStringLiteral("<"),
                                     QStringLiteral(">="), QStringLiteral("<=")})) {
        std::unique_ptr<ASTNode> right = parsePrimary();
        return std::make_unique<BinaryOp>(std::move(left), op.value(), std::move(right));
    }
    // No comparison operator => just primary
    return left;
}

/******************************************************************************
 * Parse a Primary: Identifier or '(' Expression ')'
 ******************************************************************************/
[[nodiscard]] std::unique_ptr<ASTNode> Parser::parsePrimary()
{
    // Consume first token
    const QString* token = consume();
    if(!token) {
        throw Exception(QStringLiteral("Unexpected end of expression."));
    }

    const QString& tokenValue = *token;
    if(tokenValue == "(") {
        // Parse inside expression
        std::unique_ptr<ASTNode> node = parseExpression();
        // Validate and consume ')'
        if(!peek() || *consume() != QStringLiteral(")")) {
            throw Exception(QStringLiteral("Missing closing parenthesis in expression."));
        }
        return node;
    }
    else {
        if(_mapping.contains(tokenValue)) {
            // Identifiers like StructureType or ParticleType
            return std::make_unique<Identifier>(token);
        }
        else {
            // Search mappings in inner dicts
            // -> first match is determined to be correct
            // -> all subsequent findings need to match that first result
            const QStringList* match = nullptr;
            for(const auto& [_, innerMap] : _mapping) {
                if(const auto it = innerMap.find(tokenValue); it != innerMap.end()) {
                    if(!match) {
                        // first match
                        match = &(it->second);
                    }
                    else {
                        // subsequent matches
                        if(*match != it->second) {
                            throw Exception(QStringLiteral("Mismatch in type mapping for %1: (%2) != (%3)")
                                                .arg(tokenValue)
                                                .arg(match->join(", "))
                                                .arg(it->second.join(", ")));
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
    }
    OVITO_ASSERT(false);
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
        case ASTNodeType::IDENTIFIER: {
            const Identifier* node = static_cast<const Identifier*>(astNode);

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
            // This will now be directly handled in expressionToValuesList()
            OVITO_ASSERT(false);
            //  const MultiIdentifier* node = static_cast<const MultiIdentifier*>(astNode);
            // if(!node->names) {
            // throw Exception(QStringLiteral("Empty 'MultiIdentifier' names!"));
            // }
            // return QString("(%1)").arg(node->names->join("||"));
        }
        case ASTNodeType::BINARYOP: {
            const BinaryOp* node = static_cast<const BinaryOp*>(astNode);

            if(!node->left || !node->right) {
                throw Exception(QStringLiteral("Malformed or empty expression in 'BinaryOp' is empty!"));
            }

            // Distribute / expand ternary
            if(node->right->type == ASTNodeType::TERNARYOP) {
                const TernaryOp* rightNode = static_cast<const TernaryOp*>(node->right.get());
                return handleBinaryWithTernary(write(node->left.get()), rightNode);
            }
            else if(node->left->type == ASTNodeType::TERNARYOP) {
                throw Exception(QStringList("Ternary expression on the left is not supported."));
            }

            QString leftStr = write(node->left.get());
            // If left is a known tag key and right is a multi-value (like "(4 || 5 || 6)"),
            // we expand the condition.  E.g. "ParticleType == (4 || 5 || 6)" ->
            // "(ParticleType == 4 || ParticleType == 5 || ParticleType == 6)"
            // Similarly for '!=', '>', ... => "&&".
            if(node->left->type == ASTNodeType::MULTIIDENTIFIER) {
                throw Exception(QStringList("Multi-Identifier on the left is not supported."));
            }

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
                    case Op::NEQ: [[fallthrough]];
                    case Op::GE: [[fallthrough]];
                    case Op::GEQ: [[fallthrough]];
                    case Op::LE: [[fallthrough]];
                    case Op::LEQ: {
                        // E.g. ParticleType != (4 || 5 || 6) => "(ParticleType != 4 && ParticleType != 5 && ParticleType != 6)"
                        // or ParticleType > (4 || 5 || 6) => "(ParticleType > 4 && ParticleType > 5 && ParticleType > 6)"
                        return QStringLiteral("(%1)").arg(_scratch.join("&&"));
                    }
                    default: {
                        // unreachable
                        OVITO_ASSERT(false);
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
        }
        case ASTNodeType::TERNARYOP: {
            // Recurse each branch
            const TernaryOp* node = static_cast<const TernaryOp*>(astNode);

            if(!node->condition || !node->trueExpr || !node->falseExpr) {
                throw Exception(QStringLiteral("Malformed or empty expression in 'TernaryOp'!"));
            }

            // Generate expressions
            QString conditionStr = write(node->condition.get());
            QString trueStr = write(node->trueExpr.get());
            QString falseStr = write(node->falseExpr.get());
            return QStringLiteral("(%1?%2:%3)").arg(conditionStr).arg(trueStr).arg(falseStr);
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
QDebug operator<<(QDebug dbg, const Identifier& i)
{
    dbg.nospace() << "Identifier(" << i.name() << ")";
    return dbg.nospace();
}

/*
 * Debug print for MultiIdentifier.
 */
QDebug operator<<(QDebug dbg, const MultiIdentifier& m)
{
    dbg.nospace() << "MultiIdentifier(";
    if(m.names) {
        dbg.nospace() << *(m.names);
    }
    else {
        dbg.nospace() << m.names;
    }
    dbg.nospace() << ")";
    return dbg.nospace();
}

/*
 * Debug print for TernaryOp.
 */
QDebug operator<<(QDebug dbg, const TernaryOp& b);

/*
 * Debug print for BinaryOp.
 */
QDebug operator<<(QDebug dbg, const BinaryOp& b)
{
    dbg.nospace() << "BinaryOp(";
    if(b.left) {
        switch(b.left->type) {
            case ASTNodeType::IDENTIFIER: dbg.nospace() << *static_cast<Identifier*>(b.left.get()); break;
            case ASTNodeType::MULTIIDENTIFIER: dbg.nospace() << *static_cast<MultiIdentifier*>(b.left.get()); break;
            case ASTNodeType::BINARYOP: dbg.nospace() << *static_cast<BinaryOp*>(b.left.get()); break;
            case ASTNodeType::TERNARYOP: dbg.nospace() << *static_cast<TernaryOp*>(b.left.get()); break;
            default: dbg.nospace() << "UNKNOWN LEFT!"; break;
        }
    }
    else {
        dbg.nospace() << "NO LEFT!";
    }
    dbg.nospace() << OpToString(b.op);

    if(b.right) {
        switch(b.right->type) {
            case ASTNodeType::IDENTIFIER: dbg.nospace() << *static_cast<Identifier*>(b.right.get()); break;
            case ASTNodeType::MULTIIDENTIFIER: dbg.nospace() << *static_cast<MultiIdentifier*>(b.right.get()); break;
            case ASTNodeType::BINARYOP: dbg.nospace() << *static_cast<BinaryOp*>(b.right.get()); break;
            case ASTNodeType::TERNARYOP: dbg.nospace() << *static_cast<TernaryOp*>(b.right.get()); break;
            default: dbg.nospace() << "UNKNOWN RIGHT!"; break;
        }
    }
    else {
        dbg.nospace() << "NO RIGHT!";
    }
    dbg.nospace() << ")";
    return dbg.nospace();
}

/*
 * Debug print for TernaryOp.
 */
QDebug operator<<(QDebug dbg, const TernaryOp& t)
{
    dbg.nospace() << "TernaryOp(" << "condition=";
    if(t.condition) {
        switch(t.condition->type) {
            case ASTNodeType::IDENTIFIER: dbg.nospace() << *static_cast<Identifier*>(t.condition.get()); break;
            case ASTNodeType::MULTIIDENTIFIER: dbg.nospace() << *static_cast<MultiIdentifier*>(t.condition.get()); break;
            case ASTNodeType::BINARYOP: dbg.nospace() << *static_cast<BinaryOp*>(t.condition.get()); break;
            case ASTNodeType::TERNARYOP: dbg.nospace() << *static_cast<TernaryOp*>(t.condition.get()); break;
            default: dbg.nospace() << "UNKNOWN CONDITION!"; break;
        }
    }
    else {
        dbg.nospace() << "NO CONDITION!";
    }
    dbg.nospace() << "true=";
    if(t.trueExpr) {
        switch(t.trueExpr->type) {
            case ASTNodeType::IDENTIFIER: dbg.nospace() << *static_cast<Identifier*>(t.trueExpr.get()); break;
            case ASTNodeType::MULTIIDENTIFIER: dbg.nospace() << *static_cast<MultiIdentifier*>(t.trueExpr.get()); break;
            case ASTNodeType::BINARYOP: dbg.nospace() << *static_cast<BinaryOp*>(t.trueExpr.get()); break;
            case ASTNodeType::TERNARYOP: dbg.nospace() << *static_cast<TernaryOp*>(t.trueExpr.get()); break;
            default: dbg.nospace() << "UNKNOWN TRUE EXPRESSION!"; break;
        }
    }
    else {
        dbg.nospace() << "NO TRUE EXPRESSION!";
    }
    dbg.nospace() << "false=";
    if(t.falseExpr) {
        switch(t.falseExpr->type) {
            case ASTNodeType::IDENTIFIER: dbg.nospace() << *static_cast<Identifier*>(t.falseExpr.get()); break;
            case ASTNodeType::MULTIIDENTIFIER: dbg.nospace() << *static_cast<MultiIdentifier*>(t.falseExpr.get()); break;
            case ASTNodeType::BINARYOP: dbg.nospace() << *static_cast<BinaryOp*>(t.falseExpr.get()); break;
            case ASTNodeType::TERNARYOP: dbg.nospace() << *static_cast<TernaryOp*>(t.falseExpr.get()); break;
            default: dbg.nospace() << "UNKNOWN FALSE EXPRESSION!"; break;
        }
    }
    else {
        dbg.nospace() << "NO FALSE EXPRESSION!";
    }
    dbg.nospace() << ")";
    return dbg.nospace();
}

/*
 * Debug print for ASTNode.
 */
QDebug operator<<(QDebug dbg, ASTNode* ast)
{
    if(ast) {
        switch(ast->type) {
            case ASTNodeType::IDENTIFIER: dbg.nospace() << *static_cast<Identifier*>(ast); break;
            case ASTNodeType::MULTIIDENTIFIER: dbg.nospace() << *static_cast<MultiIdentifier*>(ast); break;
            case ASTNodeType::BINARYOP: dbg.nospace() << *static_cast<BinaryOp*>(ast); break;
            case ASTNodeType::TERNARYOP: dbg.nospace() << *static_cast<TernaryOp*>(ast); break;
            default: dbg.nospace() << "UNKNOWN TYPE!"; break;
        }
    }
    return dbg.nospace();
}

#endif

}  // namespace Ovito::PropertyExpressionRewriter