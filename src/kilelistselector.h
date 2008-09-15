/*****************************************************************************************
    begin                : Fri Aug 15 2003
    copyright            : (C) 2003 by Jeroen Wijnhout (Jeroen.Wijnhout@kdemail.net)
                           (C) 2007 by Holger Danielsson (holger.danielsson@versanet.de)
 *****************************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef KILELISTSELECTOR_H
#define KILELISTSELECTOR_H

#include <KDialog>

class QTreeWidget;
class QStringList;

class KileListSelectorBase : public KDialog
{
	public:
		KileListSelectorBase(const QStringList &list, const QString &caption, const QString &select, QWidget *parent = 0, const char *name = 0);
		~KileListSelectorBase() {}

		int currentItem();

	protected:
		QTreeWidget *m_listview;
		void insertStringList(const QStringList &list);
};

class KileListSelector : public KileListSelectorBase
{
	public:
		KileListSelector(const QStringList &list, const QString &caption, const QString &select, QWidget *parent = 0, const char *name = 0);
		~KileListSelector() {}
};

class KileListSelectorMultiple : public KileListSelectorBase
{
	public:
		KileListSelectorMultiple(const QStringList & list, const QString &caption, const QString &select, QWidget *parent = 0, const char *name = 0);
		~KileListSelectorMultiple() {}

		const QStringList &selected();

	private:
		QStringList m_selectedfiles;
};


#endif