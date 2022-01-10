/*
 * Copyright (C) 2020 ~ 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     dengkeyun<dengkeyun@uniontech.com>
 *
 * Maintainer: max-lv<lvwujun@uniontech.com>
 *             xushitong<xushitong@uniontech.com>
 *             zhangsheng<zhangsheng@uniontech.com>
 *             lanxuesong<lanxuesong@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "local/dlocalfile.h"
#include "local/dlocalfile_p.h"
#include "local/dlocalhelper.h"

#include "core/dfile_p.h"

#include <QDebug>

#include <sys/stat.h>

USING_IO_NAMESPACE

DLocalFilePrivate::DLocalFilePrivate(DLocalFile *q)
    : q(q)
{
}

DLocalFilePrivate::~DLocalFilePrivate()
{
}

bool DLocalFilePrivate::open(DFile::OpenFlags mode)
{
    if (q->isOpen()) {
        error.setCode(DFMIOErrorCode::DFM_IO_ERROR_OPEN_FAILED);

        //qWarning() << "File already open";
        return false;
    }

    // check mode
    if (!checkOpenFlags(&mode)) {
        error.setCode(DFMIOErrorCode::DFM_IO_ERROR_OPEN_FAILED);
        return false;
    }

    const QUrl &&uri = q->uri();
    g_autoptr(GFile) gfile = g_file_new_for_uri(uri.toString().toLocal8Bit().data());
    g_autoptr(GError) gerror = nullptr;

    if ((mode & DFile::OpenFlag::ReadOnly) == 0x0001) {
        if (!exists()) {
            return false;
        }
        iStream = (GInputStream *)g_file_read(gfile, nullptr, &gerror);
        if (gerror)
            setErrorInfo(gerror);

        if (!iStream) {
            return false;
        }
        return true;
    } else if ((mode & DFile::OpenFlag::WriteOnly) == 0x0002) {
        oStream = (GOutputStream *)g_file_replace(gfile,
                                                  nullptr,
                                                  false,
                                                  G_FILE_CREATE_NONE,
                                                  nullptr,
                                                  &gerror);
        if (gerror)
            setErrorInfo(gerror);

        if (!oStream) {
            return false;
        }
        return true;
    } else if ((mode & DFile::OpenFlag::Append) == 0x0004) {
        oStream = (GOutputStream *)g_file_append_to(gfile, G_FILE_CREATE_NONE, nullptr, &gerror);
        if (gerror)
            setErrorInfo(gerror);

        if (!oStream) {
            return false;
        }
        return true;
    } else {
        ioStream = (GIOStream *)g_file_replace_readwrite(gfile,
                                                         nullptr,
                                                         false,
                                                         G_FILE_CREATE_NONE,
                                                         nullptr,
                                                         &gerror);
        if (gerror)
            setErrorInfo(gerror);

        if (!ioStream) {
            return false;
        }
        return true;
    }
}

bool DLocalFilePrivate::close()
{
    if (iStream) {
        g_object_unref(iStream);
        iStream = nullptr;
    }
    if (oStream) {
        g_object_unref(oStream);
        oStream = nullptr;
    }
    if (ioStream) {
        g_object_unref(ioStream);
        ioStream = nullptr;
    }
    return true;
}

qint64 DLocalFilePrivate::read(char *data, qint64 maxSize)
{
    GInputStream *inputStream = this->inputStream();
    if (!inputStream) {
        //qWarning() << "need input_stream before read.";
        return -1;
    }

    GError *gerror = nullptr;
    gssize read = g_input_stream_read(inputStream,
                                      data,
                                      static_cast<gsize>(maxSize),
                                      nullptr,
                                      &gerror);

    if (gerror) {
        setErrorInfo(gerror);
        g_error_free(gerror);
        return -1;
    }

    return read;
}

QByteArray DLocalFilePrivate::read(qint64 maxSize)
{
    GInputStream *inputStream = this->inputStream();
    if (!inputStream) {
        //qWarning() << "need input_stream before read.";
        return QByteArray();
    }

    char data[maxSize];
    GError *gerror = nullptr;
    g_input_stream_read(inputStream,
                        data,
                        static_cast<gsize>(maxSize),
                        nullptr,
                        &gerror);
    if (gerror) {
        setErrorInfo(gerror);
        g_error_free(gerror);
        return QByteArray();
    }

    return QByteArray(data);
}

QByteArray DLocalFilePrivate::readAll()
{
    GInputStream *inputStream = this->inputStream();
    if (!inputStream) {
        //qWarning() << "need input_stream before read.";
        return QByteArray();
    }

    QByteArray dataRet;

    const gsize size = 8192;

    gsize bytes_read;
    char data[size];
    GError *gerror = nullptr;

    while (true) {
        gboolean read = g_input_stream_read_all(inputStream,
                                                data,
                                                size,
                                                &bytes_read,
                                                nullptr,
                                                &gerror);
        if (!read || gerror) {
            if (gerror) {
                setErrorInfo(gerror);
                g_error_free(gerror);
            }
            break;
        }
        dataRet.append(data);
        memset(data, 0, size);
    }

    return dataRet;
}

qint64 DLocalFilePrivate::write(const char *data, qint64 maxSize)
{
    GOutputStream *outputStream = this->outputStream();
    if (!outputStream) {
        //qWarning() << "need output_stream before read.";
        return -1;
    }

    GError *gerror = nullptr;
    gssize write = g_output_stream_write(outputStream,
                                         data,
                                         static_cast<gsize>(maxSize),
                                         nullptr,
                                         &gerror);
    if (gerror) {
        setErrorInfo(gerror);
        g_error_free(gerror);
    }
    return write;
}

qint64 DLocalFilePrivate::write(const char *data)
{
    GOutputStream *outputStream = this->outputStream();
    if (!outputStream) {
        //qWarning() << "need output_stream before read.";
        return -1;
    }

    gsize bytes_write;
    GError *gerror = nullptr;
    gssize write = g_output_stream_write_all(outputStream,
                                             data,
                                             strlen(data),
                                             &bytes_write,
                                             nullptr,
                                             &gerror);
    if (gerror) {
        setErrorInfo(gerror);
        g_error_free(gerror);
    }
    return write;
}

qint64 DLocalFilePrivate::write(const QByteArray &data)
{
    return write(data.data());
}

bool DLocalFilePrivate::seek(qint64 pos, DFile::DFMSeekType type)
{
    GInputStream *inputStream = this->inputStream();
    if (!inputStream) {
        //qWarning() << "need input_stream before read.";
        return -1;
    }

    gboolean canSeek = G_IS_SEEKABLE(inputStream) && g_seekable_can_seek(G_SEEKABLE(inputStream));
    if (!canSeek) {
        //qWarning() << "try seek failed.";
        return false;
    }

    GSeekable *seekable = G_SEEKABLE(inputStream);
    if (!seekable) {
        //qWarning() << "try seek failed.";
        return false;
    }

    bool ret = false;
    GError *gerror = nullptr;
    GSeekType gtype = G_SEEK_CUR;
    switch (type) {
    case DFile::DFMSeekType::BEGIN:
        gtype = G_SEEK_SET;
        break;
    case DFile::DFMSeekType::END:
        gtype = G_SEEK_END;
        break;

    default:
        break;
    }

    ret = g_seekable_seek(seekable, pos, gtype, nullptr, &gerror);
    if (gerror) {
        setErrorInfo(gerror);
        g_error_free(gerror);
    }

    return ret;
}

qint64 DLocalFilePrivate::pos()
{
    GInputStream *inputStream = this->inputStream();
    if (!inputStream) {
        //qWarning() << "need input_stream before read.";
        return -1;
    }

    gboolean canSeek = G_IS_SEEKABLE(inputStream) && g_seekable_can_seek(G_SEEKABLE(inputStream));
    if (!canSeek) {
        //qWarning() << "try seek failed.";
        return false;
    }

    GSeekable *seekable = G_SEEKABLE(inputStream);
    if (!seekable) {
        //qWarning() << "try seek failed.";
        return false;
    }

    goffset pos = g_seekable_tell(seekable);

    return qint64(pos);
}

bool DLocalFilePrivate::flush()
{
    GOutputStream *outputStream = this->outputStream();
    if (!outputStream) {
        //qWarning() << "need output_stream before read.";
        return false;
    }

    GError *gerror = nullptr;
    gboolean ret = g_output_stream_flush(outputStream, nullptr, &gerror);

    if (gerror) {
        setErrorInfo(gerror);
        g_error_free(gerror);
    }
    return ret;
}

qint64 DLocalFilePrivate::size()
{
    const QUrl &uri = q->uri();
    GFile *gfile = g_file_new_for_uri(uri.toString().toStdString().c_str());

    GError *gerror = nullptr;
    GFileInfo *fileInfo = g_file_query_info(gfile, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, nullptr, &gerror);

    if (gerror) {
        setErrorInfo(gerror);
        g_error_free(gerror);
    }

    if (fileInfo) {
        goffset size = g_file_info_get_size(fileInfo);
        g_object_unref(fileInfo);
        return qint64(size);
    }

    return -1;
}

bool DLocalFilePrivate::exists()
{
    const QUrl &&uri = q->uri();
    GFile *gfile = g_file_new_for_uri(uri.toString().toLocal8Bit().data());
    const bool exists = g_file_query_exists(gfile, nullptr);

    g_object_unref(gfile);
    return exists;
}

DFile::Permissions DLocalFilePrivate::permissions()
{
    DFile::Permissions retValue = DFile::Permission::NoPermission;
    // 获取系统默认权限
    const QUrl &&url = q->uri();
    const char *path = url.toLocalFile().toLocal8Bit().data();

    struct stat buf;
    stat(path, &buf);

    if ((buf.st_mode & S_IXUSR) == S_IXUSR)
        retValue |= DFile::Permission::ExeOwner;
    if ((buf.st_mode & S_IWUSR) == S_IWUSR)
        retValue |= DFile::Permission::WriteOwner;
    if ((buf.st_mode & S_IRUSR) == S_IRUSR)
        retValue |= DFile::Permission::ReadOwner;

    if ((buf.st_mode & S_IXGRP) == S_IXGRP)
        retValue |= DFile::Permission::ExeGroup;
    if ((buf.st_mode & S_IWGRP) == S_IWGRP)
        retValue |= DFile::Permission::WriteGroup;
    if ((buf.st_mode & S_IRGRP) == S_IRGRP)
        retValue |= DFile::Permission::ReadGroup;

    if ((buf.st_mode & S_IXOTH) == S_IXOTH)
        retValue |= DFile::Permission::ExeOther;
    if ((buf.st_mode & S_IWOTH) == S_IWOTH)
        retValue |= DFile::Permission::WriteOther;
    if ((buf.st_mode & S_IROTH) == S_IROTH)
        retValue |= DFile::Permission::ReadOther;

    retValue |= permissionsFromGio();

    return retValue;
}

bool DLocalFilePrivate::setPermissions(DFile::Permissions permission)
{
    const QUrl &&url = q->uri();
    const char *path = url.toLocalFile().toLocal8Bit().data();

    struct stat buf;
    if (permission.testFlag(DFile::Permission::ExeOwner) | permission.testFlag(DFile::Permission::ExeUser))
        buf.st_mode |= S_IXUSR;
    if (permission.testFlag(DFile::Permission::WriteOwner) | permission.testFlag(DFile::Permission::WriteUser))
        buf.st_mode |= S_IWUSR;
    if (permission.testFlag(DFile::Permission::ReadOwner) | permission.testFlag(DFile::Permission::ReadUser))
        buf.st_mode |= S_IRUSR;

    if (permission.testFlag(DFile::Permission::ExeGroup))
        buf.st_mode |= S_IXGRP;
    if (permission.testFlag(DFile::Permission::WriteGroup))
        buf.st_mode |= S_IWGRP;
    if (permission.testFlag(DFile::Permission::ReadGroup))
        buf.st_mode |= S_IRGRP;

    if (permission.testFlag(DFile::Permission::ExeOther))
        buf.st_mode |= S_IXOTH;
    if (permission.testFlag(DFile::Permission::WriteOther))
        buf.st_mode |= S_IWOTH;
    if (permission.testFlag(DFile::Permission::ReadOther))
        buf.st_mode |= S_IROTH;

    return ::chmod(path, buf.st_mode) == 0;
}

DFMIOError DLocalFilePrivate::lastError()
{
    return error;
}

DFile::Permissions DLocalFilePrivate::permissionsFromGio()
{
    DFile::Permissions retValue = DFile::Permission::NoPermission;

    const QUrl &&url = q->uri();
    const QString &&path = url.toString();

    GFile *file = g_file_new_for_uri(path.toLocal8Bit().data());

    g_autoptr(GError) gerror = nullptr;

    GFileInfo *gfileinfo = g_file_query_info(file, "access::*", G_FILE_QUERY_INFO_NONE, nullptr, &gerror);
    g_object_unref(file);

    if (gerror) {
        setErrorInfo(gerror);
    }

    if (!gfileinfo)
        return retValue;

    if (gfileinfo) {
        if (g_file_info_get_attribute_boolean(gfileinfo, "access::can-execute"))
            retValue |= DFile::Permission::ExeUser;
        if (g_file_info_get_attribute_boolean(gfileinfo, "access::can-write"))
            retValue |= DFile::Permission::WriteUser;
        if (g_file_info_get_attribute_boolean(gfileinfo, "access::can-read"))
            retValue |= DFile::Permission::ReadUser;

        g_object_unref(gfileinfo);
    }
    return retValue;
}

void DLocalFilePrivate::setErrorInfo(GError *gerror)
{
    error.setCode(DFMIOErrorCode(gerror->code));

    //qWarning() << QString::fromLocal8Bit(gerror->message);
}

bool DLocalFilePrivate::checkOpenFlags(DFile::OpenFlags *modeIn)
{
    DFile::OpenFlags &mode = *modeIn;

    if (mode & (DFile::OpenFlag::Append | DFile::OpenFlag::NewOnly))
        mode |= DFile::OpenFlag::WriteOnly;

    if ((mode & (DFile::OpenFlag::ReadOnly | DFile::OpenFlag::WriteOnly)) == 0) {
        //qWarning("DFile::open: File access not specified.");
        return false;
    }
    if ((mode & DFile::OpenFlag::NewOnly) && (mode & DFile::OpenFlag::ExistingOnly)) {
        //qWarning("NewOnly and ExistingOnly are mutually exclusive");
        return false;
    }
    if ((mode & DFile::OpenFlag::ExistingOnly) && !(mode & (DFile::OpenFlag::ReadOnly | DFile::OpenFlag::WriteOnly))) {
        //qWarning("ExistingOnly must be specified alongside ReadOnly, WriteOnly, or ReadWrite");
        return false;
    }

    // WriteOnly implies Truncate when ReadOnly, Append, and NewOnly are not set.
    if ((mode & DFile::OpenFlag::WriteOnly) && !(mode & (DFile::OpenFlag::ReadOnly | DFile::OpenFlag::Append | DFile::OpenFlag::NewOnly)))
        mode |= DFile::OpenFlag::Truncate;

    if (mode & DFile::OpenFlag::NewOnly) {
        if (exists()) {
            //qWarning("Open flag is NewOnly, but target file already exists");
            return false;
        }
    }
    if (mode & DFile::OpenFlag::ExistingOnly) {
        if (!exists()) {
            //qWarning("Open flag is ExistingOnly, but target file not exists");
            return false;
        }
    }

    return true;
}

GInputStream *DLocalFilePrivate::inputStream()
{
    if (iStream)
        return iStream;

    if (ioStream) {
        GInputStream *inputStream = g_io_stream_get_input_stream(ioStream);
        if (inputStream)
            return inputStream;
    }

    //qWarning() << "get input stream failed.";

    return nullptr;
}

GOutputStream *DLocalFilePrivate::outputStream()
{
    if (oStream)
        return oStream;

    if (ioStream) {
        GOutputStream *outputStream = g_io_stream_get_output_stream(ioStream);
        if (outputStream)
            return outputStream;
    }

    //qWarning() << "get out stream failed.";

    return nullptr;
}

DLocalFile::DLocalFile(const QUrl &uri)
    : DFile(uri), d(new DLocalFilePrivate(this))
{
    using bind_read = qint64 (DLocalFile::*)(char *, qint64);
    using bind_readQ = QByteArray (DLocalFile::*)(qint64);

    using bind_write = qint64 (DLocalFile::*)(const char *, qint64);
    using bind_writeAll = qint64 (DLocalFile::*)(const char *);
    using bind_writeQ = qint64 (DLocalFile::*)(const QByteArray &);

    registerOpen(std::bind(&DLocalFile::open, this, std::placeholders::_1));
    registerClose(std::bind(&DLocalFile::close, this));

    registerRead(std::bind<bind_read>(&DLocalFile::read, this, std::placeholders::_1, std::placeholders::_2));
    registerReadQ(std::bind<bind_readQ>(&DLocalFile::read, this, std::placeholders::_1));
    registerReadAll(std::bind(&DLocalFile::readAll, this));

    registerWrite(std::bind<bind_write>(&DLocalFile::write, this, std::placeholders::_1, std::placeholders::_2));
    registerWriteAll(std::bind<bind_writeAll>(&DLocalFile::write, this, std::placeholders::_1));
    registerWriteQ(std::bind<bind_writeQ>(&DLocalFile::write, this, std::placeholders::_1));

    registerSeek(std::bind(&DLocalFile::seek, this, std::placeholders::_1, std::placeholders::_2));
    registerPos(std::bind(&DLocalFile::pos, this));
    registerFlush(std::bind(&DLocalFile::flush, this));
    registerSize(std::bind(&DLocalFile::size, this));
    registerExists(std::bind(&DLocalFile::exists, this));
    registerPermissions(std::bind(&DLocalFile::permissions, this));
    registerSetPermissions(std::bind(&DLocalFile::setPermissions, this, std::placeholders::_1));
    registerLastError(std::bind(&DLocalFile::lastError, this));
}

DLocalFile::~DLocalFile()
{
    close();
}

bool DLocalFile::open(DFile::OpenFlags mode)
{
    return d->open(mode);
}

bool DLocalFile::close()
{
    return d->close();
}

qint64 DLocalFile::read(char *data, qint64 maxSize)
{
    return d->read(data, maxSize);
}

QByteArray DLocalFile::read(qint64 maxSize)
{
    return d->read(maxSize);
}

QByteArray DLocalFile::readAll()
{
    return d->readAll();
}

qint64 DLocalFile::write(const char *data, qint64 len)
{
    return d->write(data, len);
}

qint64 DLocalFile::write(const char *data)
{
    return d->write(data);
}

qint64 DLocalFile::write(const QByteArray &byteArray)
{
    return d->write(byteArray);
}

bool DLocalFile::seek(qint64 pos, DFile::DFMSeekType type)
{
    return d->seek(pos, type);
}

qint64 DLocalFile::pos()
{
    return d->pos();
}

bool DLocalFile::flush()
{
    return d->flush();
}

qint64 DLocalFile::size()
{
    return d->size();
}

bool DLocalFile::exists()
{
    return d->exists();
}

DFile::Permissions DLocalFile::permissions()
{
    return d->permissions();
}

bool DLocalFile::setPermissions(DFile::Permissions permission)
{
    return d->setPermissions(permission);
}

DFMIOError DLocalFile::lastError() const
{
    return d->lastError();
}
