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

#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include "PropertyExpressionEvaluator.h"

namespace Ovito {

/// List of characters allowed in variable names.
/// Note: Keep this list in sync with the regex used in PropertyExpressionRewriter::tokenizeExpression().
mu::string_type PropertyExpressionEvaluator::_validVariableNameChars(_T("0123456789_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.@"));

/******************************************************************************
* Specifies the expressions to be evaluated for each data element and create the
* list of input variables.
******************************************************************************/
void PropertyExpressionEvaluator::initialize(const QStringList& expressions, const PipelineFlowState& state, const ConstDataObjectPath& containerPath, int animationFrame)
{
    // Create the input variables.
    initializeInputs(state, containerPath, animationFrame);

    // Expression parser and rewriter
    std::optional<PropertyExpressionRewriter::Parser> parser;
    std::optional<PropertyExpressionRewriter::ASTWriter> rewriter;

    // Process expressions being passed in.
    _expressions.clear();
    _expressions.reserve(expressions.size());
    for(const auto& expr : expressions) {
        // Check if expression contains a reference to a named type.
        if(PropertyExpressionRewriter::expressionNeedsRewrite(expr)) {
            // Rewrite expressions if required.
            if(!parser) {
                parser.emplace(_typeMapping);
                rewriter.emplace(_typeMapping);
            }
            const QStringList& tokens = PropertyExpressionRewriter::tokenizeExpression(expr);
            std::unique_ptr<PropertyExpressionRewriter::ASTNode> ast = parser->parse(&expr, &tokens);
            // qDebug() << rewriter->write(ast.get());
            _expressions.push_back(convertQString(rewriter->write(ast.get())));
        }
        else {
            // Use original expression as is.
            _expressions.push_back(convertQString(expr));
        }
    }
}

/******************************************************************************
 * Creates the input variables.
 ******************************************************************************/
void PropertyExpressionEvaluator::initializeInputs(const PipelineFlowState& state, const ConstDataObjectPath& containerPath,
                                                   int animationFrame)
{
    const PropertyContainer* container = static_object_cast<PropertyContainer>(containerPath.back());

    // Build list of properties that will be made available as expression variables.
    std::vector<ConstPropertyPtr> inputProperties;
    for(const Property* property : container->properties()) inputProperties.push_back(property);
    _elementDescriptionName = container->getOOMetaClass().elementDescriptionName();

    // Get simulation cell information.
    const SimulationCell* simCell = state.getObject<SimulationCell>();

    // Determine number of input elements.
    _elementCount = container->elementCount();
    _referencedVariablesKnown = false;
    OVITO_ASSERT(inputProperties.empty() || _elementCount == inputProperties.front()->size());

    // Create list of input variables.
    createInputVariables(inputProperties, simCell, state.buildAttributesMap(), animationFrame);
}

/******************************************************************************
* Initializes the list of input variables from the given input state.
******************************************************************************/
void PropertyExpressionEvaluator::createInputVariables(const std::vector<ConstPropertyPtr>& inputProperties, const SimulationCell* simCell, const QVariantMap& attributes, int animationFrame)
{
    // Register the list of expression variables that refer to input properties.
    registerPropertyVariables(inputProperties, 0);

    // Create index variable.
    if(!_indexVarName.isEmpty())
        registerIndexVariable(_indexVarName, 0, tr("zero-based"));

    // Create constant variables.
    ExpressionVariable constVar;

    // Number of elements
    registerGlobalParameter("N", elementCount(), tr("total number of %1").arg(_elementDescriptionName.isEmpty() ? tr("elements") : _elementDescriptionName));

    // Animation frame
    registerGlobalParameter("Frame", animationFrame, tr("animation frame number"));

    // Global attributes
    for(auto entry = attributes.constBegin(); entry != attributes.constEnd(); ++entry) {
        if(entry.value().canConvert<double>())
            registerGlobalParameter(entry.key(), entry.value().toDouble());
        else if(entry.value().canConvert<long>())
            registerGlobalParameter(entry.key(), entry.value().value<long>());
    }

    if(simCell) {
        // Store simulation cell data.
        _simCell = simCell;

        // Cell volume
        registerGlobalParameter("CellVolume", _simCell.is2D() ? _simCell.volume2D() : _simCell.volume3D(), tr("simulation cell volume"));

        // Cell size
        registerGlobalParameter("CellSize.X", std::abs(_simCell.cellMatrix().column(0).x()), tr("size along X"));
        registerGlobalParameter("CellSize.Y", std::abs(_simCell.cellMatrix().column(1).y()), tr("size along Y"));
        registerGlobalParameter("CellSize.Z", std::abs(_simCell.cellMatrix().column(2).z()), tr("size along Z"));
    }

    // Constant pi
    registerConstant("pi", M_PI, QStringLiteral("%1...").arg(M_PI));

    // Constant infinity
    if(std::numeric_limits<FloatType>::has_infinity) {
        registerConstant("inf", std::numeric_limits<FloatType>::infinity(), QStringLiteral("∞"));
    }
}

/******************************************************************************
* Registers the list of expression variables that refer to input properties.
******************************************************************************/
void PropertyExpressionEvaluator::registerPropertyVariables(const std::vector<ConstPropertyPtr>& inputProperties, int variableClass, const mu::char_type* namePrefix)
{
    int propertyIndex = 1;
    for(const ConstPropertyPtr& property : inputProperties) {
        ExpressionVariable v;

        // Properties with custom data type are not supported by this modifier.
        if(property->dataType() == Property::Int8)
            v.type = INT8_PROPERTY;
        else if(property->dataType() == Property::Int32)
            v.type = INT32_PROPERTY;
        else if(property->dataType() == Property::Int64)
            v.type = INT64_PROPERTY;
        else if(property->dataType() == Property::Float32)
            v.type = FLOAT32_PROPERTY;
        else if(property->dataType() == Property::Float64)
            v.type = FLOAT64_PROPERTY;
        else
            continue;
        v.variableClass = variableClass;
        v.propertyRef = property;
        v.propertyAccess = property.get();

        // Derive a valid variable name from the property name by removing all invalid characters.
        QString propertyName = property->name();
        // If the name is empty, generate one.
        if(propertyName.isEmpty())
            propertyName = QStringLiteral("Property%1").arg(propertyIndex);
        // If the name starts with a number, prepend an underscore.
        else if(propertyName[0].isDigit())
            propertyName.prepend(QChar('_'));

        for(size_t k = 0; k < property->componentCount(); k++) {

            QString fullPropertyName = propertyName;
            if(property->componentNames().size() == property->componentCount())
                fullPropertyName += QStringLiteral(".") + property->componentNames()[k];
            if(!namePrefix)
                v.name = convertQString(fullPropertyName);
            else
                v.name = namePrefix + convertQString(fullPropertyName);

            // Initialize data pointer into property storage.
            v.dataPointer = v.propertyAccess.cdata(k);
            v.stride = v.propertyAccess.stride();

            // Register variable.
            addVariable(v);

            // Register typed property to map
            if(property->isTypedProperty()) {
                addTypedPropertyToMap(convertMuString(_variables.back().mangledName), property);
            }
        }

        propertyIndex++;
    }
}

/******************************************************************************
* Registers an input variable if the name does not exist yet.
******************************************************************************/
size_t PropertyExpressionEvaluator::addVariable(ExpressionVariable v)
{
    // Replace invalid characters in variable name with an underscore.
    v.mangledName.clear();
    v.mangledName.reserve(v.name.size());
    for(char c : v.name) {
        // Remove whitespace from variable names.
        if(c <= ' ') continue;
        // Replace other invalid characters in variable names with an underscore.
        v.mangledName.push_back(_validVariableNameChars.find(c) != mu::string_type::npos ? c : '_');
    }
    if(!v.mangledName.empty()) {
        // Prepend '_' if name starts with number.
        if(v.mangledName[0] >= '0' && v.mangledName[0] <= '9')
            v.mangledName.insert(v.mangledName.begin(), '_');
        // Check if mangled name is unique.
        if(std::none_of(_variables.begin(), _variables.end(), [&v](const ExpressionVariable& v2) -> bool { return v2.mangledName == v.mangledName; })) {
            v.isRegistered = true;
        }
    }
    _referencedVariablesKnown = false;
    _variables.push_back(std::move(v));
    return _variables.size() - 1;
}

/******************************************************************************
 * Registers a typed property with its mangled name and adds it to _typeMapping
 ******************************************************************************/
void PropertyExpressionEvaluator::addTypedPropertyToMap(const QString& mangledPropertyName, const ConstPropertyPtr& property)
{
    // Access or insert mangledName in _typeMapping (outer map)
    auto& omap = _typeMapping[mangledPropertyName];

    // Register names and numeric ids of all types defined for the property.
    // Type names get placed in double quotes to make them compatible with the format used in input expressions.
    // Numeric ids are converted to strings to make them compatible with the format used in output expressions.
    for(const ElementType* type : property->elementTypes()) {
        QString typeIdStr = QString::number(type->numericId());
        QString typeName = QStringLiteral("\"%1\"").arg(type->nameOrNumericId());

        // Access or insert typeName in omap.
        auto& imap = omap[typeName];

        // Append to QStringList
        if(!imap.contains(typeIdStr))
            imap << typeIdStr;
    }
}

/******************************************************************************
* Returns the list of available input variables.
******************************************************************************/
QStringList PropertyExpressionEvaluator::inputVariableNames() const
{
    QStringList vlist;
    for(const ExpressionVariable& v : _variables) {
        if(v.isRegistered)
            vlist << convertMuString(v.mangledName);
    }
    for(const auto& [_, ovalue] : _typeMapping) {
        for(const auto& [ikey, _] : ovalue) {
            if(!vlist.contains(ikey)) vlist << ikey;
        }
    }
    return vlist;
}

/******************************************************************************
* Returns whether a variable is being referenced in one of the expressions.
******************************************************************************/
bool PropertyExpressionEvaluator::isVariableUsed(const mu::char_type* varName)
{
    if(!_referencedVariablesKnown) {
        Worker worker(*this);
        _variables = worker._variables;
        _referencedVariablesKnown = true;
    }
    for(const ExpressionVariable& var : _variables) {
        if(var.name == varName && var.isReferenced)
            return true;
    }
    return false;
}

/******************************************************************************
* Initializes the parser objects of this thread.
******************************************************************************/
PropertyExpressionEvaluator::Worker::Worker(PropertyExpressionEvaluator& evaluator) : _evaluator(evaluator)
{
    _parsers.resize(evaluator._expressions.size());

    // Make a per-thread copy of the input variables.
    _variables = evaluator._variables;

    // Reset variable flags.
    for(ExpressionVariable& v : _variables)
        v.isReferenced = false;

    auto parser = _parsers.begin();
    auto expr = evaluator._expressions.cbegin();
    for(size_t i = 0; i < evaluator._expressions.size(); i++, ++parser, ++expr) {

        if(expr->empty()) {
            if(evaluator._expressions.size() > 1)
                throw Exception(tr("Expression %1 is empty.").arg(i+1));
            else
                throw Exception(tr("Expression is empty."));
        }

        try {

            // Configure parser to accept alpha-numeric characters and '.' in variable names.
            parser->DefineNameChars(_validVariableNameChars.c_str());

            // Define some extra math functions.
            parser->DefineFun(_T("fmod"), static_cast<double (*)(double,double)>(fmod), false);

            // Let the muParser process the math expression.
            parser->SetExpr(*expr);

            // Register input variables.
            for(ExpressionVariable& v : _variables) {
                if(v.isRegistered)
                    parser->DefineVar(v.mangledName, &v.value);
            }

            // Query list of used variables.
            for(const auto& vname : parser->GetUsedVar()) {
                for(ExpressionVariable& var : _variables) {
                    if(var.isRegistered && var.mangledName == vname.first) {
                        var.isReferenced = true;
                    }
                }
            }
        }
        catch(mu::Parser::exception_type& ex) {
            throw Exception(convertMuString(ex.GetMsg()));
        }
    }
}

/******************************************************************************
* The worker routine.
******************************************************************************/
void PropertyExpressionEvaluator::Worker::run(size_t startIndex, size_t endIndex, std::function<void(size_t,size_t,double)> callback, std::function<bool(size_t)> filter)
{
    try {
        for(size_t i = startIndex; i < endIndex; i++) {
            if(filter && !filter(i))
                continue;

            for(size_t j = 0; j < _parsers.size(); j++) {
                // Evaluate expression for the current data element.
                callback(i, j, evaluate(i, j));
            }
        }
    }
    catch(const Exception& ex) {
        _errorMsg = ex.message();
    }
}

/******************************************************************************
* The innermost evaluation routine.
******************************************************************************/
double PropertyExpressionEvaluator::Worker::evaluate(size_t elementIndex, size_t component)
{
    OVITO_ASSERT(component < _parsers.size());
    try {
        if(elementIndex != _lastElementIndex) {
            _lastElementIndex = elementIndex;

            // Update variable values for the current data element.
            _evaluator.updateVariables(*this, elementIndex);
        }

        // Evaluate expression for the current data element.
        return _parsers[component].Eval();
    }
    catch(const mu::Parser::exception_type& ex) {
        throw Exception(convertMuString(ex.GetMsg()));
    }
}

/******************************************************************************
* Retrieves the value of the variable and stores it in the memory location
* passed to muparser.
******************************************************************************/
void PropertyExpressionEvaluator::ExpressionVariable::updateValue(size_t elementIndex)
{
    if(!isReferenced)
        return;

    switch(type) {
    case FLOAT32_PROPERTY:
        if(elementIndex < propertyAccess.size())
            value = *reinterpret_cast<const float*>(dataPointer + stride * elementIndex);
        break;
    case FLOAT64_PROPERTY:
        if(elementIndex < propertyAccess.size())
            value = *reinterpret_cast<const double*>(dataPointer + stride * elementIndex);
        break;
    case INT8_PROPERTY:
        if(elementIndex < propertyAccess.size())
            value = *reinterpret_cast<const int8_t*>(dataPointer + stride * elementIndex);
        break;
    case INT32_PROPERTY:
        if(elementIndex < propertyAccess.size())
            value = *reinterpret_cast<const int32_t*>(dataPointer + stride * elementIndex);
        break;
    case INT64_PROPERTY:
        if(elementIndex < propertyAccess.size())
            value = *reinterpret_cast<const int64_t*>(dataPointer + stride * elementIndex);
        break;
    case ELEMENT_INDEX:
        value = elementIndex;
        break;
    case DERIVED_PROPERTY:
        value = function(elementIndex);
        break;
    case GLOBAL_PARAMETER:
    case CONSTANT:
        // Nothing to do.
        break;
    }
}

/******************************************************************************
* Returns a human-readable text listing the input variables.
******************************************************************************/
QString PropertyExpressionEvaluator::inputVariableTable() const
{
    QString str(tr("<p><b>Properties:</b><ul>"));
    for(const ExpressionVariable& v : _variables) {
        if((v.type == FLOAT32_PROPERTY || v.type == FLOAT64_PROPERTY || v.type == INT8_PROPERTY || v.type == INT32_PROPERTY || v.type == INT64_PROPERTY || v.type == ELEMENT_INDEX || v.type == DERIVED_PROPERTY) && v.isRegistered && v.variableClass == 0) {
            if(v.description.isEmpty())
                str.append(QStringLiteral("<li>%1</li>").arg(convertMuString(v.mangledName)));
            else
                str.append(QStringLiteral("<li>%1 (<i style=\"color: #555;\">%2</i>)</li>").arg(convertMuString(v.mangledName)).arg(v.description));
        }
    }
    str.append(QStringLiteral("</ul></p><p><b>Global values:</b><ul>"));
    for(const ExpressionVariable& v : _variables) {
        if(v.type == GLOBAL_PARAMETER && v.isRegistered) {
            if(v.description.isEmpty())
                str.append(QStringLiteral("<li>%1</li>").arg(convertMuString(v.mangledName)));
            else
                str.append(QStringLiteral("<li>%1 (<i style=\"color: #555;\">%2</i>)</li>").arg(convertMuString(v.mangledName)).arg(v.description));
        }
    }
    str.append(QStringLiteral("</ul></p><p><b>Constants:</b><ul>"));
    for(const ExpressionVariable& v : _variables) {
        if(v.type == CONSTANT && v.isRegistered) {
            if(v.description.isEmpty())
                str.append(QStringLiteral("<li>%1</li>").arg(convertMuString(v.mangledName)));
            else
                str.append(QStringLiteral("<li>%1 (<i style=\"color: #555;\">%2</i>)</li>").arg(convertMuString(v.mangledName)).arg(v.description));
        }
    }
    str.append(QStringLiteral("</ul></p>"));
    return str;
}

}  // namespace Ovito
