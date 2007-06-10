/* ============================================================
 *
 * This file is a part of kipi-plugins project
 * http://www.kipi-plugins.org
 *
 * Date        : 2003-12-03
 * Description : misc utils to used in batch process
 * 
 * Copyright (C) 2003-2005 by Renchi Raju <renchi@pooh.tam.uiuc.edu>
 * Copyright (C) 2006-2007 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation;
 * either version 2, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * ============================================================ */

// C Ansi includes.

extern "C" 
{
#include <utime.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
}

// Qt includes.

#include <QFileInfo>
#include <QImageReader>
#include <QImage>
#include <QString>
#include <QFile>
#include <QDir>

// KDE includes.

#include <kdebug.h>

// LibKDcraw includes.

#include <libkdcraw/rawfiles.h>

// Local includes.

#include "utils.h"

namespace KIPIJPEGLossLessPlugin
{

bool Utils::isJPEG(const QString& file)
{
    QString format = QString(QImageReader::imageFormat(file)).toUpper();
    return format=="JPEG";
}

bool Utils::isRAW(const QString& file)
{
    QString rawFilesExt(raw_file_extentions);

    QFileInfo fileInfo(file);
    if (rawFilesExt.toUpper().contains( fileInfo.suffix(false).toUpper() ))
        return true;

    return false;
}

bool Utils::CopyFile(const QString& src, const QString& dst)
{
    QFile sFile(src);
    QFile dFile(dst);

    if ( !sFile.open(IO_ReadOnly) )
        return false;

    if ( !dFile.open(IO_WriteOnly) )
    {
        sFile.close();
        return false;
    }

    const int MAX_IPC_SIZE = (1024*32);
    char buffer[MAX_IPC_SIZE];

    Q_LONG len;
    while ((len = sFile.readBlock(buffer, MAX_IPC_SIZE)) != 0)
    {
        if (len == -1 || dFile.writeBlock(buffer, (Q_ULONG)len) == -1)
        {
            sFile.close();
            dFile.close();
            return false;
        }
    }

    sFile.close();
    dFile.close();

    return true;
}

bool Utils::MoveFile(const QString& src, const QString& dst)
{
    struct stat stbuf;
    if (::stat(QFile::encodeName(dst), &stbuf) != 0)
    {
        kWarning( 51000 ) << "KIPIJPEGLossLessPlugin:MoveFile: failed to stat src"
                          << endl;
        return false;
    }
    
    if (!CopyFile(src, dst))
        return false;

    struct utimbuf timbuf;
    timbuf.actime = stbuf.st_atime;
    timbuf.modtime = stbuf.st_mtime;
    if (::utime(QFile::encodeName(dst), &timbuf) != 0)
    {
        kWarning( 51000 ) << "KIPIJPEGLossLessPlugin:MoveFile: failed to update dst time"
                          << endl;
    }
    
    if (::unlink(QFile::encodeName(src).data()) != 0)
    {
        kWarning( 51000 ) << "KIPIJPEGLossLessPlugin:MoveFile: failed to unlink src"
                          << endl;
    }
    return true;
}

bool Utils::deleteDir(const QString& dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists()) 
        return false;

    dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoSymLinks);

    const QFileInfoList* infoList = dir.entryInfoList();
    if (!infoList) 
        return false;

    QFileInfoListIterator it(*infoList);
    QFileInfo* fi;

    while( (fi = it.current()) ) 
    {
        ++it;
        if(fi->fileName() == "." || fi->fileName() == ".." )
            continue;

        if( fi->isDir() ) 
        {
            deleteDir(fi->absFilePath());
        }
        else if( fi->isFile() )
            dir.remove(fi->absFilePath());
    }

    dir.rmdir(dir.absPath());
    return true;
}

}  // NameSpace KIPIJPEGLossLessPlugin
