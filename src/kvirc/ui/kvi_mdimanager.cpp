//=============================================================================
//
//   File : kvi_mdimanager.cpp
//   Creation date : Wed Jun 21 2000 17:28:04 by Szymon Stefanek
//
//   This file is part of the KVirc irc client distribution
//   Copyright (C) 2000-2003 Szymon Stefanek (pragma at kvirc dot net)
//
//   This program is FREE software. You can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your opinion) any later version.
//
//   This program is distributed in the HOPE that it will be USEFUL,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//   See the GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program. If not, write to the Free Software Foundation,
//   Inc. ,59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//=============================================================================

#include "kvi_debug.h"
#include "kvi_settings.h"
#include "kvi_mdimanager.h"
#include "kvi_mdichild.h"
#include "kvi_locale.h"
#include "kvi_options.h"
#include "kvi_iconmanager.h"
#include "kvi_frame.h"
#include "kvi_menubar.h"
//#include "kvi_mdicaption.h"
#include "kvi_app.h"
#include "kvi_tal_popupmenu.h"

#include <QMenuBar>
#include <QLayout>
#include <QPainter>
#include <QCursor>
#include <QEvent>
//#include <qdrawutil.h>

#include <math.h>

#ifdef COMPILE_PSEUDO_TRANSPARENCY
	#include <QPixmap>
	extern QPixmap * g_pShadedParentGlobalDesktopBackground;
#endif

#if 0
KviMdiManager::KviMdiManager(QWidget * parent,KviFrame * pFrm,const char * name)
: QMdiArea(parent)
{
	setFrameShape(NoFrame);
	m_pZ = new QList<KviMdiChild*>;

	m_pFrm = pFrm;

	m_iSdiIconItemId = 0;
	m_iSdiCloseItemId = 0;
	m_iSdiRestoreItemId = 0;
	m_iSdiMinimizeItemId = 0;
	m_pSdiIconButton = 0;
	m_pSdiCloseButton = 0;
	m_pSdiRestoreButton = 0;
	m_pSdiMinimizeButton = 0;
	m_pSdiControls = 0;

	m_pWindowPopup = new KviTalPopupMenu(this);
	connect(m_pWindowPopup,SIGNAL(triggered(QAction*)),this,SLOT(menuActivated(QAction*)));
	connect(m_pWindowPopup,SIGNAL(aboutToShow()),this,SLOT(fillWindowPopup()));
	m_pTileMethodPopup = new KviTalPopupMenu(this);
	connect(m_pTileMethodPopup,SIGNAL(triggered(QAction*)),this,SLOT(tileMethodMenuActivated(QAction*)));

	/*viewport()->setAutoFillBackground(false);

	setStaticBackground(true);
	resizeContents(width(),height());

	setFocusPolicy(Qt::NoFocus);
	viewport()->setFocusPolicy(Qt::NoFocus);*/
	
	connect(g_pApp,SIGNAL(reloadImages()),this,SLOT(reloadImages()));
}

KviMdiManager::~KviMdiManager()
{
	foreach(KviMdiChild* c, *m_pZ)
	{
		delete c;
	}
	delete m_pZ;
}

void KviMdiManager::reloadImages()
{
	foreach(KviMdiChild* c, *m_pZ)
	{
		c->reloadImages();
	}
}

bool KviMdiManager::focusNextPrevChild(bool bNext)
{
	//bug("FFFFFF");
	// this is a QScrollView bug... it doesn't pass this
	// event to the toplevel window
	return m_pFrm->focusNextPrevChild(bNext);
}

void KviMdiManager::drawContents(QPainter *p,int x,int y,int w,int h)
{
	//debug("MY DRAW CONTENTS (%d,%d,%d,%d)",x,y,w,h);
	QRect r(x,y,w,h);

#ifdef COMPILE_PSEUDO_TRANSPARENCY
	if(g_pShadedParentGlobalDesktopBackground)
	{
		QPoint pnt = viewport()->mapToGlobal(contentsToViewport(r.topLeft()));
		p->drawTiledPixmap(r,*(g_pShadedParentGlobalDesktopBackground),pnt);
	}
#endif

	if(KVI_OPTION_PIXMAP(KviOption_pixmapMdiBackground).pixmap())
	{
		p->drawTiledPixmap(r,*(KVI_OPTION_PIXMAP(KviOption_pixmapMdiBackground).pixmap()));
	} else {
		p->fillRect(r,KVI_OPTION_COLOR(KviOption_colorMdiBackground));
	}
}


void KviMdiManager::showAndActivate(KviMdiChild * lpC)
{
	lpC->show();
	setTopChild(lpC,true);
	if(KVI_OPTION_BOOL(KviOption_boolAutoTileWindows))tile();
}

void KviMdiManager::setTopChild(KviMdiChild *lpC,bool bSetFocus)
{
	__range_valid(lpC);
	// The following check fails safely at startup....
	//	__range_valid(lpC->isVisible() || lpC->testWState(WState_ForceHide));

	KviMdiChild * pOldTop = m_pZ->last();
	if(pOldTop != lpC)
	{

		if(!m_pZ->removeAll(lpC))return; // no such child ?

		// disable the labels of all the other children
		//for(KviMdiChild *pC=m_pZ->first();pC;pC=m_pZ->next())
		//{
		//	pC->captionLabel()->setActive(false);
		//}
		KviMdiChild * pMaximizedChild = pOldTop;
		if(pOldTop)
		{
			pOldTop->captionLabel()->setActive(false);
			if(pOldTop->m_state != KviMdiChild::Maximized)pMaximizedChild=0;
		}
		
		m_pZ->append(lpC);

		if(pMaximizedChild)lpC->maximize(); //do not animate the change	
		lpC->raise();
		if(pMaximizedChild)pMaximizedChild->restore();
	}

	if(bSetFocus)
	{
		if(!lpC->hasFocus())
		{
			lpC->setFocus();
			/*
			if(topLevelWidget()->isActiveWindow())
			{
				
			}
			*/
		}
	}
}

void KviMdiManager::focusInEvent(QFocusEvent *)
{
	focusTopChild();
}

void KviMdiManager::destroyChild(KviMdiChild *lpC,bool bFocusTopChild)
{
	bool bWasMaximized = lpC->state() == KviMdiChild::Maximized;
	disconnect(lpC);
	lpC->blockSignals(true);
	m_pZ->removeAll(lpC);
	delete lpC;

	if(bWasMaximized)
	{
		KviMdiChild * c=topChild();
		if(c)
		{
			if(c->state() != KviMdiChild::Minimized)c->maximize();
			else {
				// minimized top child...the last one
				leaveSDIMode();
			}
		} else {
			// SDI state change
			leaveSDIMode();
		}
	}
	if(bFocusTopChild)focusTopChild();

	if(KVI_OPTION_BOOL(KviOption_boolAutoTileWindows))tile();
	updateContentsSize();
}

KviMdiChild * KviMdiManager::highestChildExcluding(KviMdiChild * pChild)
{
	QListIterator<KviMdiChild *> it(*m_pZ);
	it.toBack();
	KviMdiChild * c;
	while(it.hasPrevious() && (it.peekPrevious() == pChild))c = it.previous();
	return c;
}

QPoint KviMdiManager::getCascadePoint(int indexOfWindow)
{
	QPoint pnt(0,0);
	if(indexOfWindow==0)return pnt;
	KviMdiChild *lpC=m_pZ->first();
	int step=(lpC ? (lpC->captionLabel()->heightHint()+KVI_MDICHILD_BORDER) : 20);
	int availableHeight=viewport()->height()-(lpC ? lpC->minimumSize().height() : KVI_MDICHILD_MIN_HEIGHT);
	int availableWidth=viewport()->width()-(lpC ? lpC->minimumSize().width() : KVI_MDICHILD_MIN_WIDTH);
	int ax=0;
	int ay=0;
	for(int i=0;i<indexOfWindow;i++)
	{
		ax+=step;
		ay+=step;
		if(ax>availableWidth)ax=0;
		if(ay>availableHeight)ay=0;
	}
	pnt.setX(ax);
	pnt.setY(ay);
	return pnt;
}

void KviMdiManager::mousePressEvent(QMouseEvent *e)
{
	//Popup the window menu
	if(e->button() & Qt::RightButton)m_pWindowPopup->popup(mapToGlobal(e->pos()));
}

void KviMdiManager::childMoved(KviMdiChild *)
{
	updateContentsSize();
}

void KviMdiManager::maximizeChild(KviMdiChild * lpC)
{
	// the children must be moved once by the means of QScrollView::moveChild()
	// so the QScrollView internal structures get updated with the negative
	// position of the widget, otherwise, when restoring with moveChild()
	// it will refuse to move it back to the original position
	resizeContents(visibleWidth(),visibleHeight());
	updateScrollBars();
	g_pApp->sendPostedEvents();
	if(g_pApp->closingDown())return;

	moveChild(lpC,-KVI_MDICHILD_HIDDEN_EDGE,
		-(KVI_MDICHILD_HIDDEN_EDGE + KVI_MDICHILD_SPACING + lpC->m_pCaption->heightHint()));

	lpC->setGeometry(
		-KVI_MDICHILD_HIDDEN_EDGE,
		-(KVI_MDICHILD_HIDDEN_EDGE + KVI_MDICHILD_SPACING + lpC->m_pCaption->heightHint()),
		viewport()->width() + (KVI_MDICHILD_HIDDEN_EDGE * 2), //KVI_MDICHILD_DOUBLE_HIDDEN_EDGE,
		viewport()->height() + (KVI_MDICHILD_HIDDEN_EDGE * 2) + lpC->m_pCaption->heightHint() + KVI_MDICHILD_SPACING);

	if(isInSDIMode())updateSDIMode();
	else {
		enterSDIMode(lpC);
		// make sure that the child is focused
		lpC->setFocus();
	}
	
	// fixme: we could hide all the other children now!
}



void KviMdiManager::resizeEvent(QResizeEvent *e)
{
	//If we have a maximized children at the top , adjust its size
	KviTalScrollView::resizeEvent(e);
	KviMdiChild *lpC=m_pZ->last();
	if(lpC)
	{
		if(lpC->state()==KviMdiChild::Maximized)
		{
			// SDI mode
			lpC->resize(viewport()->width() + (KVI_MDICHILD_HIDDEN_EDGE * 2),
				viewport()->height() + lpC->m_pCaption->heightHint() + (KVI_MDICHILD_HIDDEN_EDGE * 2) + KVI_MDICHILD_SPACING);
			return;
		} else {
			if(KVI_OPTION_BOOL(KviOption_boolAutoTileWindows))tile();
		}
	}
	updateContentsSize();
}


/*
void KviMdiManager::childMaximized(KviMdiChild * lpC)
{
	if(lpC == m_pZ->last())
	{
		enterSDIMode(lpC);
	}
	updateContentsSize();
}
*/

void KviMdiManager::childMinimized(KviMdiChild * lpC,bool bWasMaximized)
{
	__range_valid(lpC);
	if(!m_pZ->contains(lpC))return;
	if(m_pZ->count() > 1)
	{
		m_pZ->removeAll(lpC);
		m_pZ->prepend(lpC);
		if(bWasMaximized)
		{
			// Need to maximize the top child
			lpC = m_pZ->last();
			if(!lpC)return; //??
			if(lpC->state()==KviMdiChild::Minimized)
			{
				if(bWasMaximized)leaveSDIMode();
				return;
			}
			lpC->maximize(); //do nrot animate the change
		} else {
			if(KVI_OPTION_BOOL(KviOption_boolAutoTileWindows))tile();
		}
		focusTopChild();
	} else {
		// Unique window minimized...it won't loose the focus...!!
		setFocus(); //Remove focus from the child
		if(bWasMaximized)leaveSDIMode();
	}
	updateContentsSize();
}

void KviMdiManager::childRestored(KviMdiChild * lpC,bool bWasMaximized)
{
	if(bWasMaximized)
	{
		if(lpC != topChild())return; // do nothing in this case
		leaveSDIMode();
		updateContentsSize();
	}
	if(KVI_OPTION_BOOL(KviOption_boolAutoTileWindows))tile();
}

void KviMdiManager::focusTopChild()
{
	KviMdiChild *lpC=topChild();
	if(!lpC)return;
	if(!lpC->isVisible())return;
	//	if(lpC->state()==KviMdiChild::Minimized)return;
	//	debug("Focusing top child %s",lpC->name());
	//disable the labels of all the other children
	foreach(KviMdiChild* pC, *m_pZ)
	{
		if(pC != lpC)pC->captionLabel()->setActive(false);
	}
	lpC->raise();
	if(!lpC->hasFocus())lpC->setFocus();
}

void KviMdiManager::minimizeActiveChild()
{
	KviMdiChild * lpC = topChild();
	if(!lpC)return;
	if(lpC->state() != KviMdiChild::Minimized)lpC->minimize();
}

void KviMdiManager::restoreActiveChild()
{
	KviMdiChild * lpC = topChild();
	if(!lpC)return;
	if(lpC->state() == KviMdiChild::Maximized)lpC->restore();
}

void KviMdiManager::closeActiveChild()
{
	KviMdiChild * lpC = topChild();
	if(!lpC)return;
	lpC->closeRequest();
}

void KviMdiManager::updateContentsSize()
{
	KviMdiChild * c = topChild();
	if(c)
	{
		if(c->state() == KviMdiChild::Maximized)
		{
			return;
		}
	}

	int fw = frameWidth() * 2;
	int mx = width() - fw;
	int my = height() - fw;

	foreach(KviMdiChild* c, *m_pZ)
	{
		if(c->isVisible())
		{
			int x = childX(c) + c->width();
			if(x > mx)mx = x;
			int y = childY(c) + c->height();
			if(y > my)my = y;
		}
	}

	resizeContents(mx,my);
}


void KviMdiManager::updateSDIMode()
{

	KviMdiChild * lpC = topChild();

	if(m_pSdiCloseButton)
		m_pSdiCloseButton->setEnabled(lpC ? lpC->closeEnabled() : false);

// This would result in an addictional main menu bar entry on MacOSX which would trigger a popup menu and not
// a submenu. Due to the optical reasons it is removed here.
// The same popup is triggered by right clicking on the window name in the channel window list.
#ifndef Q_OS_MACX
	KviMenuBar * b = m_pFrm->mainMenuBar();

	const QPixmap * pix = lpC ? lpC->icon() : 0;
	if(!pix)pix = g_pIconManager->getSmallIcon(KVI_SMALLICON_NONE);
	else if(pix->isNull())pix = g_pIconManager->getSmallIcon(KVI_SMALLICON_NONE);

	if(!m_pSdiIconButton)
	{
		m_pSdiIconButton = new KviMenuBarToolButton(b,*pix,"nonne");
		connect(m_pSdiIconButton,SIGNAL(clicked()),this,SLOT(activeChildSystemPopup()));
#ifdef COMPILE_USE_QT4
		// This is an obscure, undocumented and internal function in QT4 QMenuBar
		// I won't be surprised if this disappears....
		b->setCornerWidget(m_pSdiIconButton,Qt::TopLeftCorner);
		m_pSdiIconButton->show();
#else
		m_iSdiIconItemId = b->insertItem(m_pSdiIconButton,-1,0);
#endif
		connect(m_pSdiIconButton,SIGNAL(destroyed()),this,SLOT(sdiIconButtonDestroyed()));
	} else {
		m_pSdiIconButton->setPixmap(*pix);
	}
#endif //Q_OS_MACX
}

void KviMdiManager::activeChildSystemPopup()
{
	KviMdiChild * lpC = topChild();
	if(!lpC)return;
	QPoint pnt;
	if(m_pSdiIconButton)
	{
		pnt = m_pSdiIconButton->mapToGlobal(QPoint(0,m_pSdiIconButton->height()));
	} else {
		pnt = QCursor::pos();
	}
	lpC->emitSystemPopupRequest(pnt);
}

bool KviMdiManager::isInSDIMode()
{
	return (m_pSdiCloseButton != 0);
}


void KviMdiManager::enterSDIMode(KviMdiChild *lpC)
{
	if(!m_pSdiCloseButton)
	{
		KviMenuBar * b = m_pFrm->mainMenuBar();
	
		QWidget * pButtonParent;
	
#ifdef COMPILE_USE_QT4
		m_pSdiControls = new KviTalHBox(b);
		m_pSdiControls->setMargin(0);
		m_pSdiControls->setSpacing(2);
		m_pSdiControls->setAutoFillBackground(false);
		pButtonParent = m_pSdiControls;
#else
		pButtonParent = b;
#endif
		m_pSdiMinimizeButton = new KviMenuBarToolButton(pButtonParent,*(g_pIconManager->getSmallIcon(KVI_SMALLICON_MINIMIZE)),"btnminimize");
		connect(m_pSdiMinimizeButton,SIGNAL(clicked()),this,SLOT(minimizeActiveChild()));
#ifndef COMPILE_USE_QT4
		m_iSdiMinimizeItemId = b->insertItem(m_pSdiMinimizeButton,-1,b->count());
#endif
		connect(m_pSdiMinimizeButton,SIGNAL(destroyed()),this,SLOT(sdiMinimizeButtonDestroyed()));

		m_pSdiRestoreButton = new KviMenuBarToolButton(pButtonParent,*(g_pIconManager->getSmallIcon(KVI_SMALLICON_RESTORE)),"btnrestore");
		connect(m_pSdiRestoreButton,SIGNAL(clicked()),this,SLOT(restoreActiveChild()));
#ifndef COMPILE_USE_QT4
		m_iSdiRestoreItemId = b->insertItem(m_pSdiRestoreButton,-1,b->count());
#endif
		connect(m_pSdiRestoreButton,SIGNAL(destroyed()),this,SLOT(sdiRestoreButtonDestroyed()));

		m_pSdiCloseButton = new KviMenuBarToolButton(pButtonParent,*(g_pIconManager->getSmallIcon(KVI_SMALLICON_CLOSE)),"btnclose");
		connect(m_pSdiCloseButton,SIGNAL(clicked()),this,SLOT(closeActiveChild()));
#ifndef COMPILE_USE_QT4
		m_iSdiCloseItemId = b->insertItem(m_pSdiCloseButton,-1,b->count());
#endif
		connect(m_pSdiCloseButton,SIGNAL(destroyed()),this,SLOT(sdiCloseButtonDestroyed()));

#ifdef COMPILE_USE_QT4
		// This is an obscure, undocumented and internal function in QT4 QMenuBar
		// I won't be surprised if this disappears....
		b->setCornerWidget(m_pSdiControls,Qt::TopRightCorner);
		// The show below SHOULD force a re-layout of the menubar..
		// but it doesn't work when the KviFrame is still hidden (at startup)
		// We handle this BUG in showEvent()
		m_pSdiControls->show();
#else
		m_pSdiRestoreButton->show();
		m_pSdiMinimizeButton->show();
		m_pSdiCloseButton->show();
#endif
		emit enteredSdiMode();
		
		setVScrollBarMode(KviTalScrollView::AlwaysOff);
		setHScrollBarMode(KviTalScrollView::AlwaysOff);
	}

	updateSDIMode();
}
void KviMdiManager::relayoutMenuButtons()
{
#ifdef COMPILE_USE_QT4
	debug("mdi show");
	// force a re-layout of the menubar in Qt4 (see the note in enterSDIMode())
	// by resetting the corner widget
	if(m_pSdiControls)
	{
		m_pFrm->mainMenuBar()->setCornerWidget(0,Qt::TopRightCorner);
		m_pFrm->mainMenuBar()->setCornerWidget(m_pSdiControls,Qt::TopRightCorner);
	}
	// also force an activation of the top MdiChild since it probably didn't get it yet
	KviMdiChild * c = topChild();
	if(c)
		c->activate(false);
#endif
}


void KviMdiManager::sdiIconButtonDestroyed()
{
	m_iSdiIconItemId = 0;
	m_pSdiIconButton = 0;
}

void KviMdiManager::sdiMinimizeButtonDestroyed()
{
	m_iSdiMinimizeItemId = 0;
	m_pSdiMinimizeButton = 0;
}

void KviMdiManager::sdiRestoreButtonDestroyed()
{
	m_iSdiRestoreItemId = 0;
	m_pSdiRestoreButton = 0;
}

void KviMdiManager::sdiCloseButtonDestroyed()
{
	m_iSdiCloseItemId = 0;
	m_pSdiCloseButton = 0;
}

void KviMdiManager::leaveSDIMode()
{
	__range_valid(m_pSdiCloseButton);

#ifdef COMPILE_USE_QT4
	if(m_pSdiControls)
	{
		delete m_pSdiControls;
		m_pSdiControls = 0;
	}
	if(m_pSdiIconButton)
	{
		m_pSdiIconButton->hide(); // this will force a QMenuBar relayout
		delete m_pSdiIconButton;
		m_pSdiIconButton = 0;
	}
#else
	if(m_iSdiIconItemId != 0)m_pFrm->mainMenuBar()->removeItem(m_iSdiIconItemId);
	if(m_iSdiCloseItemId != 0)m_pFrm->mainMenuBar()->removeItem(m_iSdiCloseItemId);
	if(m_iSdiRestoreItemId != 0)m_pFrm->mainMenuBar()->removeItem(m_iSdiRestoreItemId);
	if(m_iSdiMinimizeItemId != 0)m_pFrm->mainMenuBar()->removeItem(m_iSdiMinimizeItemId);
#endif

	setVScrollBarMode(KviTalScrollView::Auto);
	setHScrollBarMode(KviTalScrollView::Auto);

	emit leftSdiMode();
}

#define KVI_TILE_METHOD_ANODINE 0
#define KVI_TILE_METHOD_PRAGMA4HOR 1
#define KVI_TILE_METHOD_PRAGMA4VER 2
#define KVI_TILE_METHOD_PRAGMA6HOR 3
#define KVI_TILE_METHOD_PRAGMA6VER 4
#define KVI_TILE_METHOD_PRAGMA9HOR 5
#define KVI_TILE_METHOD_PRAGMA9VER 6

#define KVI_NUM_TILE_METHODS 7

void KviMdiManager::fillWindowPopup()
{
	m_pWindowPopup->clear();

	m_pWindowPopup->insertItem(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_CASCADEWINDOWS)),(__tr2qs("&Cascade Windows")),this,SLOT(cascadeWindows()));
	m_pWindowPopup->insertItem(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_CASCADEWINDOWS)),(__tr2qs("Cascade &Maximized")),this,SLOT(cascadeMaximized()));

	m_pWindowPopup->insertSeparator();
	m_pWindowPopup->insertItem(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_TILEWINDOWS)),(__tr2qs("&Tile Windows")),this,SLOT(tile()));

	m_pTileMethodPopup->clear();
	QAction * action = m_pTileMethodPopup->insertItem(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_AUTOTILEWINDOWS)),(__tr2qs("&Auto Tile")),this,SLOT(toggleAutoTile()));
	action->setChecked(KVI_OPTION_BOOL(KviOption_boolAutoTileWindows));
	action->setData(QVariant(-1));
	m_pTileMethodPopup->insertSeparator();
	QAction * ids[KVI_NUM_TILE_METHODS];
	ids[KVI_TILE_METHOD_ANODINE] = m_pTileMethodPopup->insertItem(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_TILEWINDOWS)),(__tr2qs("Anodine's Full Grid")));
	ids[KVI_TILE_METHOD_ANODINE]->setData(QVariant(KVI_TILE_METHOD_ANODINE));
	ids[KVI_TILE_METHOD_PRAGMA4HOR] = m_pTileMethodPopup->insertItem(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_TILEWINDOWS)),(__tr2qs("Pragma's Horizontal 4-Grid")));
	ids[KVI_TILE_METHOD_PRAGMA4HOR]->setData(QVariant(KVI_TILE_METHOD_PRAGMA4HOR));
	ids[KVI_TILE_METHOD_PRAGMA4VER] = m_pTileMethodPopup->insertItem(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_TILEWINDOWS)),(__tr2qs("Pragma's Vertical 4-Grid")));
	ids[KVI_TILE_METHOD_PRAGMA4VER]->setData(QVariant(KVI_TILE_METHOD_PRAGMA4VER));
	ids[KVI_TILE_METHOD_PRAGMA6HOR] = m_pTileMethodPopup->insertItem(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_TILEWINDOWS)),(__tr2qs("Pragma's Horizontal 6-Grid")));
	ids[KVI_TILE_METHOD_PRAGMA6HOR]->setData(QVariant(KVI_TILE_METHOD_PRAGMA6HOR));
	ids[KVI_TILE_METHOD_PRAGMA6VER] = m_pTileMethodPopup->insertItem(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_TILEWINDOWS)),(__tr2qs("Pragma's Vertical 6-Grid")));
	ids[KVI_TILE_METHOD_PRAGMA6VER]->setData(QVariant(KVI_TILE_METHOD_PRAGMA6VER));
	ids[KVI_TILE_METHOD_PRAGMA9HOR] = m_pTileMethodPopup->insertItem(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_TILEWINDOWS)),(__tr2qs("Pragma's Horizontal 9-Grid")));
	ids[KVI_TILE_METHOD_PRAGMA9HOR]->setData(QVariant(KVI_TILE_METHOD_PRAGMA9HOR));
	ids[KVI_TILE_METHOD_PRAGMA9VER] = m_pTileMethodPopup->insertItem(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_TILEWINDOWS)),(__tr2qs("Pragma's Vertical 9-Grid")));
	ids[KVI_TILE_METHOD_PRAGMA9VER]->setData(QVariant(KVI_TILE_METHOD_PRAGMA9VER));

	if(KVI_OPTION_UINT(KviOption_uintTileMethod) >= KVI_NUM_TILE_METHODS)KVI_OPTION_UINT(KviOption_uintTileMethod) = KVI_TILE_METHOD_PRAGMA9HOR;
	ids[KVI_OPTION_UINT(KviOption_uintTileMethod)]->setChecked(true);

	m_pWindowPopup->insertItem(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_TILEWINDOWS)),(__tr2qs("Tile Met&hod")),m_pTileMethodPopup);

	m_pWindowPopup->insertSeparator();
	m_pWindowPopup->insertItem(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_MAXVERTICAL)),(__tr2qs("Expand &Vertically")),this,SLOT(expandVertical()));
	m_pWindowPopup->insertItem(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_MAXHORIZONTAL)),(__tr2qs("Expand &Horizontally")),this,SLOT(expandHorizontal()));

    m_pWindowPopup->insertSeparator();
	m_pWindowPopup->insertItem(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_MINIMIZE)),(__tr2qs("Mi&nimize All")),this,SLOT(minimizeAll()));
//    m_pWindowPopup->insertItem(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_RESTORE)),(__tr2qs("&Restore all")),this,SLOT(restoreAll()));
//
	m_pWindowPopup->insertSeparator();
	int i=100;
	QString szItem;
	QString szCaption;
	foreach(KviMdiChild* lpC, *m_pZ)
	{
		szItem.setNum(((uint)i)-99);
		szItem+=". ";

		szCaption = lpC->plainCaption();
		if(szCaption.length() > 30)
		{
			QString trail = szCaption.right(12);
			szCaption.truncate(12);
			szCaption+="...";
			szCaption+=trail;
		}

		if(lpC->state()==KviMdiChild::Minimized)
		{
			szItem+="(";
			szItem+=szCaption;
			szItem+=")";
		} else szItem+=szCaption;
		QAction * a;
		const QPixmap * pix = lpC->icon();
		if(pix && !(pix->isNull())) a = m_pWindowPopup->insertItem(*pix,szItem);
		else a = m_pWindowPopup->insertItem(szItem);
		a->setChecked(((uint)i)==(m_pZ->count()+99));
		a->setData(QVariant(i));
		i++;
	}
}

void KviMdiManager::menuActivated(QAction * action)
{
	bool ok = false;
	int id = action->data().toInt(&ok);
	if (!ok) return;
	if(id<100)return;
	id-=100;
	__range_valid(((uint)id) < m_pZ->count());
	KviMdiChild *lpC=m_pZ->value(id);
	if(!lpC)return;
	if(lpC->state()==KviMdiChild::Minimized)lpC->restore();
	setTopChild(lpC,true);
}

void KviMdiManager::ensureNoMaximized()
{
	KviMdiChild * lpC;

	foreach(KviMdiChild* lpC, *m_pZ)
	{
		if(lpC->state()==KviMdiChild::Maximized)lpC->restore();
	}
}

void KviMdiManager::tileMethodMenuActivated(QAction * action)
{
	bool ok = false;
	int idx = action->data().toInt(&ok);
	if (!ok) return;
	
	if(idx < 0)idx = 0;
	if(idx >= KVI_NUM_TILE_METHODS)idx = KVI_TILE_METHOD_PRAGMA9VER;
	KVI_OPTION_UINT(KviOption_uintTileMethod) = idx;
	if(KVI_OPTION_BOOL(KviOption_boolAutoTileWindows))tile();
}

void KviMdiManager::cascadeWindows()
{
	ensureNoMaximized();
	// this hack is needed to ensure that the scrollbars are hidden and the viewport()->width() and height() are correct
	resizeContents(visibleWidth(),visibleHeight());
	updateScrollBars();
	g_pApp->sendPostedEvents();
	if(g_pApp->closingDown())return;

	int idx=0;
	foreach(KviMdiChild* lpC, *m_pZ)
	{
		if(lpC->state() != KviMdiChild::Minimized)
		{
			QPoint p = getCascadePoint(idx);
			moveChild(lpC,p.x(),p.y());
			lpC->resize(lpC->sizeHint());
			idx++;
		}
	}
	focusTopChild();
	updateContentsSize();
}

void KviMdiManager::cascadeMaximized()
{
	ensureNoMaximized();
	// this hack is needed to ensure that the scrollbars are hidden and the viewport()->width() and height() are correct
	resizeContents(visibleWidth(),visibleHeight());
	updateScrollBars();
	g_pApp->sendPostedEvents();
	if(g_pApp->closingDown())return;
	
	int idx=0;

	foreach(KviMdiChild* lpC, *m_pZ)
	{
		if(lpC->state() != KviMdiChild::Minimized)
		{
			QPoint pnt(getCascadePoint(idx));
			moveChild(lpC,pnt.x(),pnt.y());
			QSize curSize(viewport()->width() - pnt.x(),viewport()->height() - pnt.y());
			if((lpC->minimumSize().width() > curSize.width()) ||
				(lpC->minimumSize().height() > curSize.height()))lpC->resize(lpC->minimumSize());
			else lpC->resize(curSize);
			idx++;
		}
	}
	focusTopChild();
	updateContentsSize();
}

void KviMdiManager::expandVertical()
{
	ensureNoMaximized();
	// this hack is needed to ensure that the scrollbars are hidden and the viewport()->width() and height() are correct
	resizeContents(visibleWidth(),visibleHeight());
	updateScrollBars();
	g_pApp->sendPostedEvents();
	if(g_pApp->closingDown())return;
	
	foreach(KviMdiChild* lpC, *m_pZ)
	{
		if(lpC->state() != KviMdiChild::Minimized)
		{
			moveChild(lpC,lpC->x(),0);
			lpC->resize(lpC->width(),viewport()->height());
		}
	}

	focusTopChild();
	updateContentsSize();
}

void KviMdiManager::expandHorizontal()
{
	ensureNoMaximized();
	// this hack is needed to ensure that the scrollbars are hidden and the viewport()->width() and height() are correct
	resizeContents(visibleWidth(),visibleHeight());
	updateScrollBars();
	g_pApp->sendPostedEvents();
	if(g_pApp->closingDown())return;
	
	foreach(KviMdiChild* lpC, *m_pZ)
	{
		if(lpC->state() != KviMdiChild::Minimized)
		{
			moveChild(lpC,0,lpC->y());
			lpC->resize(viewport()->width(),lpC->height());
		}
	}
	focusTopChild();
	updateContentsSize();
}

void KviMdiManager::minimizeAll()
{
    m_pFrm->setActiveWindow((KviWindow*)m_pFrm->firstConsole());
    foreach(KviMdiChild* lpC, *m_pZ)
	{
		if(lpC->state() != KviMdiChild::Minimized)lpC->minimize();
	}
	focusTopChild();
	updateContentsSize();
}

/*
void KviMdiManager::restoreAll()
{
	int idx=0;
	KviPtrList<KviMdiChild> list(*m_pZ);
	list.setAutoDelete(false);
	while(!list.isEmpty())
	{
		KviMdiChild *lpC=list.first();
		if(lpC->state() != KviMdiChild::Normal && (!(lpC->plainCaption()).contains("CONSOLE") ))
            lpC->restore();
		list.removeFirst();
	}
	focusTopChild();
	updateContentsSize();
}
*/

int KviMdiManager::getVisibleChildCount()
{
	int cnt=0;
	foreach(KviMdiChild* lpC, *m_pZ)
	{
		if(lpC->state() != KviMdiChild::Minimized)cnt++;
	}
	return cnt;
}

void KviMdiManager::tile()
{
	switch(KVI_OPTION_UINT(KviOption_uintTileMethod))
	{
		case KVI_TILE_METHOD_ANODINE:      tileAnodine(); break;
		case KVI_TILE_METHOD_PRAGMA4HOR:   tileAllInternal(4,true); break;
		case KVI_TILE_METHOD_PRAGMA4VER:   tileAllInternal(4,false); break;
		case KVI_TILE_METHOD_PRAGMA6HOR:   tileAllInternal(6,true); break;
		case KVI_TILE_METHOD_PRAGMA6VER:   tileAllInternal(6,false); break;
		case KVI_TILE_METHOD_PRAGMA9HOR:   tileAllInternal(9,true); break;
		case KVI_TILE_METHOD_PRAGMA9VER:   tileAllInternal(9,false); break;
		default:
			KVI_OPTION_UINT(KviOption_uintTileMethod) = KVI_TILE_METHOD_PRAGMA9HOR;
			tileAllInternal(9,true);
		break;
	}
}

void KviMdiManager::toggleAutoTile()
{
	if(KVI_OPTION_BOOL(KviOption_boolAutoTileWindows))
	{
		KVI_OPTION_BOOL(KviOption_boolAutoTileWindows) = false;
	} else {
		KVI_OPTION_BOOL(KviOption_boolAutoTileWindows) = true;
		tile();
	}
}


void KviMdiManager::tileAllInternal(int maxWnds,bool bHorizontal)
{
	//NUM WINDOWS =           1,2,3,4,5,6,7,8,9
	static int colstable[9]={ 1,1,1,2,2,2,3,3,3 }; //num columns
	static int rowstable[9]={ 1,2,3,2,3,3,3,3,3 }; //num rows
	static int lastwindw[9]={ 1,1,1,1,2,1,3,2,1 }; //last window multiplier
	static int colrecall[9]={ 0,0,0,3,3,3,6,6,6 }; //adjust self
	static int rowrecall[9]={ 0,0,0,0,4,4,4,4,4 }; //adjust self

	int * pColstable = bHorizontal ? colstable : rowstable;
	int * pRowstable = bHorizontal ? rowstable : colstable;
	int * pColrecall = bHorizontal ? colrecall : rowrecall;
	int * pRowrecall = bHorizontal ? rowrecall : colrecall;

	ensureNoMaximized();
	// this hack is needed to ensure that the scrollbars are hidden and the viewport()->width() and height() are correct
	resizeContents(visibleWidth(),visibleHeight());
	updateScrollBars();
	g_pApp->sendPostedEvents();
	if(g_pApp->closingDown())return;

	KviMdiChild *lpTop=topChild();
	int numVisible=getVisibleChildCount();

	if(numVisible<1)return;

	int numToHandle=((numVisible > maxWnds) ? maxWnds : numVisible);
	int xQuantum=viewport()->width()/pColstable[numToHandle-1];
	if(xQuantum < ((lpTop->minimumSize().width() > KVI_MDICHILD_MIN_WIDTH) ? lpTop->minimumSize().width() : KVI_MDICHILD_MIN_WIDTH)){
		if(pColrecall[numToHandle-1]==0)debug("Tile : Not enouh space");
		else tileAllInternal(pColrecall[numToHandle-1],bHorizontal);
		return;
	}
	int yQuantum=viewport()->height()/pRowstable[numToHandle-1];
	if(yQuantum < ((lpTop->minimumSize().height() > KVI_MDICHILD_MIN_HEIGHT) ? lpTop->minimumSize().height() : KVI_MDICHILD_MIN_HEIGHT)){
		if(pRowrecall[numToHandle-1]==0)debug("Tile : Not enough space");
		else tileAllInternal(pRowrecall[numToHandle-1],bHorizontal);
		return;
	}
	int curX=0;
	int curY=0;
	int curRow=1;
	int curCol=1;
	int curWin=1;

	foreach(KviMdiChild* lpC, *m_pZ)
	{
		if(lpC->state()!=KviMdiChild::Minimized)
		{
			if((curWin%numToHandle)==0)
			{
				moveChild(lpC,curX,curY);
				lpC->resize(xQuantum * lastwindw[numToHandle-1],yQuantum);
			} else {
				moveChild(lpC,curX,curY);
				lpC->resize(xQuantum,yQuantum);
			}
			//example : 12 windows : 3 cols 3 rows
			if(curCol<pColstable[numToHandle-1])
			{ //curCol<3
				curX+=xQuantum; //add a column in the same row
				curCol++;       //increase current column
			} else {
				curX=0;         //new row
				curCol=1;       //column 1
				if(curRow<pRowstable[numToHandle-1])
				{ //curRow<3
					curY+=yQuantum; //add a row
					curRow++;       //
				} else {
					curY=0;         //restart from beginning
					curRow=1;       //
				}
			}
			curWin++;
		}
	}
	if(lpTop)lpTop->setFocus();
	updateContentsSize();
}

void KviMdiManager::tileAnodine()
{
	ensureNoMaximized();
	// this hack is needed to ensure that the scrollbars are hidden and the viewport()->width() and height() are correct
	resizeContents(visibleWidth(),visibleHeight());
	updateScrollBars();
	g_pApp->sendPostedEvents();
	if(g_pApp->closingDown())return;

	KviMdiChild *lpTop=topChild();
	int numVisible=getVisibleChildCount(); // count visible windows
	if(numVisible<1)return;
	int numCols=int(sqrt((double)numVisible)); // set columns to square root of visible count
	// create an array to form grid layout
	int *numRows=new int[numCols];
	int numCurCol=0;
	while(numCurCol<numCols)
	{
		numRows[numCurCol]=numCols; // create primary grid values
		numCurCol++;
	}
	int numDiff=numVisible-(numCols*numCols); // count extra rows
	int numCurDiffCol=numCols; // set column limiting for grid updates
	while(numDiff>0)
	{
		numCurDiffCol--;
		numRows[numCurDiffCol]++; // add extra rows to column grid
		if(numCurDiffCol<1)numCurDiffCol=numCols; // rotate through the grid
		numDiff--;
	}
	numCurCol=0;
	int numCurRow=0;
	int curX=0;
	int curY=0;
	// the following code will size everything based on my grid above
	// there is no limit to the number of windows it will handle
	// it's great when a kick-ass theory works!!!                      // Pragma :)
	int xQuantum=viewport()->width()/numCols;
	int yQuantum=viewport()->height()/numRows[numCurCol];

	foreach(KviMdiChild* lpC, *m_pZ)
	{
		if(lpC->state() != KviMdiChild::Minimized)
		{
			moveChild(lpC,curX,curY);
			lpC->resize(xQuantum,yQuantum);
			numCurRow++;
			curY+=yQuantum;
			if(numCurRow==numRows[numCurCol])
			{
				numCurRow=0;
				numCurCol++;
				curY=0;
				curX+=xQuantum;
				if(numCurCol!=numCols)yQuantum=viewport()->height()/numRows[numCurCol];
			}
		}
	}
	delete[] numRows;
	if(lpTop)lpTop->setFocus();
	updateContentsSize();
}
#endif


