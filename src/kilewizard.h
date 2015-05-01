/***************************************************************************
    begin                : Tue Dec 23 2003
    copyright            : (C) 2003 Jeroen Wijnhout
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

#ifndef KILEWIZARD_H
#define KILEWIZARD_H

#include <QDialog>

#include "kileactions.h"

class KConfig;
class QDialogButtonBox;

namespace KileDialog
{
class Wizard : public QDialog
{
public:
	explicit Wizard(KConfig *, QWidget *parent = Q_NULLPTR, const char *name = NULL, const QString &caption = QString());
	virtual ~Wizard();

public:
	const KileAction::TagData & tagData() const { return m_td; }

protected:
	KConfig * config() const;
	QDialogButtonBox * buttonBox() const;
	KileAction::TagData m_td;

private:
	KConfig *m_config;
	QDialogButtonBox *m_buttonBox;
};
}

#endif
