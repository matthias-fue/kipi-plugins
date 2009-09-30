/* ============================================================
 *
 * This file is a part of kipi-plugins project
 * http://www.kipi-plugins.org
 *
 * Date        : 2009-09-28
 * Description : a tool to export image to a KIO accessible
 *               location
 *
 * Copyright (C) 2006-2009 by Johannes Wienke <languitar at semipol dot de>
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

#include "KioExportWindow.h"
#include "KioExportWindow.moc"
// Qt includes

#include <QCloseEvent>

// KDE includes

#include <kdebug.h>
#include <kconfig.h>
#include <klocale.h>
#include <kpushbutton.h>
#include <kmenu.h>
#include <kmessagebox.h>
#include <kpushbutton.h>
#include <kurl.h>
#include <kio/copyjob.h>
#include <khelpmenu.h>
#include <ktoolinvocation.h>

// Local includes

#include "kpaboutdata.h"
#include "pluginsversion.h"
#include "imageslist.h"
#include "KioExportWidget.h"

namespace KIPIKioExportPlugin
{

const QString KioExportWindow::TARGET_URL_PROPERTY = "targetUrl";
const QString KioExportWindow::CONFIG_GROUP        = "KioExport";

KioExportWindow::KioExportWindow(QWidget *parent, KIPI::Interface *interface)
               : KDialog(parent), m_interface(interface)
{
    if (!interface)
    {
        kFatal(51000) << "KIPI::Interface is empty";
    }

    m_exportWidget = new KioExportWidget(this, interface);
    setMainWidget(m_exportWidget);

    // -- Window setup ------------------------------------------------------

    setWindowTitle(i18n("Export using KIO"));
    setDefaultButton(Ok);
    setButtons(Help | User1 | Close);
    setDefaultButton(Close);
    setModal(false);

    setButtonGuiItem(User1, KGuiItem(i18n("Start export"), "network-workgroup",
                     i18n("Start export to the specified target")));

    connect(this, SIGNAL(user1Clicked()), 
            this, SLOT(slotUpload()));

    connect(m_exportWidget->imageList(), SIGNAL(signalImageListChanged()),
            this, SLOT(slotImageListChanged()));

    connect(m_exportWidget, SIGNAL(signalTargetUrlChanged(KUrl)),
            this, SLOT(slotTargetUrlChanged(KUrl)));

    // -- About data and help button ----------------------------------------

    m_about = new KIPIPlugins::KPAboutData(ki18n("Export using KIO"),
                   0,
                   KAboutData::License_GPL,
                   ki18n("A Kipi plugin to export images over network"),
                   ki18n("(c) 2009, Johannes Wienke"));

    m_about->addAuthor(ki18n("Johannes Wienke"),
                       ki18n("Developer and maintainer"),
                       "languitar at semipol dot de");

    disconnect(this, SIGNAL(helpClicked()),
               this, SLOT(slotHelp()));

    KHelpMenu* helpMenu = new KHelpMenu(this, m_about, false);
    helpMenu->menu()->removeAction(helpMenu->menu()->actions().first());
    QAction *handbook   = new QAction(i18n("Handbook"), this);
    connect(handbook, SIGNAL(triggered(bool)),
            this, SLOT(slotHelp()));
    helpMenu->menu()->insertAction(helpMenu->menu()->actions().first(), handbook);
    button(Help)->setMenu(helpMenu->menu());

    // -- initial sync ------------------------------------------------------

    restoreSettings();
    updateUploadButton();
}

KioExportWindow::~KioExportWindow()
{
    delete m_about;
}

void KioExportWindow::closeEvent(QCloseEvent *e)
{
    if (!e)
    {
        return;
    }

    saveSettings();
    e->accept();
}

void KioExportWindow::restoreSettings()
{
    kDebug(51000) << "restoring settings";

    KConfigGroup group = KGlobal::config()->group(CONFIG_GROUP);
    m_exportWidget->setTargetUrl(group.readEntry(TARGET_URL_PROPERTY, ""));

    kDebug(51000) << "target url after restoring: "
                  << m_exportWidget->targetUrl().prettyUrl();
}

void KioExportWindow::saveSettings()
{
    kDebug(51000) << "saving settings";

    KConfigGroup group = KGlobal::config()->group(CONFIG_GROUP);
    group.writeEntry(TARGET_URL_PROPERTY, m_exportWidget->targetUrl().url());

    kDebug(51000) << "stored target url "
                  << m_exportWidget->targetUrl().prettyUrl();
}

void KioExportWindow::slotHelp()
{
    KToolInvocation::invokeHelp("kioexport", "kipi-plugins");
}

void KioExportWindow::slotImageListChanged()
{
    updateUploadButton();
}

void KioExportWindow::slotTargetUrlChanged(KUrl target)
{
    Q_UNUSED(target);
    updateUploadButton();
}

void KioExportWindow::updateUploadButton()
{
    bool listNotEmpty = !m_exportWidget->imageList()->imageUrls().empty();
    enableButton(User1, listNotEmpty && m_exportWidget->targetUrl().isValid());

    kDebug(51000) << "Updated upload button with listNotEmpty = "
                  << listNotEmpty << ", targetUrl().isValid() = "
                  << m_exportWidget->targetUrl().isValid();
}

void KioExportWindow::slotCopyingDone(KIO::Job *job, const KUrl& from,
                                      const KUrl& to, time_t mtime, bool directory, bool renamed)
{
    Q_UNUSED(job);
    Q_UNUSED(to);
    Q_UNUSED(mtime);
    Q_UNUSED(directory);
    Q_UNUSED(renamed);

    kDebug(51000) << "copied " << to.prettyUrl();

    m_exportWidget->imageList()->removeItemByUrl(from);
}

void KioExportWindow::slotCopyingFinished(KJob *job)
{
    Q_UNUSED(job);

    setEnabled(true);

    if (!m_exportWidget->imageList()->imageUrls().empty())
    {
        KMessageBox::sorry(this, i18n(
                           "Some of the images have not been transferred "
                           "and are still in the list. "
                           "You can retry to export these images now."), i18n(
                           "Upload not completed"));
    }
}

void KioExportWindow::slotUpload()
{
    // start copying and react on signals
    setEnabled(false);
    KIO::CopyJob *copyJob = KIO::copy(m_exportWidget->imageList()->imageUrls(),
                            m_exportWidget->targetUrl());

    connect(copyJob, SIGNAL(copyingDone(KIO::Job*, const KUrl&, const KUrl&, time_t, bool, bool)),
            this, SLOT(slotCopyingDone(KIO::Job*, const KUrl&, const KUrl&, time_t, bool, bool)));

    connect(copyJob, SIGNAL(result(KJob*)),
            this, SLOT(slotCopyingFinished(KJob*)));
}

} // namespace KIPIKioExportPlugin
