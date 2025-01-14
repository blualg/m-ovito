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

//
// Standard precompiled header file included by all source files in this module
//

#ifndef __OVITO_CORE_
#define __OVITO_CORE_

/******************************************************************************
* Standard Template Library (STL)
******************************************************************************/
#include <algorithm>
#include <array>
#include <atomic>
#include <cinttypes>
#include <clocale>
#include <cmath>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <forward_list>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <span>
#include <stack>
#include <thread>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>
#include <version>

/******************************************************************************
* Qt framework
******************************************************************************/
#define QT_EXPLICIT_QFILE_CONSTRUCTION_FROM_PATH    // Force QFile(const QString&) class constructor to be explicit.
#include <QBrush>
#include <QBuffer>
#include <QCache>
#include <QColor>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFont>
#include <QGenericMatrix>
#include <QGuiApplication>
#include <QImage>
#include <QMap>
#include <QMatrix4x4>
#include <QMetaClassInfo>
#include <QMutex>
#include <QMutex>
#include <QPainter>
#include <QPainterPath>
#include <QPair>
#include <QPen>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QResource>
#include <QRunnable>
#include <QStringList>
#include <QtDebug>
#include <QTemporaryFile>
#include <QtGlobal>
#include <QThread>
#include <QTimer>
#include <QtMath>
#include <QUrl>
#include <QVariant>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
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
#include <boost/algorithm/algorithm.hpp>
#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/algorithm/cxx11/iota.hpp>
#include <boost/algorithm/cxx11/none_of.hpp>
#include <boost/algorithm/cxx11/one_of.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <boost/range/algorithm_ext/is_sorted.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/counting_range.hpp>
#include <boost/range/irange.hpp>

/******************************************************************************
* SYCL
******************************************************************************/
#if defined(OVITO_USE_SYCL) && !defined(Q_MOC_RUN)
    #ifdef OVITO_USE_SYCL_ACPP
        #include <CL/sycl.hpp>
        using namespace cl;
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
#define TCB_SPAN_NAMESPACE_NAME Ovito
#include <ovito/core/utilities/DataTypes.h>
#include <ovito/core/utilities/Exception.h>
#include <ovito/core/utilities/linalg/LinAlg.h>
#include <ovito/core/utilities/Color.h>
#include <ovito/core/utilities/concurrent/Future.h>
#include <ovito/core/utilities/concurrent/SCFuture.h>
#include <ovito/core/utilities/concurrent/SharedFuture.h>
#include <ovito/core/utilities/concurrent/WeakSharedFuture.h>
#include <ovito/core/utilities/concurrent/Promise.h>
#include <ovito/core/utilities/concurrent/CoroutinePromise.h>
#include <ovito/core/utilities/concurrent/MainThreadOperation.h>
#include <ovito/core/oo/OvitoObject.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/utilities/concurrent/Launch.h>
#include <ovito/core/utilities/concurrent/TaskProgress.h>

#endif // __OVITO_CORE_
