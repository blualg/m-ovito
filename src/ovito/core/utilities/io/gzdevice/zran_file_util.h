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

#pragma once

static int fseek_(void *f, int64_t offset)
{
    QIODevice* device = static_cast<QIODevice*>(f);
    if(device->seek(offset))
        return 0;
    else
        return -1;
}

static int64_t fsize_(void *f)
{
    QIODevice* device = static_cast<QIODevice*>(f);
    return device->size();
}

static int64_t fread_(void *ptr, size_t size, size_t nmemb, void *f)
{
    QIODevice* device = static_cast<QIODevice*>(f);
    if(device->atEnd())
        return 0;
    qint64 nread = device->read(static_cast<char*>(ptr), size * nmemb);
//    qDebug() << "nread:" << nread << "of" << (size * nmemb);
    if(nread < 0)
        return -1;
    return nread / size;
}

static int64_t ftell_(void *f)
{
    QIODevice* device = static_cast<QIODevice*>(f);
    return device->pos();
}

static int feof_(void *f, size_t f_ret)
{
    QIODevice* device = static_cast<QIODevice*>(f);
    return device->atEnd();
}

static int getc_(void *f)
{
    char c;
    QIODevice* device = static_cast<QIODevice*>(f);
    if(device->getChar(&c))
        return c;
    else
        return -1;
}

#if 0
int fflush_(void *f);
size_t fwrite_(const void *ptr, size_t size, size_t nmemb, void *f);
#endif

static int seekable_(void *f)
{
    QIODevice* device = static_cast<QIODevice*>(f);
    return !device->isSequential();
}
