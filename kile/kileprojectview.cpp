/***************************************************************************
                          kileproject.cpp -  description
                             -------------------
    begin                : Tue Aug 12 2003
    copyright            : (C) 2003 by Jeroen Wijnhout
    email                : Jeroen.Wijnhout@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qheader.h>

#include <klocale.h>
#include <kiconloader.h>
#include <kpopupmenu.h>
#include <kurl.h>

#include "kileinfo.h"
#include "kiledocumentinfo.h"
#include "kileproject.h"
#include "kileprojectview.h"

const int KPV_ID_OPEN = 0, KPV_ID_SAVE = 1, KPV_ID_CLOSE = 2, KPV_ID_OPTIONS = 3, KPV_ID_ADD = 4, KPV_ID_REMOVE = 5;

/*
 * KileProjectViewItem
 */
void KileProjectViewItem::urlChanged(const KURL &url)
{
	setURL(url); setText(0,url.fileName()); kdDebug() << "SLOT KileProjectViewItem(" << text(0) << ")::urlChanged" << endl;
}

void KileProjectViewItem::nameChanged(const QString & name)
{
	setText(0,name); kdDebug() << "SLOT KileProjectViewItem(" << text(0) << ")::nameChanged" << endl;
}

void KileProjectViewItem::isrootChanged(bool isroot)
{
	kdDebug() << "SLOT isrootChanged " << text(0) << " to " << isroot << endl;
	if (isroot)
	{
		setPixmap(0,UserIcon("masteritem"));
	}
	else
	{
		if (type() == KileType::ProjectItem)
		{
			if ( text(0).right(7) == ".kilepr" )
				setPixmap(0,SmallIcon("kile"));
			else
				setPixmap(0,UserIcon("projectitem"));
		}
		else
			setPixmap(0,UserIcon("file"));
	}
}

/*
 * KileProjectView
 */
KileProjectView::KileProjectView(QWidget *parent, KileInfo *ki) : KListView(parent), m_ki(ki), m_nProjects(0)
{
	addColumn(i18n("Files and Projects"),-1);
	setSorting(-1);
	setFocusPolicy(QWidget::ClickFocus);
	header()->hide();
	setRootIsDecorated(true);

	m_popup = new KPopupMenu(this, "projectview_popup");

	connect(this, SIGNAL(contextMenu(KListView *, QListViewItem *, const QPoint & )),
		this,SLOT(popup(KListView *, QListViewItem * , const QPoint & )));

	connect(this, SIGNAL(executed(QListViewItem*)), this, SLOT(slotClicked(QListViewItem*)));
}

void KileProjectView::slotClicked(QListViewItem *item)
{
	if (item == 0)
		item = currentItem();

	KileProjectViewItem *itm = static_cast<KileProjectViewItem*>(item);
	kdDebug() << "KileProjectView:: clicked(" << itm->url().fileName() << ")" << endl;
	if (itm && (itm->type() == KileType::File || itm->type() == KileType::ProjectItem ))
	{
		kdDebug() << "KileProjectView:: emit fileSelected" << endl;
		emit fileSelected(itm->url());
	}
}

void KileProjectView::slotFile(int id)
{
	KileProjectViewItem *item = static_cast<KileProjectViewItem*>(currentItem());
	if (item && item->type() == KileType::File)
	{
		switch (id)
		{
			case KPV_ID_OPEN : emit(fileSelected(item->url())); break;
			case KPV_ID_SAVE : emit(saveURL(item->url())); break;
			case KPV_ID_ADD : emit(addToProject(item->url())); break;
			case KPV_ID_CLOSE : emit(closeURL(item->url())); break;
			default : break;
		}
	}
}

void KileProjectView::slotProjectItem(int id)
{
	KileProjectViewItem *item = static_cast<KileProjectViewItem*>(currentItem());
	if (item && item->type() == KileType::ProjectItem)
	{
		KURL projecturl,url;
		switch (id)
		{
			case KPV_ID_OPEN : emit(fileSelected(item->url())); break;
			case KPV_ID_SAVE : emit(saveURL(item->url())); break;
			case KPV_ID_REMOVE :
				projecturl = item->parent()->url();
				url = item->url();
				emit(removeFromProject(projecturl , url));
				break;
			case KPV_ID_CLOSE : emit(closeURL(item->url())); break;
			default : break;
		}
	}
}

void KileProjectView::slotProject(int id)
{
	KileProjectViewItem *item = static_cast<KileProjectViewItem*>(currentItem());
	if (item && item->type() == KileType::Project)
	{
		switch (id)
		{
			case KPV_ID_OPTIONS : emit(projectOptions(item->url())); break;
			case KPV_ID_CLOSE : emit(closeProject(item->url())); break;
			default : break;
		}
	}
}

void KileProjectView::popup(KListView *, QListViewItem *  item, const QPoint &  point)
{
	if (item != 0)
	{
		m_popup->clear();
		m_popup->disconnect();

		KileProjectViewItem *itm = static_cast<KileProjectViewItem*>(item);
		kdDebug() << "popup " << itm->url().path() << endl;

		if (itm->type() == KileType::File)
		{
			m_popup->insertItem(SmallIcon("fileopen"), i18n("&Open"), KPV_ID_OPEN);
			m_popup->insertItem(SmallIcon("filesave"), i18n("&Save"), KPV_ID_SAVE);
			m_popup->insertSeparator();
			if (m_nProjects>0) m_popup->insertItem(i18n("&Add To Project"), KPV_ID_ADD);
			m_popup->insertSeparator();
			connect(m_popup,  SIGNAL(activated(int)), this, SLOT(slotFile(int)));
		}
		else if (itm->type() == KileType::ProjectItem)
		{
			m_popup->insertItem(SmallIcon("fileopen"), i18n("&Open"), KPV_ID_OPEN);
			m_popup->insertItem(SmallIcon("filesave"), i18n("&Save"), KPV_ID_SAVE);
			m_popup->insertSeparator();
			m_popup->insertItem(i18n("&Remove From Project"), KPV_ID_REMOVE);
			m_popup->insertSeparator();
			connect(m_popup,  SIGNAL(activated(int)), this, SLOT(slotProjectItem(int)));
		}
		else if (itm->type() == KileType::Project)
		{
   			m_popup->insertItem(SmallIcon("configure"),i18n("Project &Options"), KPV_ID_OPTIONS);
			m_popup->insertSeparator();
			connect(m_popup,  SIGNAL(activated(int)), this, SLOT(slotProject(int)));
		}
		m_popup->insertItem(SmallIcon("fileclose"), i18n("&Close"), KPV_ID_CLOSE);

		m_popup->exec(point);
	}
}

void KileProjectView::makeTheConnection(KileProjectViewItem *item)
{
	kdDebug() << "\tmakeTheConnection " << item->text(0) << endl;

	if (item->type() == KileType::Project)
	{
		KileProject *project = m_ki->projectFor(item->url());
		//make some connections
		connect(project, SIGNAL(nameChanged(const QString &)), item, SLOT(nameChanged(const QString &)));
	}
	else
	{
		KileDocumentInfo *docinfo = m_ki->infoFor(item->url().path());
		item->setInfo(docinfo);
		if (docinfo ==0 ) {kdDebug() << "\tmakeTheConnection COULD NOT FIND A DOCINFO" << endl; return;}
		connect(docinfo, SIGNAL(nameChanged(const KURL&)),  item, SLOT(urlChanged(const KURL&)));
		connect(docinfo, SIGNAL(isrootChanged(bool)), item, SLOT(isrootChanged(bool)));
		//set the pixmap
		item->isrootChanged(docinfo->isLaTeXRoot());
	}
}

void KileProjectView::add(KileProject *project)
{
	KileProjectViewItem *item, *parent = new KileProjectViewItem(this, project->name());
	parent->setType(KileType::Project);
	parent->setURL(project->url());
	parent->setOpen(true);
	parent->setPixmap(0,SmallIcon("relation"));
	makeTheConnection(parent);

	KileProjectItemList *list = project->items();
	for (uint i = 0; i < list->count(); i++)
	{
		kdDebug() << "adding ITEM " << list->at(i)->url().fileName() << endl;
		item =  new KileProjectViewItem(parent, list->at(i)->url().fileName());
		item->setType(KileType::ProjectItem);
		item->setURL(list->at(i)->url());
		makeTheConnection(item);
	}

	m_nProjects++;
}

void KileProjectView::add(const KileProjectItem *projitem)
{
	kdDebug() << "\tKileProjectView::adding projectitem " << projitem->path() << endl;
	//find project first
	const KileProject *project = projitem->project();

	KileProjectViewItem *item = static_cast<KileProjectViewItem*>(firstChild());

	while ( item)
	{
		if ( (item->type() == KileType::Project) && (item->url() == project->url()) )
		{
			item =  new KileProjectViewItem(item, projitem->url().fileName());
			item->setType(KileType::ProjectItem);
			item->setURL(projitem->url());
			makeTheConnection(item);
			break;
		}
		item = static_cast<KileProjectViewItem*>(item->nextSibling());
	}
}

void KileProjectView::add(const KURL & url)
{
	kdDebug() << "\tKileProjectView::adding item " << url.path() << endl;
	//check if file is already present
	QListViewItemIterator it( this );
	KileProjectViewItem *item;
	while ( it.current())
	{
		item = static_cast<KileProjectViewItem*>(*it);
		if ( (item->type() != KileType::Project) && item->url() == url )
			return;
		it++;
	}

	item = new KileProjectViewItem(this, url.fileName());
	item->setType(KileType::File);
	item->setURL(url);
	makeTheConnection(item);
}

void KileProjectView::remove(const KileProject *project)
{
	KileProjectViewItem *item = static_cast<KileProjectViewItem*>(firstChild());

	while ( item)
	{
		if ( item->url() == project->url() )
		{
			takeItem(item);
			delete item;
			m_nProjects--;
			break;
		}
		item = static_cast<KileProjectViewItem*>(item->nextSibling());
	}
}

/**
 * Removes a file from the projectview, does not remove project-items. Only files without a project.
 **/
void KileProjectView::remove(const KURL &url)
{
	KileProjectViewItem *item = static_cast<KileProjectViewItem*>(firstChild());

	while ( item)
	{
		if ( (item->type() == KileType::File) && (item->url() == url) )
		{
			takeItem(item);
			delete item;
			break;
		}
		item = static_cast<KileProjectViewItem*>(item->nextSibling());
	}
}

void KileProjectView::removeItem(const KURL &url)
{
	QListViewItemIterator it( this );
	KileProjectViewItem *item;

	while ( it.current())
	{
		item = static_cast<KileProjectViewItem*>(*it);
		if ( (item->type() == KileType::ProjectItem) && (item->url() == url) )
		{
			item->parent()->takeItem(item);
			delete item;
			break;
		}
		it++;
	}
}

#include "kileprojectview.moc"
