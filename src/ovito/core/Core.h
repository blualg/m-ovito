////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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

//
// Standard precompiled header file included by all source files in this module
//

#ifndef __OVITO_CORE_
#define __OVITO_CORE_

/******************************************************************************
* Standard Template Library (STL)
******************************************************************************/
#include <iostream>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cinttypes>
#include <cstring>
#include <type_traits>
#include <stack>
#include <array>
#include <vector>
#include <forward_list>
#include <map>
#include <unordered_map>
#include <set>
#include <utility>
#include <random>
#include <memory>
#include <mutex>
#include <thread>
#include <clocale>
#include <atomic>
#include <tuple>
#include <numeric>
#include <functional>
#include <optional>
#include <queue>

/******************************************************************************
* Qt framework classes.
******************************************************************************/
#include <QCoreApplication>
#include <QStringList>
#include <QUrl>
#include <QPointer>
#include <QFileInfo>
#include <QResource>
#include <QDir>
#include <QtDebug>
#include <QtGlobal>
#include <QMetaClassInfo>
#include <QColor>
#include <QGenericMatrix>
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QDateTime>
#include <QThread>
#include <QMutex>
#include <QRunnable>
#include <QImage>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QTimer>
#include <QCache>
#include <QMutex>
#include <QTemporaryFile>
#include <QElapsedTimer>
#include <QtMath>
#include <QBuffer>
#include <QRegularExpression>
#include <QPair>
#include <QVariant>
#include <QMap>
#include <QProcess>
#ifndef OVITO_DISABLE_THREADING
    #include <QThreadPool>
    #include <QWaitCondition>
#endif
#ifndef OVITO_DISABLE_QSETTINGS
    #include <QSettings>
#endif
#ifndef OVITO_DISABLE_THREADING
    #include <QException>
#endif
#ifndef Q_OS_WASM
    #include <QNetworkAccessManager>
#endif
#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
    #define QLatin1StringView QLatin1String
#endif

/******************************************************************************
* Boost library
******************************************************************************/
#include <boost/dynamic_bitset.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/algorithm_ext/is_sorted.hpp>
#include <boost/range/irange.hpp>
#include <boost/range/counting_range.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <boost/algorithm/algorithm.hpp>
#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/algorithm/cxx11/none_of.hpp>
#include <boost/algorithm/cxx11/one_of.hpp>
#include <boost/algorithm/cxx11/iota.hpp>

/******************************************************************************
* SYCL
******************************************************************************/
#ifdef OVITO_USE_SYCL
    #ifdef OVITO_USE_OPENSYCL
        #include <CL/sycl.hpp>
    #else
        #include <sycl/sycl.hpp>
    #endif
#endif

/******************************************************************************
* Forward declaration of classes.
******************************************************************************/
#include "ForwardDecl.h"

/******************************************************************************
* Our own basic headers
******************************************************************************/
#include <ovito/core/utilities/Debugging.h>
#include <ovito/core/utilities/Invoke.h>
#define TCB_SPAN_NAMESPACE_NAME Ovito
#include <ovito/core/utilities/Span.h>
#include <ovito/core/utilities/DataTypes.h>
#include <ovito/core/utilities/Exception.h>
#include <ovito/core/utilities/linalg/LinAlg.h>
#include <ovito/core/utilities/Color.h>
#include <ovito/core/utilities/concurrent/Future.h>
#include <ovito/core/utilities/concurrent/SharedFuture.h>
#include <ovito/core/utilities/concurrent/WeakSharedFuture.h>
#include <ovito/core/utilities/concurrent/Promise.h>
#include <ovito/core/utilities/concurrent/MainThreadOperation.h>
#include <ovito/core/oo/OvitoObject.h>

#endif // __OVITO_CORE_
