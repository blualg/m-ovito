////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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

#include <ovito/particles/util/ParticleSelectionHelper.h>
#include <ovito/particles/util/ParticleExpressionEvaluator.h>
#include <ovito/particles/objects/Particles.h>

#include <QHash>
#include <QRegularExpression>
#include <algorithm>
#include <unordered_set>

namespace Ovito {

std::vector<int> parseParticleTypeIds(const QString& typeListText,
                                      const Property* typeProperty,
                                      const QString& roleDescription,
                                      const QString& contextDescription)
{
    if(!typeProperty || !typeProperty->isTypedProperty())
        throw Exception(QObject::tr("%1 requires a typed 'Particle Type' property with defined element types.").arg(contextDescription));

    const QString trimmedText = typeListText.trimmed();
    if(trimmedText.isEmpty())
        throw Exception(QObject::tr("Please enter at least one %1.").arg(roleDescription));

    QHash<QString, int> nameToId;
    for(const ElementType* type : typeProperty->elementTypes()) {
        if(!type->name().isEmpty())
            nameToId.insert(type->name(), type->numericId());
        nameToId.insert(type->nameOrNumericId(), type->numericId());
    }

    std::vector<int> typeIds;
    const QStringList tokens = trimmedText.split(QRegularExpression(QStringLiteral("[,;]")), Qt::SkipEmptyParts);
    for(QString token : tokens) {
        token = token.trimmed();
        if(token.isEmpty())
            continue;

        int typeId = 0;
        if(nameToId.contains(token)) {
            typeId = nameToId.value(token);
        }
        else {
            bool ok = false;
            typeId = token.toInt(&ok);
            if(!ok || !typeProperty->elementType(typeId))
                throw Exception(QObject::tr("Unknown %1 '%2'. Use particle type names or numeric IDs separated by commas.")
                                    .arg(roleDescription, token));
        }

        if(std::find(typeIds.begin(), typeIds.end(), typeId) == typeIds.end())
            typeIds.push_back(typeId);
    }

    if(typeIds.empty())
        throw Exception(QObject::tr("Please enter at least one valid %1.").arg(roleDescription));

    return typeIds;
}

QString canonicalizeTypeList(QString typeListText)
{
    QStringList tokens = typeListText.split(QRegularExpression(QStringLiteral("[,;]")), Qt::SkipEmptyParts);
    for(QString& token : tokens)
        token = token.trimmed();
    tokens.erase(std::remove_if(tokens.begin(), tokens.end(), [](const QString& token) { return token.isEmpty(); }), tokens.end());
    std::sort(tokens.begin(), tokens.end());
    return tokens.join(QStringLiteral(","));
}

QString canonicalizeParticleSelector(const QString& typeListText, const QString& expressionText)
{
    const QString trimmedExpression = expressionText.trimmed();
    if(!trimmedExpression.isEmpty())
        return QStringLiteral("expr:%1").arg(trimmedExpression.simplified());
    return QStringLiteral("types:%1").arg(canonicalizeTypeList(typeListText));
}

std::vector<uint8_t> evaluateParticleSelector(const PipelineFlowState& state,
                                              const Particles* particles,
                                              const Property* typeProperty,
                                              const BufferReadAccess<int32_t>& particleTypes,
                                              const QString& typeListText,
                                              const QString& expressionText,
                                              const QString& roleDescription,
                                              const QString& contextDescription,
                                              size_t* matchCount)
{
    const QString trimmedExpression = expressionText.trimmed();
    std::vector<uint8_t> mask(particles->elementCount(), 0);
    size_t selectedCount = 0;

    if(!trimmedExpression.isEmpty()) {
        static const QRegularExpression assignmentRegex(QStringLiteral("[^=!><]=(?!=)"));
        if(trimmedExpression.contains(assignmentRegex))
            throw Exception(QObject::tr("The %1 expression contains the assignment operator '='. Please use the comparison operator '==' instead.")
                                .arg(roleDescription));

        ParticleExpressionEvaluator evaluator;
        ConstDataObjectPath containerPath = state.data()->expectObject(DataObjectReference(&Particles::OOClass()));
        const int animationFrame = state.data() ? std::max(0, state.data()->sourceFrame()) : 0;
        evaluator.initialize(QStringList(trimmedExpression), state, containerPath, animationFrame);
        PropertyExpressionEvaluator::Worker worker(evaluator);
        for(size_t particleIndex = 0; particleIndex < particles->elementCount(); ++particleIndex) {
            if(worker.evaluate(particleIndex, 0)) {
                mask[particleIndex] = 1;
                selectedCount++;
            }
        }
    }
    else {
        const std::vector<int> typeIds = parseParticleTypeIds(typeListText, typeProperty, roleDescription, contextDescription);
        const std::unordered_set<int> allowedTypes(typeIds.begin(), typeIds.end());
        for(size_t particleIndex = 0; particleIndex < particles->elementCount(); ++particleIndex) {
            if(allowedTypes.find(particleTypes[particleIndex]) != allowedTypes.end()) {
                mask[particleIndex] = 1;
                selectedCount++;
            }
        }
    }

    if(matchCount)
        *matchCount = selectedCount;
    return mask;
}

PropertyPtr createSelectionPropertyFromMask(const std::vector<uint8_t>& mask)
{
    PropertyPtr selectionProperty =
        Particles::OOClass().createStandardProperty(DataBuffer::Initialized, mask.size(), Particles::SelectionProperty);
    BufferWriteAccess<SelectionIntType, access_mode::discard_write> selection(selectionProperty);
    for(size_t particleIndex = 0; particleIndex < mask.size(); ++particleIndex)
        selection[particleIndex] = mask[particleIndex] ? 1 : 0;
    return selectionProperty;
}

}   // End of namespace Ovito
