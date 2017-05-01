/** ===========================================================
 * @file
 *
 * This file is a part of kipi-plugins project
 * <a href="http://www.digikam.org">http://www.digikam.org</a>
 *
 * @date   2017-04-29
 * @brief  A QFrame that automatically adds added child widgets to its layout manager.
 *
 * @author Copyright (C) 2017 by Matthias Fuessel
 *         <a href="mailto:matthias.fuessel@mailbox.org">matthias.fuessel@mailbox.org</a>
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

#include "framewithlayout.h"

// Qt includes


namespace KIPIGeolocatorPlugin
{

FrameWithLayout::FrameWithLayout(QWidget* const parent)
	:QFrame(parent)
{
}

FrameWithLayout::~FrameWithLayout()
{
}

void FrameWithLayout::childEvent(QChildEvent* e)
{
    switch (e->type())
    {
        case QEvent::ChildAdded:
        {
            if (layout() && e->child()->isWidgetType())
            {
                layout()->addWidget(qobject_cast<QWidget*>(e->child()));
            }
            break;
        }

        case QEvent::ChildRemoved:
        {
            if (layout() && e->child()->isWidgetType())
            {
                layout()->removeWidget(qobject_cast<QWidget*>(e->child()));
            }
            break;
        }
        default:
        	break;
    }

    QFrame::childEvent(e);
}

}  // namespace KIPIGeolocatorPlugin
