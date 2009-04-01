/******************************************************************************************
    begin                : Sat 3-1 20:40:00 CEST 2004
    copyright            : (C) 2004 by Jeroen Wijnhout (Jeroen.Wijnhout@kdemail.net)
                               2007 by Michel Ludwig (michel.ludwig@kdemail.net)
 ******************************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "widgets/toolconfigwidget.h"

#include <QCheckBox>
#include <QLabel>
#include <QLayout>
#include <QRegExp>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>

#include "kiledebug.h"
#include <keditlistbox.h>
#include <klocale.h>
#include <kicondialog.h>
#include <kiconloader.h>
#include <kcombobox.h>
#include <kpushbutton.h>
#include <kconfig.h>
#include <klineedit.h>
#include <kmessagebox.h>
#include <kinputdialog.h>

#include "kiletoolmanager.h"
#include "kilestdtools.h"
#include "widgets/maintoolconfigwidget.h"
#include "widgets/processtoolconfigwidget.h"
#include "widgets/librarytoolconfigwidget.h"
#include "widgets/quicktoolconfigwidget.h"
#include "widgets/latextoolconfigwidget.h"
#include "dialogs/newtoolwizard.h"

namespace KileWidget
{
	ToolConfig::ToolConfig(KileTool::Manager *mngr, QWidget *parent, const char *name) :
		QWidget(parent),
		m_manager(mngr)
	{
		setObjectName(name);
		m_config = m_manager->config();
		QVBoxLayout *layout = new QVBoxLayout();
		layout->setMargin(0);
		layout->setSpacing(KDialog::spacingHint());
		setLayout(layout);
		m_configWidget = new ToolConfigWidget(this);
		layout->addWidget(m_configWidget);

		m_tabGeneral = m_configWidget->m_tab->widget(0);
		m_tabAdvanced = m_configWidget->m_tab->widget(1);
		m_tabMenu = m_configWidget->m_tab->widget(2);

		updateToollist();
		QListWidgetItem *item = m_configWidget->m_lstbTools->item(indexQuickBuild());
		if (item)
			m_configWidget->m_lstbTools->setCurrentItem(item);

		connect(m_configWidget->m_cbConfig, SIGNAL(activated(int)), this, SLOT(switchConfig(int)));

		QStringList lst; lst << i18n( "Quick" ) << i18n( "Compile" ) << i18n( "Convert" ) << i18n( "View" ) << i18n( "Other" );
		m_configWidget->m_cbMenu->addItems(lst);
		connect(m_configWidget->m_cbMenu, SIGNAL(activated(const QString &)), this, SLOT(setMenu(const QString &)));
		connect(m_configWidget->m_pshbIcon, SIGNAL(clicked()), this, SLOT(selectIcon()));

		connect(m_configWidget->m_pshbRemoveTool, SIGNAL(clicked()), this, SLOT(removeTool()));
		connect(m_configWidget->m_pshbNewTool, SIGNAL(clicked()), this, SLOT(newTool()));
		connect(m_configWidget->m_pshbRemoveConfig, SIGNAL(clicked()), this, SLOT(removeConfig()));
		connect(m_configWidget->m_pshbNewConfig, SIGNAL(clicked()), this, SLOT(newConfig()));
		connect(m_configWidget->m_pshbDefault, SIGNAL(clicked()), this, SLOT(writeDefaults()));

		//--->m_current = m_configWidget->m_lstbTools->text(0);
		QListWidgetItem *currentItem = m_configWidget->m_lstbTools->currentItem();
		if(currentItem) {
			m_current = currentItem->text();
		}
		m_manager->retrieveEntryMap(m_current, m_map, false, false);
		QString cfg = KileTool::configName(m_current, m_config);
		m_configWidget->m_cbConfig->addItem(cfg);

		setupGeneral();
		setupAdvanced();

		switchConfig(cfg);
		switchTo(m_current, false);
		connect(m_configWidget->m_lstbTools, SIGNAL(currentTextChanged(const QString &)), this, SLOT(switchTo(const QString &)));

		connect(this, SIGNAL(changed()), this, SLOT(updateAdvanced()));
		connect(this, SIGNAL(changed()), this, SLOT(updateGeneral()));
	}

	void ToolConfig::setupAdvanced()
	{
		m_configWidget->m_cbType->addItem(i18n("Run Outside of Kile"));
		m_configWidget->m_cbType->addItem(i18n("Run in Konsole"));
		m_configWidget->m_cbType->addItem(i18n("Run Embedded in Kile"));
		m_configWidget->m_cbType->addItem(i18n("Use HTML Viewer"));
		m_configWidget->m_cbType->addItem(i18n("Run Sequence of Tools"));
		connect(m_configWidget->m_cbType, SIGNAL(activated(int)), this, SLOT(switchType(int)));
		connect(m_configWidget->m_ckClose, SIGNAL(toggled(bool)), this, SLOT(setClose(bool)));

		m_classes << "Compile" << "Convert" << "Archive" << "View" <<  "Sequence" << "LaTeX" << "ViewHTML" << "ViewBib" << "ForwardDVI" << "Base";
		m_configWidget->m_cbClass->addItems(m_classes);
		connect(m_configWidget->m_cbClass, SIGNAL(activated(const QString &)), this, SLOT(switchClass(const QString &)));

		connect(m_configWidget->m_leSource, SIGNAL(textChanged(const QString &)), this, SLOT(setFrom(const QString &)));
		connect(m_configWidget->m_leTarget, SIGNAL(textChanged(const QString &)), this, SLOT(setTo(const QString &)));
		connect(m_configWidget->m_leFile, SIGNAL(textChanged(const QString &)), this, SLOT(setTarget(const QString &)));
		connect(m_configWidget->m_leRelDir, SIGNAL(textChanged(const QString &)), this, SLOT(setRelDir(const QString &)));

		m_configWidget->m_cbState->addItem("Editor");
		m_configWidget->m_cbState->addItem("Viewer");
		m_configWidget->m_cbState->addItem("HTMLpreview");
		connect(m_configWidget->m_cbState, SIGNAL(activated(const QString &)), this, SLOT(setState(const QString &)));
	}

	void ToolConfig::updateAdvanced()
	{
		bool enablekonsoleclose = false;
		QString type = m_map["type"];
		if(type == "Process") {
			m_configWidget->m_cbType->setCurrentItem(0);
		}
		else if(type == "Konsole") {
			m_configWidget->m_cbType->setCurrentIndex(1);
			enablekonsoleclose = true;
		}
		else if(type == "Part") {
			m_configWidget->m_cbType->setCurrentIndex(2);
		}
		else if(type == "DocPart") {
			m_configWidget->m_cbType->setCurrentIndex(3);
		}
		else if(type == "Sequence") {
			m_configWidget->m_cbType->setCurrentIndex(4);
		}
		m_configWidget->m_ckClose->setEnabled(enablekonsoleclose);

		QString state = m_map["state"];
		if(state.isEmpty()) {
			state = "Editor";
		}
		int i = m_configWidget->m_cbState->findText(state);
		if(i >= 0) {
			m_configWidget->m_cbState->setCurrentIndex(i);
		}
		else {
			m_configWidget->m_cbState->setItemText(m_configWidget->m_cbState->currentIndex(),
			                                       state);
		}

		int index = m_classes.indexOf(m_map["class"]);
		if(index == -1) {
			index = m_classes.count() - 1;
		}
		m_configWidget->m_cbClass->setCurrentIndex(index);
		m_configWidget->m_ckClose->setChecked(m_map["close"] == "yes");
		m_configWidget->m_leSource->setText(m_map["from"]);
		m_configWidget->m_leTarget->setText(m_map["to"]);
		m_configWidget->m_leFile->setText(m_map["target"]);
		m_configWidget->m_leRelDir->setText(m_map["relDir"]);
	}

	void ToolConfig::setupGeneral()
	{
		m_configWidget->m_stackBasic->insertWidget(GBS_None, new QLabel(i18n("Use the \"Advanced\" tab to configure this tool."), this));

		m_ptcw = new ProcessToolConfigWidget(m_configWidget->m_stackBasic);
		m_configWidget->m_stackBasic->insertWidget(GBS_Process, m_ptcw);
		connect(m_ptcw->m_command, SIGNAL(textChanged(const QString &)), this, SLOT(setCommand(const QString &)));
		connect(m_ptcw->m_options, SIGNAL(textChanged()), this, SLOT(setOptions()));

		m_ltcw = new LibraryToolConfigWidget(m_configWidget->m_stackBasic);
		m_configWidget->m_stackBasic->insertWidget(GBS_Library, m_ltcw);
		connect(m_ltcw->m_library, SIGNAL(textChanged(const QString &)), this, SLOT(setLibrary(const QString &)));
		connect(m_ltcw->m_class, SIGNAL(textChanged(const QString &)), this, SLOT(setClassName(const QString &)));
		connect(m_ltcw->m_options, SIGNAL(textChanged(const QString &)), this, SLOT(setLibOptions(const QString &)));

		m_qtcw = new QuickToolConfigWidget(m_configWidget->m_stackBasic);
		m_configWidget->m_stackBasic->insertWidget(GBS_Sequence, m_qtcw);
		connect(m_qtcw, SIGNAL(sequenceChanged(const QString &)), this, SLOT(setSequence(const QString &)));

		m_configWidget->m_stackBasic->insertWidget(GBS_Error, new QLabel(i18n("Unknown tool type; your configuration data is malformed.\nPerhaps it is a good idea to restore the default settings."), this));

		m_configWidget->m_stackExtra->insertWidget(GES_None, new QWidget(this));

		m_LaTeXtcw = new LaTeXToolConfigWidget(m_configWidget->m_stackExtra);
		m_configWidget->m_stackExtra->insertWidget(GES_LaTeX, m_LaTeXtcw);
		connect(m_LaTeXtcw->m_ckRootDoc, SIGNAL(toggled(bool)), this, SLOT(setLaTeXCheckRoot(bool)));
		connect(m_LaTeXtcw->m_ckJump, SIGNAL(toggled(bool)), this, SLOT(setLaTeXJump(bool)));
		connect(m_LaTeXtcw->m_ckAutoRun, SIGNAL(toggled(bool)), this, SLOT(setLaTeXAuto(bool)));

	}

	void ToolConfig::updateGeneral()
	{
		QString type = m_map["type"];

		int basicPage = GBS_None;
		int extraPage = GES_None;

		if ( type == "Process" || type == "Konsole" ) basicPage = GBS_Process;
		else if ( type == "Part" ) basicPage = GBS_Library;
		else if ( type == "DocPart" ) basicPage = GBS_None;
		else if ( type == "Sequence" )
		{
			basicPage = GBS_Sequence;
			m_qtcw->updateSequence(m_map["sequence"]);
		}
		else basicPage = GBS_Error;

		QString cls = m_map["class"];
		if ( cls == "LaTeX" )
			extraPage = GES_LaTeX;

		m_ptcw->m_command->setText(m_map["command"]);
		m_ptcw->m_options->setText(m_map["options"]);

		m_ltcw->m_library->setText(m_map["libName"]);
		m_ltcw->m_class->setText(m_map["className"]);
		m_ltcw->m_options->setText(m_map["libOptions"]);

		m_LaTeXtcw->m_ckRootDoc->setChecked(m_map["checkForRoot"] == "yes");
		m_LaTeXtcw->m_ckJump->setChecked(m_map["jumpToFirstError"] == "yes");
		m_LaTeXtcw->m_ckAutoRun->setChecked(m_map["autoRun"] == "yes");

		KILE_DEBUG() << "showing pages " << basicPage << " " << extraPage;
		m_configWidget->m_stackBasic->setCurrentIndex(basicPage);
		m_configWidget->m_stackExtra->setCurrentIndex(extraPage);

	}

	void ToolConfig::writeDefaults()
	{
		if (KMessageBox::warningContinueCancel(this, i18n("All your tool settings will be overwritten with the default settings, are you sure you want to continue?")) == KMessageBox::Continue) {
			QStringList groups = m_config->groupList();
			QRegExp re = QRegExp("Tool/(.+)/.+");
			for(int i = 0; i < groups.count(); ++i) {
				if (re.exactMatch(groups[i])) {
					m_config->deleteGroup(groups[i]);
				}
			}
			// magic names, defined in kilestdtools.rc
			m_config->deleteGroup("ToolsGUI");
			m_config->deleteGroup("Tools");

			m_manager->factory()->writeStdConfig();
			m_config->sync();
			updateToollist();
  			QStringList tools = KileTool::toolList(m_config, true);
			for (int i = 0; i < tools.count(); ++i) {
				switchTo(tools[i], false);// needed to retrieve the new map
 				switchTo(tools[i],true); // this writes the newly retrieved entry map (and not an perhaps changed old one)
			}
			int index = indexQuickBuild();
			switchTo(tools[index], false);
			m_configWidget->m_lstbTools->item(index)->setSelected(true);
		}
	}

	void ToolConfig::updateToollist()
	{
		//KILE_DEBUG() << "==ToolConfig::updateToollist()====================";
		m_configWidget->m_lstbTools->clear();
		m_configWidget->m_lstbTools->addItems(KileTool::toolList(m_config, true));
		m_configWidget->m_lstbTools->sortItems();
	}

	void ToolConfig::setMenu(const QString & menu)
	{
		//KILE_DEBUG() << "==ToolConfig::setMenu(const QString & menu)====================";
		m_map["menu"] = menu;
	}

	void ToolConfig::writeConfig()
	{
		//KILE_DEBUG() << "==ToolConfig::writeConfig()====================";
		//save config
		m_manager->saveEntryMap(m_current, m_map, false, false);
		KileTool::setGUIOptions(m_current, m_configWidget->m_cbMenu->currentText(), m_icon, m_config);
	}

	int ToolConfig::indexQuickBuild()
	{
		QList<QListWidgetItem *> itemsList = m_configWidget->m_lstbTools->findItems("QuickBuild", Qt::MatchExactly);
		if(itemsList.isEmpty()) {
			return 0;
		}

		return m_configWidget->m_lstbTools->row(itemsList.first());
	}
	
	void ToolConfig::switchConfig(int /*index*/)
	{
		//KILE_DEBUG() << "==ToolConfig::switchConfig(int /*index*/)====================";
		switchTo(m_current);
	}

	void ToolConfig::switchConfig(const QString & cfg)
	{
		//KILE_DEBUG() << "==ToolConfig::switchConfig(const QString & cfg)==========";
		for(int i = 0; i < m_configWidget->m_cbConfig->count(); ++i) {
			if (m_configWidget->m_cbConfig->itemText(i) == cfg) {
				m_configWidget->m_cbConfig->setCurrentIndex(i);
			}
		}
	}

	void ToolConfig::switchTo(const QString & tool, bool save /* = true */)
	{
		//KILE_DEBUG() << "==ToolConfig::switchTo(const QString & tool, bool save /* = true */)====================";
		//save config
		if(save) {
			writeConfig();

			//update the config number
			QString cf = m_configWidget->m_cbConfig->currentText();
			KileTool::setConfigName(m_current, cf, m_config);
		}

		m_current = tool;

		m_map.clear();
		if (!m_manager->retrieveEntryMap(m_current, m_map, false, false)) {
			kWarning() << "no entrymap";
		}

		updateConfiglist();
		updateGeneral();
		updateAdvanced();

		//show GUI info
		QString menu = KileTool::menuFor(m_current, m_config);
		int i = m_configWidget->m_cbMenu->findText(menu);
		if(i >= 0) {
			m_configWidget->m_cbMenu->setCurrentIndex(i);
		}
		else {
			m_configWidget->m_cbMenu->setItemText(m_configWidget->m_cbMenu->currentIndex(), menu);
		}
		m_icon = KileTool::iconFor(m_current, m_config);
		if(m_icon.isEmpty()) {
			m_configWidget->m_pshbIcon->setIcon(KIcon(QString()));
		}
		else {
			m_configWidget->m_pshbIcon->setIcon(KIcon(m_icon));
		}
	}

	void ToolConfig::updateConfiglist()
	{
		//KILE_DEBUG() << "==ToolConfig::updateConfiglist()=====================";
		m_configWidget->m_groupBox->setTitle(i18n("Choose a configuration for the tool %1",m_current));
		m_configWidget->m_cbConfig->clear();
		m_configWidget->m_cbConfig->addItems(KileTool::configNames(m_current, m_config));
		QString cfg = KileTool::configName(m_current, m_config);
		switchConfig(cfg);
		m_configWidget->m_cbConfig->setEnabled(m_configWidget->m_cbConfig->count() > 1);
	}

	void ToolConfig::selectIcon()
	{
		KILE_DEBUG() << "icon ---> " << m_icon;
		//KILE_DEBUG() << "==ToolConfig::selectIcon()=====================";
		KIconDialog *dlg = new KIconDialog(this);
		QString res = dlg->openDialog();
		if(m_icon != res) {
			if(res.isEmpty()) {
				return;
			}

			m_icon = res;
			writeConfig();
			if (m_icon.isEmpty()) {
				m_configWidget->m_pshbIcon->setIcon(KIcon(QString()));
			}
			else {
				m_configWidget->m_pshbIcon->setIcon(KIcon(m_icon));
			}
		}
	}

	void ToolConfig::newTool()
	{
		//KILE_DEBUG() << "==ToolConfig::newTool()=====================";
		NewToolWizard *ntw = new NewToolWizard(this);
		if (ntw->exec()) {
			QString toolName = ntw->toolName();
			QString parentTool = ntw->parentTool();

			writeStdConfig(toolName, "Default");
			if(parentTool != ntw->customTool()) {
				//copy tool info
				KileTool::Config tempMap;
				m_manager->retrieveEntryMap(parentTool, tempMap, false, false);
				KConfigGroup toolGroup = m_config->group(KileTool::groupFor(toolName, "Default"));
				toolGroup.writeEntry("class", tempMap["class"]);
				toolGroup.writeEntry("type", tempMap["type"]);
				toolGroup.writeEntry("state", tempMap["state"]);
				toolGroup.writeEntry("close", tempMap["close"]);
				toolGroup.writeEntry("checkForRoot", tempMap["checkForRoot"]);
				toolGroup.writeEntry("autoRun", tempMap["autoRun"]);
				toolGroup.writeEntry("jumpToFirstError", tempMap["jumpToFirstError"]);
			}

			m_configWidget->m_lstbTools->blockSignals(true);
			updateToollist();
			switchTo(toolName);
			for(int i = 0; i < m_configWidget->m_lstbTools->count(); ++i) {
				if(m_configWidget->m_lstbTools->item(i)->text() == toolName) {
					m_configWidget->m_lstbTools->setCurrentRow(i);
					break;
				}
			}
			m_configWidget->m_lstbTools->blockSignals(false);
		}
	}

	void ToolConfig::newConfig()
	{
		//KILE_DEBUG() << "==ToolConfig::newConfig()=====================";
		writeConfig();
		bool ok;
		QString cfg = KInputDialog::getText(i18n("New Configuration"), i18n("Enter new configuration name:"), "", &ok, this);
		if (ok && (!cfg.isEmpty())) {
			//copy config
			KConfigGroup toolGroup = m_config->group(KileTool::groupFor(m_current, cfg));
			for (QMap<QString,QString>::Iterator it  = m_map.begin(); it != m_map.end(); ++it) {
				toolGroup.writeEntry(it.key(), it.value());
			}
			KileTool::setConfigName(m_current, cfg, m_config);
			switchTo(m_current, false);
			switchConfig(cfg);
		}
	}

	void ToolConfig::writeStdConfig(const QString & tool, const QString & cfg)
	{
		KConfigGroup toolGroup = m_config->group(KileTool::groupFor(tool, cfg));
		toolGroup.writeEntry("class", "Compile");
		toolGroup.writeEntry("type", "Process");
		toolGroup.writeEntry("menu", "Compile");
		toolGroup.writeEntry("state", "Editor");
		toolGroup.writeEntry("close", "no");

		m_config->group("Tools").writeEntry(tool, cfg);
	}

	void ToolConfig::removeTool()
	{
// 		KILE_DEBUG() << "==ToolConfig::removeTool()=====================";
		if(KMessageBox::warningContinueCancel(this, i18n("Are you sure you want to remove the tool %1?", m_current)) == KMessageBox::Continue) {
			QStringList cfgs = KileTool::configNames(m_current, m_config);
// 			KILE_DEBUG() << "cfgs " <<  cfgs.join(", ");
			for(int i = 0; i < cfgs.count(); ++i) {
// 				KILE_DEBUG() << "group " << KileTool::groupFor(m_current, cfgs[i]);
				m_config->deleteGroup(KileTool::groupFor(m_current, cfgs[i]));
			}
			m_config->group("Tools").deleteEntry(m_current);
			m_config->group("ToolsGUI").deleteEntry(m_current);
			m_config->sync();

			int index = m_configWidget->m_lstbTools->currentRow() - 1;
			if(index < 0) {
				index = 0;
			}
			QString tool = m_configWidget->m_lstbTools->item(index)->text();
// 			KILE_DEBUG() << "tool is " << tool;
			m_configWidget->m_lstbTools->blockSignals(true);
			updateToollist();
			m_configWidget->m_lstbTools->setCurrentRow(index);
			switchTo(tool, false);
			m_configWidget->m_lstbTools->blockSignals(false);
		}
	}

	void ToolConfig::removeConfig()
	{
		//KILE_DEBUG() << "==ToolConfig::removeConfig()=====================";
		writeConfig();
		if ( m_configWidget->m_cbConfig->count() > 1) {
			if(KMessageBox::warningContinueCancel(this, i18n("Are you sure that you want to remove this configuration?") )
			   == KMessageBox::Continue) {
				m_config->deleteGroup(KileTool::groupFor(m_current, m_configWidget->m_cbConfig->currentText()));
				int currentIndex = m_configWidget->m_cbConfig->currentIndex();
				int newIndex = 0;
				if(currentIndex == 0 )
					newIndex = 1;
				KileTool::setConfigName(m_current, m_configWidget->m_cbConfig->itemText(newIndex), m_config);
				m_config->reparseConfiguration(); // FIXME should be not needed
				updateConfiglist();
				switchTo(m_current, false);
			}
		}
		else {
			KMessageBox::error(this, i18n("You need at least one configuration for each tool."), i18n("Cannot Remove Configuration"));
		}
	}

	void ToolConfig::switchClass(const QString & cls)
	{
		if(m_map["class"] != cls) {
			setClass(cls);
			emit(changed());
		}
	}

	void ToolConfig::switchType(int index)
	{
		switch (index) {
			case 0 : m_map["type"] = "Process"; break;
			case 1 : m_map["type"] = "Konsole"; break;
			case 2 : m_map["type"] = "Part"; break;
			case 3 : m_map["type"] = "DocPart"; break;
			case 4 : m_map["type"] = "Sequence"; break;
			default : m_map["type"] = "Process"; break;
		}
		emit(changed());
	}

	void ToolConfig::setCommand(const QString & command) { m_map["command"] = command.trimmed(); }
	void ToolConfig::setOptions() { m_map["options"] = m_ptcw->m_options->toPlainText().trimmed(); }
	void ToolConfig::setLibrary(const QString & lib) { m_map["libName"] = lib.trimmed(); }
	void ToolConfig::setLibOptions(const QString & options) { m_map["libOptions"] = options.trimmed(); }
	void ToolConfig::setClassName(const QString & name) { m_map["className"] = name.trimmed(); }
	void ToolConfig::setState(const QString & state)
	{
		QString str = state.trimmed();
		if(str.isEmpty()) str = "Editor";
		m_map["state"] = str;
	}
	void ToolConfig::setSequence(const QString & sequence) { m_map["sequence"] = sequence.trimmed(); }
	void ToolConfig::setClose(bool on) { m_map["close"] = on ? "yes" : "no"; }
	void ToolConfig::setTarget(const QString & trg) { m_map["target"] = trg.trimmed(); }
	void ToolConfig::setRelDir(const QString & rd) { m_map["relDir"] = rd.trimmed(); }
	void ToolConfig::setLaTeXCheckRoot(bool ck) { m_map["checkForRoot"] = ck ? "yes" : "no"; }
	void ToolConfig::setLaTeXJump(bool ck) { m_map["jumpToFirstError"] = ck ? "yes" : "no"; }
	void ToolConfig::setLaTeXAuto(bool ck) { m_map["autoRun"] = ck ? "yes" : "no"; }
	void ToolConfig::setRunLyxServer(bool ck)
	{
		//KILE_DEBUG() << "setRunLyxServer";
		m_config->group("Tools").writeEntry("RunLyxServer", ck);
	}
	void ToolConfig::setFrom(const QString & from) { m_map["from"] = from.trimmed(); }
	void ToolConfig::setTo(const QString & to) { m_map["to"] = to.trimmed(); }
	void ToolConfig::setClass(const QString & cls) { m_map["class"] = cls.trimmed(); }
}

#include "toolconfigwidget.moc"
