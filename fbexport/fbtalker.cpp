/* ============================================================
 *
 * This file is a part of kipi-plugins project
 * http://www.kipi-plugins.org
 *
 * Date        : 2008-12-26
 * Description : a kipi plugin to export images to Facebook web service
 *
 * Copyright (C) 2008 by Luka Renko <lure at kmail dot org>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation;
 * either version 2, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#include "fbtalker.h"
#include "fbtalker.moc"

// Qt includes.
#include <QByteArray>
#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QProgressDialog>

// KDE includes.
#include <kcodecs.h>
#include <kdebug.h>
#include <kio/job.h>
#include <kio/jobuidelegate.h>
#include <kapplication.h>
#include <kmessagebox.h>
#include <ktoolinvocation.h>


// Local includes.
#include "pluginsversion.h"
#include "fbitem.h"
#include "mpform.h"

namespace KIPIFbExportPlugin
{

FbTalker::FbTalker(QWidget* parent)
{
    m_parent = parent;
    m_job    = 0;

    m_userAgent  = QString("KIPI-Plugin-FbExport/%1 (lure@kubuntu.org)").arg(kipiplugins_version);
    m_apiVersion = "1.0";
    m_apiURL     = "https://api.facebook.com/restserver.php";
    m_apiKey     = "bf430ad869b88aba5c0c17ea6707022b";
    m_secretKey  = "0434307e70dd12c414cc6d0928f132d8";
}

FbTalker::~FbTalker()
{
    if (loggedIn())
        logout();

    if (m_job)
        m_job->kill();
}

bool FbTalker::loggedIn()
{
    return !m_sessionKey.isEmpty();
}

QString FbTalker::getDisplayName() const
{
    return m_userName;
}

QString FbTalker::getProfileURL() const
{
    return m_userURL;
}

void FbTalker::cancel()
{
    if (m_job)
    {
        m_job->kill();
        m_job = 0;
    }

    if (m_authProgressDlg && !m_authProgressDlg->isHidden())
        m_authProgressDlg->hide();
}

/** Compute MD5 signature using url queries keys and values:
    http://wiki.developers.facebook.com/index.php/How_Facebook_Authenticates_Your_Application
*/
QString FbTalker::getApiSig(const QMap<QString, QString>& args)
{
    QString concat;
    // NOTE: QMap iterator will sort alphabetically
    for (QMap<QString, QString>::const_iterator it = args.begin(); 
         it != args.end(); 
         ++it)
    {
        concat.append(it.key());
        concat.append("=");
        concat.append(it.value());
    }
    concat.append(m_secretKey);

    KMD5 md5(concat.toUtf8());
    return md5.hexDigest().data();
}

QString FbTalker::getCallString(const QMap<QString, QString>& args)
{
    QString concat;
    // NOTE: QMap iterator will sort alphabetically
    for (QMap<QString, QString>::const_iterator it = args.begin(); 
         it != args.end(); 
         ++it)
    {
        if (!concat.isEmpty())
            concat.append("&");
        concat.append(it.key());
        concat.append("=");
        concat.append(it.value());
    }

    return concat;
}


void FbTalker::authenticate()
{
    if (m_job)
    {
        m_job->kill();
        m_job = 0;
    }
    emit signalBusy(true);

    m_authProgressDlg->setLabelText(i18n("Logging to Facebook service..."));
    m_authProgressDlg->setMaximum(6);
    m_authProgressDlg->setValue(1);

    QMap<QString, QString> args;
    args["method"]  = "facebook.auth.createToken";
    args["api_key"] = m_apiKey;
    args["v"]       = m_apiVersion;
    QString md5 = getApiSig(args);
    args["sig"]     =  md5;

    QByteArray tmp(getCallString(args).toUtf8());
    KIO::TransferJob* job = KIO::http_post(m_apiURL, tmp, KIO::HideProgressInfo);
    job->addMetaData("UserAgent", m_userAgent);
    job->addMetaData("content-type",
                     "Content-Type: application/x-www-form-urlencoded");

    connect(job, SIGNAL(data(KIO::Job*, const QByteArray&)),
            this, SLOT(data(KIO::Job*, const QByteArray&)));

    connect(job, SIGNAL(result(KJob*)),
            this, SLOT(slotResult(KJob*)));

    m_state = FB_CREATETOKEN;
    m_job   = job;
    m_buffer.resize(0);
}

void FbTalker::getSession()
{
    if (m_job)
    {
        m_job->kill();
        m_job = 0;
    }
    emit signalBusy(true);
    m_authProgressDlg->setValue(3);

    QMap<QString, QString> args;
    args["method"]      = "facebook.auth.getSession";
    args["api_key"]     = m_apiKey;
    args["v"]           = m_apiVersion;
    args["auth_token"]  = m_authToken;
    QString md5 = getApiSig(args);
    args["sig"]         =  md5;

    QByteArray tmp(getCallString(args).toUtf8());
    KIO::TransferJob* job = KIO::http_post(m_apiURL, tmp, KIO::HideProgressInfo);
    job->addMetaData("UserAgent", m_userAgent);
    job->addMetaData("content-type",
                     "Content-Type: application/x-www-form-urlencoded");

    connect(job, SIGNAL(data(KIO::Job*, const QByteArray&)),
            this, SLOT(data(KIO::Job*, const QByteArray&)));

    connect(job, SIGNAL(result(KJob*)),
            this, SLOT(slotResult(KJob*)));

    m_state = FB_GETSESSION;
    m_job   = job;
    m_buffer.resize(0);
}

void FbTalker::getUserInfo()
{
    if (m_job)
    {
        m_job->kill();
        m_job = 0;
    }
    emit signalBusy(true);
    m_authProgressDlg->setValue(5);

    QMap<QString, QString> args;
    args["method"]      = "facebook.users.getStandardInfo";
    args["api_key"]     = m_apiKey;
    args["v"]           = m_apiVersion;
    args["call_id"]     = QString::number(m_callID.elapsed());
    args["uids"]        = QString::number(m_uid);
    args["fields"]      = "name, profile_url";
    QString md5 = getApiSig(args);
    args["sig"]         =  md5;

    QByteArray tmp(getCallString(args).toUtf8());
    KIO::TransferJob* job = KIO::http_post(m_apiURL, tmp, KIO::HideProgressInfo);
    job->addMetaData("UserAgent", m_userAgent);
    job->addMetaData("content-type",
                     "Content-Type: application/x-www-form-urlencoded");

    connect(job, SIGNAL(data(KIO::Job*, const QByteArray&)),
            this, SLOT(data(KIO::Job*, const QByteArray&)));

    connect(job, SIGNAL(result(KJob*)),
            this, SLOT(slotResult(KJob*)));

    m_state = FB_GETUSERINFO;
    m_job   = job;
    m_buffer.resize(0);
}

void FbTalker::logout()
{
    if (m_job)
    {
        m_job->kill();
        m_job = 0;
    }
    emit signalBusy(true);

    QMap<QString, QString> args;
    args["method"]      = "facebook.auth.expireSession";
    args["api_key"]     = m_apiKey;
    args["v"]           = m_apiVersion;
    args["session_key"] = m_sessionKey;
    QString md5 = getApiSig(args);
    args["sig"]         =  md5;

    QByteArray tmp(getCallString(args).toUtf8());
    KIO::TransferJob* job = KIO::http_post(m_apiURL, tmp, KIO::HideProgressInfo);
    job->addMetaData("UserAgent", m_userAgent);
    job->addMetaData("content-type",
                     "Content-Type: application/x-www-form-urlencoded");

    connect(job, SIGNAL(data(KIO::Job*, const QByteArray&)),
            this, SLOT(data(KIO::Job*, const QByteArray&)));

    m_state = FB_LOGOUT;
    m_job   = job;
    m_buffer.resize(0);

    // logout is synchronous call
    job->exec();
    slotResult(job);
}

void FbTalker::listAlbums()
{
    if (m_job)
    {
        m_job->kill();
        m_job = 0;
    }
    emit signalBusy(true);

    QMap<QString, QString> args;
    args["method"]      = "facebook.photos.getAlbums";
    args["api_key"]     = m_apiKey;
    args["v"]           = m_apiVersion;
    args["session_key"] = m_sessionKey;
    args["call_id"]     = QString::number(m_callID.elapsed());
    args["uid"]         = QString::number(m_uid);
    QString md5 = getApiSig(args);
    args["sig"]         =  md5;

    QByteArray tmp(getCallString(args).toUtf8());
    KIO::TransferJob* job = KIO::http_post(m_apiURL, tmp, KIO::HideProgressInfo);
    job->addMetaData("UserAgent", m_userAgent);
    job->addMetaData("content-type",
                     "Content-Type: application/x-www-form-urlencoded");

    connect(job, SIGNAL(data(KIO::Job*, const QByteArray&)),
            this, SLOT(data(KIO::Job*, const QByteArray&)));

    connect(job, SIGNAL(result(KJob*)),
            this, SLOT(slotResult(KJob*)));

    m_state = FB_LISTALBUMS;
    m_job   = job;
    m_buffer.resize(0);
}


void FbTalker::createAlbum(const FbAlbum& album)
{
    if (m_job)
    {
        m_job->kill();
        m_job = 0;
    }
    emit signalBusy(true);

    QMap<QString, QString> args;
    args["method"]      = "facebook.photos.createAlbum";
    args["api_key"]     = m_apiKey;
    args["v"]           = m_apiVersion;
    args["session_key"] = m_sessionKey;
    args["name"]        = album.title;
    if (!album.location.isEmpty())
        args["location"] = album.location;
    if (!album.description.isEmpty())
        args["description"] = album.description;
    switch (album.privacy)
    {
        case FB_FRIENDS:
            args["visible"] = "friends";
            break;
        case FB_FRIENDS_OF_FRIENDS:
            args["visible"] = "friends-of-friends";
            break;
        case FB_NETWORKS:
            args["visible"] = "networks";
            break;
        case FB_EVERYONE:
            args["visible"] = "everyone";
            break;
    }
    QString md5 = getApiSig(args);
    args["sig"]         =  md5;

    QByteArray tmp(getCallString(args).toUtf8());
    KIO::TransferJob* job = KIO::http_post(m_apiURL, tmp, KIO::HideProgressInfo);
    job->addMetaData("UserAgent", m_userAgent);
    job->addMetaData("content-type",
                     "Content-Type: application/x-www-form-urlencoded");

    connect(job, SIGNAL(data(KIO::Job*, const QByteArray&)),
            this, SLOT(data(KIO::Job*, const QByteArray&)));

    connect(job, SIGNAL(result(KJob*)),
            this, SLOT(slotResult(KJob*)));

    m_state = FB_CREATEALBUM;
    m_job   = job;
    m_buffer.resize(0);
}

bool FbTalker::addPhoto(const QString& imgPath, long long albumID)
{
    if (m_job)
    {
        m_job->kill();
        m_job = 0;
    }
    emit signalBusy(true);

    QMap<QString, QString> args;
    args["method"]      = "facebook.photos.upload";
    args["api_key"]     = m_apiKey;
    args["v"]           = m_apiVersion;
    args["call_id"]     = QString::number(m_callID.elapsed());
    args["session_key"] = m_sessionKey;
    args["name"]        = KUrl(imgPath).fileName();
    if (albumID != -1)
        args["aid"]     = QString::number(albumID);
    QString md5 = getApiSig(args);
    args["sig"]         =  md5;

    MPForm  form;
    for (QMap<QString, QString>::const_iterator it = args.constBegin(); 
         it != args.constEnd(); 
         ++it)
    {
        form.addPair(it.key(), it.value());
    }

    if (!form.addFile(args["name"], imgPath)) 
    {
        emit signalBusy(false);
        return false;
    }
    form.finish();


    kDebug(51000) << "FORM: " << endl << form.formData();

    KIO::TransferJob* job = KIO::http_post(m_apiURL, form.formData(), KIO::HideProgressInfo);
    job->addMetaData("UserAgent", m_userAgent);
    job->addMetaData("content-type", form.contentType());

    connect(job, SIGNAL(data(KIO::Job*, const QByteArray&)),
            this, SLOT(data(KIO::Job*, const QByteArray&)));

    connect(job, SIGNAL(result(KJob *)),
            this, SLOT(slotResult(KJob *)));

    m_state = FB_ADDPHOTO;
    m_job   = job;
    m_buffer.resize(0);
    return true;
}

void FbTalker::data(KIO::Job*, const QByteArray& data)
{
    if (data.isEmpty())
        return;

    int oldSize = m_buffer.size();
    m_buffer.resize(m_buffer.size() + data.size());
    memcpy(m_buffer.data()+oldSize, data.data(), data.size());
}

QString FbTalker::errorToText(int errCode, const QString &errMsg)
{
    QString transError;
    kDebug(51000) << "errorToText: " << errCode << ": " << errMsg;

    switch (errCode)
    {
        case 0:
            transError = "";
            break;
        case 1:
            transError = i18n("Login failed");
            break;
        case 18:
            transError = i18n("Invalid API key");
            break;
        default:
            transError = errMsg;
            break;
    }
    return transError;
}

void FbTalker::slotResult(KJob *kjob)
{
    m_job = 0;
    KIO::Job *job = static_cast<KIO::Job*>(kjob);

    if (job->error())
    {
        if (m_state == FB_CREATETOKEN)
        {
            m_authProgressDlg->hide();
        }

        if (m_state == FB_ADDPHOTO)
        {
            // TODO: should we implement similar for all?
            //emit signalAddPhotoFailed(job->errorString());
        }
        else
        {
            job->ui()->setWindow(m_parent);
            job->ui()->showErrorMessage();
        }
        emit signalBusy(false);
        return;
    }

    switch(m_state)
    {
        case(FB_CREATETOKEN):
            parseResponseCreateToken(m_buffer);
            break;
        case(FB_GETSESSION):
            parseResponseGetSession(m_buffer);
            break;
        case(FB_GETUSERINFO):
            parseResponseGetUserInfo(m_buffer);
            break;
        case(FB_LOGOUT):
            parseResponseLogout(m_buffer);
            break;
        case(FB_LISTALBUMS):
            parseResponseListAlbums(m_buffer);
            break;
        case(FB_CREATEALBUM):
            parseResponseCreateAlbum(m_buffer);
            break;
        case(FB_ADDPHOTO):
            parseResponseAddPhoto(m_buffer);
            break;
    }
}

int FbTalker::parseErrorResponse(const QDomElement& e, QString& errMsg)
{
    int errCode = -1;

    for (QDomNode node = e.firstChild();
         !node.isNull();
         node = node.nextSibling())
    {
        if (!node.isElement())
            continue;
        if (node.nodeName() == "error_code")
        {
            errCode = node.toElement().text().toInt();
            kDebug(51000) << "Error Code:" << errCode;
        }
        else if (node.nodeName() == "error_msg")
        {
            errMsg = node.toElement().text();
            kDebug(51000) << "Error Text:" << errMsg;
        }
    }
    return errCode;
}

void FbTalker::parseResponseCreateToken(const QByteArray& data)
{
    int errCode = -1;
    QString errMsg;

    QDomDocument doc("createToken");
    if (!doc.setContent(data))
        return;

    m_authProgressDlg->setValue(2);
    kDebug(51000) << "Parse CreateToken response:" << endl << data;

    QDomElement docElem = doc.documentElement();
    if (docElem.tagName() == "auth_createToken_response") 
    {
        m_authToken = docElem.text();
        errCode = 0;
    }
    else if (docElem.tagName() == "error_response")
        errCode = parseErrorResponse(docElem, errMsg);

    kDebug(51000) << "CreateToken finished";

    if (errCode != 0) // if login failed, reset user properties
    {
        m_authToken.clear();
        m_sessionKey.clear();
        m_sessionExpires = 0;
        m_uid = 0;
        m_userName.clear();
        m_userURL.clear();
        emit signalLoginDone(errCode, errorToText(errCode, errMsg));
        emit signalBusy(false);
        m_authProgressDlg->hide();
        return;
    }

    KUrl url("https://www.facebook.com/login.php");
    url.addQueryItem("api_key", m_apiKey);
    url.addQueryItem("v", m_apiVersion);
    url.addQueryItem("auth_token", m_authToken);
    kDebug( 51000 ) << "Login URL: " << url;
    KToolInvocation::invokeBrowser(url.url());

    int valueOk = KMessageBox::questionYesNo(kapp->activeWindow(),
                  i18n("Please follow the instructions in the browser window."
                       "Press Yes if you have authenticated and No if you failed."),
                  i18n("Facebook Service Web Authorization"));

    if (valueOk == KMessageBox::Yes)
    {
        getSession();
        emit signalBusy(true);
    }
    else
    {
        cancel();
    }
}

void FbTalker::parseResponseGetSession(const QByteArray& data)
{
    int errCode = -1;
    QString errMsg;

    QDomDocument doc("getSession");
    if (!doc.setContent(data))
        return;

    m_authProgressDlg->setValue(4);
    kDebug(51000) << "Parse GetSession response:" << endl << data;

    QDomElement docElem = doc.documentElement();
    if (docElem.tagName() == "auth_getSession_response") 
    {
        for (QDomNode node = docElem.firstChild();
             !node.isNull();
             node = node.nextSibling())
        {
            if (!node.isElement())
                continue;
            if (node.nodeName() == "session_key")
                m_sessionKey = node.toElement().text();
            else if (node.nodeName() == "uid")
                m_uid = node.toElement().text().toInt();
            else if (node.nodeName() == "expires")
                m_sessionExpires = node.toElement().text().toInt();
        }
        errCode = 0;
    }
    else if (docElem.tagName() == "error_response")
        errCode = parseErrorResponse(docElem, errMsg);

    kDebug(51000) << "GetSession finished";

    if (errCode != 0) // if login failed, reset user properties
    {
        m_authToken.clear();
        m_sessionKey.clear();
        m_sessionExpires = 0;
        m_uid = 0;
        m_userName.clear();
        m_userURL.clear();
        return;
    }

    // reset call_id counter
    m_callID.start();

    getUserInfo();
}

void FbTalker::parseResponseGetUserInfo(const QByteArray& data)
{
    int errCode = -1;
    QString errMsg;
    QDomDocument doc("getUserInfo");
    if (!doc.setContent(data))
        return;

    m_authProgressDlg->setValue(6);
    kDebug(51000) << "Parse GetUserInfo response:" << endl << data;

    QDomElement docElem = doc.documentElement();
    if (docElem.tagName() == "users_getStandardInfo_response") 
    {
        for (QDomNode node = docElem.firstChild();
             !node.isNull();
             node = node.nextSibling())
        {
            if (!node.isElement())
                continue;
            if (node.nodeName() == "standard_user_info")
            {
                FbAlbum album;
                for (QDomNode nodeU = node.toElement().firstChild();
                     !nodeU.isNull();
                     nodeU = nodeU.nextSibling())
                {
                    if (!nodeU.isElement())
                        continue;
                    if (nodeU.nodeName() == "name")
                        m_userName = nodeU.toElement().text();
                    else if (nodeU.nodeName() == "profile_url")
                        m_userURL = nodeU.toElement().text();
                }
            }
        }
        errCode = 0;
    }
    else if (docElem.tagName() == "error_response")
        errCode = parseErrorResponse(docElem, errMsg);

    kDebug(51000) << "GetUserInfo finished";

    emit signalLoginDone(errCode, errorToText(errCode, errMsg));
    emit signalBusy(false);
    m_authProgressDlg->hide();
}

void FbTalker::parseResponseLogout(const QByteArray& data)
{
    int errCode = -1;
    QString errMsg;

    QDomDocument doc("expireSession");
    if (!doc.setContent(data))
        return;

    kDebug(51000) << "Parse ExpireSession response:" << endl << data;

    QDomElement docElem = doc.documentElement();
    if (docElem.tagName() == "auth_expireSession_response ") 
    {
        errCode = 0;
    }
    else if (docElem.tagName() == "error_response")
        errCode = parseErrorResponse(docElem, errMsg);

    // consider we are logged out in any case
    m_sessionKey.clear();
    m_sessionExpires = 0;
    m_uid = 0;
    m_userName.clear();
    m_userURL.clear();

    kDebug(51000) << "Logout finished";

    emit signalBusy(false);
}

void FbTalker::parseResponseAddPhoto(const QByteArray& data)
{
    int errCode = -1;
    QString errMsg;

    QDomDocument doc("addphoto");
    if (!doc.setContent(data))
        return;

    kDebug(51000) << "Parse Add Photo response:" << endl << data;

    QDomElement docElem = doc.documentElement();
    if (docElem.tagName() == "photos_upload_response") 
    {
        for (QDomNode node = docElem.firstChild();
             !node.isNull();
             node = node.nextSibling())
        {
            if (!node.isElement())
                continue;
            
            kDebug(51000) << "NEW: " << node.nodeName() 
                          << "=" << node.toElement().text();
        }
        errCode = 0;
    }
    else if (docElem.tagName() == "error_response")
        errCode = parseErrorResponse(docElem, errMsg);

    kDebug(51000) << "Add Photo finished";

    emit signalAddPhotoDone(errCode, errorToText(errCode, errMsg));
    emit signalBusy(false);
}

void FbTalker::parseResponseCreateAlbum(const QByteArray& data)
{
    int errCode = -1;
    QString errMsg;
    QDomDocument doc("createalbum");
    if (!doc.setContent(data))
        return;

    kDebug(51000) << "Parse Create Album response:" << endl << data;

    long long newAlbumID = -1;
    QDomElement docElem = doc.documentElement();
    if (docElem.tagName() == "photos_createAlbum_response") 
    {
        for (QDomNode node = docElem.firstChild();
             !node.isNull();
             node = node.nextSibling())
        {
            if (!node.isElement())
                continue;
            if (node.nodeName() == "aid")
            {
                newAlbumID = node.toElement().text().toLongLong();
                kDebug(51000) << "newAID: " << newAlbumID;
            }
        }
        errCode = 0;
    }
    else if (docElem.tagName() == "error_response")
        errCode = parseErrorResponse(docElem, errMsg);

    kDebug(51000) << "Create Album finished";

    emit signalCreateAlbumDone(errCode, errorToText(errCode, errMsg),
                               newAlbumID);
    emit signalBusy(false);
}

void FbTalker::parseResponseListAlbums(const QByteArray& data)
{
    int errCode = -1;
    QString errMsg;
    QDomDocument doc("getAlbums");
    if (!doc.setContent(data))
        return;

    kDebug(51000) << "Parse Albums response:" << endl << data;

    QDomElement docElem = doc.documentElement();
    QList <FbAlbum> albumsList;
    if (docElem.tagName() == "photos_getAlbums_response") 
    {
        for (QDomNode node = docElem.firstChild();
             !node.isNull();
             node = node.nextSibling())
        {
            if (!node.isElement())
                continue;
            if (node.nodeName() == "album")
            {
                FbAlbum album;
                for (QDomNode nodeA = node.toElement().firstChild();
                     !nodeA.isNull();
                     nodeA = nodeA.nextSibling())
                {
                    if (!nodeA.isElement())
                        continue;
                    if (nodeA.nodeName() == "aid")
                        album.id = nodeA.toElement().text().toLongLong();
                    else if (nodeA.nodeName() == "name")
                        album.title = nodeA.toElement().text();
                    else if (nodeA.nodeName() == "description")
                        album.description = nodeA.toElement().text();
                    else if (nodeA.nodeName() == "location")
                        album.location = nodeA.toElement().text();
                    else if (nodeA.nodeName() == "link")
                        album.url = nodeA.toElement().text();
                    else if (nodeA.nodeName() == "visible")
                    {
                        if (nodeA.toElement().text() == "friends")
                            album.privacy = FB_FRIENDS;
                        else if (nodeA.toElement().text() == "friends-of-friends")
                            album.privacy = FB_FRIENDS_OF_FRIENDS;
                        else if (nodeA.toElement().text() == "networks")
                            album.privacy = FB_NETWORKS;
                        else if (nodeA.toElement().text() == "everyone")
                            album.privacy = FB_EVERYONE;
                    }   
                }
                kDebug(51000) << "AID: " << album.id;
                albumsList.append(album);
            }
        }
        errCode = 0;
    }
    else if (docElem.tagName() == "error_response")
        errCode = parseErrorResponse(docElem, errMsg);

    kDebug(51000) << "List Albums finished";

    emit signalListAlbumsDone(errCode, errorToText(errCode, errMsg),
                              albumsList);
    emit signalBusy(false);
}

} // namespace KIPIFbExportPlugin
