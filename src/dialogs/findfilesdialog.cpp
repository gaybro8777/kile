/* This file is part of the kile project
   Copyright (C) 2001 Christoph Cullmann <cullmann@kde.org>
   Copyright (C) 2001 Joseph Wenninger <jowenn@kde.org>
   Copyright (C) 2001 Anders Lund <anders.lund@lund.tdcadsl.dk>
   Copyright (C) 2003 Jan-Marek Glogowski <glogow@stud.fbi.fh-darmstadt.de>
   Copyright (C) 2005 Holger Danielsson <holger.danielsson@versanet.de>
   Copyright (C) 2008 Michel Ludwig <michel.ludwig@kdemail.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Original from kdebase / kate

   changes: 2005-11-27 (dani)
    - add a search for all files of a Kile project
      (done with one grep command for each file)
    - dialog is now based on KDialogBase
    - an item of the resultbox ist opened when it's highlightened
      (no double click is needed anymore)
    - dialog is deleted after work to minimize resources
    - added additional search modes for environments, labels etc.
    - fixed some bugs (f.e. two slashes at the end of directory
      names, jumping to the wrong line, wrong pattern lists)
    - add some editable template modes to search for LaTeX commands
    - add some predined modes to search for environments, graphics,
      labels, and references, either all of them or some special ones

    (in other words: most parts have changed to work perfectly with Kile ...)
*/

// 2007-03-12 dani
//  - use KileDocument::Extensions

#include "dialogs/findfilesdialog.h"

#include <QCheckBox>
#include <QCursor>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QObject>
#include <QPushButton>
#include <QRegExp>
#include <QVBoxLayout>

#include <KProcess>
#include <KAcceleratorManager>
#include <KApplication>
#include <KComboBox>
#include <KDialogButtonBox>
#include <KFileDialog>
#include <KIconLoader>
#include <KLineEdit>
#include <KLocale>
#include <KMessageBox>
#include <KShell>
#include <KUrlCompletion>
#include <KUrlRequester>

#include "kiledebug.h"
#include "kileconfig.h"
#include "kileproject.h"
#include "kiledocmanager.h"
#include "kileextensions.h"

namespace KileDialog {

FindFilesDialog::FindFilesDialog(QWidget *parent, KileInfo *ki, KileGrep::Mode mode, const char *name)
		: KDialog(parent),
		m_ki(ki), m_mode(mode), m_proc(NULL), m_grepJobs(0)
{
	setObjectName(name);
	setCaption(QString());
	setModal(false);
	setButtons(0);
	setDefaultButton(NoDefault);
	showButtonSeparator(false);

	QWidget *page = new QWidget(this);
	setMainWidget(page);
	//setWFlags( Qt::WStyle_StaysOnTop );

	// build dialog
	QVBoxLayout *vbox = new QVBoxLayout();
	vbox->setMargin(0);
	vbox->setSpacing(KDialog::spacingHint());
	page->setLayout(vbox);

	// project groupbox
	QGroupBox *projectgroup = new QGroupBox(i18n("Project"), page);
	QGridLayout *projectgrouplayout = new QGridLayout();
	projectgrouplayout->setMargin(KDialog::marginHint());
	projectgrouplayout->setSpacing(KDialog::spacingHint());
	projectgrouplayout->setAlignment(Qt::AlignTop);
	projectgroup->setLayout(projectgrouplayout);

	QLabel *project_label = new QLabel(i18n("Name:"), projectgroup);
	int labelwidth = project_label->sizeHint().width();

	QLabel *projectdir_label = new QLabel(i18n("Directory:"), projectgroup);
	if(projectdir_label->sizeHint().width() > labelwidth) {
		labelwidth = projectdir_label->sizeHint().width();
	}

	projectname_label = new QLabel(projectgroup);
	projectdirname_label = new QLabel(projectgroup);

	projectgrouplayout->addWidget(project_label, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
	projectgrouplayout->addWidget(projectname_label, 0, 1, Qt::AlignLeft | Qt::AlignVCenter);
	projectgrouplayout->addWidget(projectdir_label, 1, 0, Qt::AlignLeft | Qt::AlignVCenter);
	projectgrouplayout->addWidget(projectdirname_label, 1, 1, Qt::AlignLeft | Qt::AlignVCenter);
	projectgrouplayout->setColumnStretch(1, 1);

	// search groupbox
	QGroupBox *searchgroup = new QGroupBox(i18n("Search"), page);
	QGridLayout *searchgrouplayout = new QGridLayout();
	searchgrouplayout->setMargin(KDialog::marginHint());
	searchgrouplayout->setSpacing(KDialog::spacingHint());
	searchgrouplayout->setAlignment(Qt::AlignTop);
	searchgroup->setLayout(searchgrouplayout);

	QLabel *pattern_label = new QLabel(i18n("Pattern:"), searchgroup);
	if(pattern_label->sizeHint().width() > labelwidth) {
		labelwidth = pattern_label->sizeHint().width();
	}

	pattern_combo = new KComboBox(true, searchgroup);
	pattern_combo->setInsertPolicy(KComboBox::NoInsert);
	pattern_combo->setFocus();
	pattern_combo->setMinimumSize(pattern_combo->sizeHint());
	pattern_label->setBuddy(pattern_combo);

	QLabel *template_label = new QLabel(i18n("Template:"), searchgroup);
	if(template_label->sizeHint().width() > labelwidth) {
		labelwidth = template_label->sizeHint().width();
	}

	QStringList templatemode_list;
	templatemode_list << i18n("Normal")
	<< i18n("Command")
	<< i18n("Command[]")
	<< i18n("Environment")
	<< i18n("Image")
	<< i18n("Label")
	<< i18n("Reference")
	<< i18n("File");

	QHBoxLayout *template_layout = new QHBoxLayout();
	template_layout->setMargin(0);
	template_layout->setSpacing(KDialog::spacingHint());
	template_combo = new KComboBox(false, searchgroup);
	template_combo->addItems(templatemode_list);
	template_combo->adjustSize();
	template_combo->setFixedSize(template_combo->size());
	template_layout->addWidget(template_combo);
	m_lastTemplateIndex = 0;

	template_edit = new KLineEdit(searchgroup);
	template_edit->setText("%s");
	template_edit->setMinimumSize(template_edit->sizeHint());
	template_label->setBuddy(template_edit);
	template_layout->addWidget(template_edit);

	searchgrouplayout->addWidget(pattern_label, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
	searchgrouplayout->addWidget(pattern_combo, 0, 1);
	searchgrouplayout->addWidget(template_label, 1, 0, Qt::AlignLeft | Qt::AlignVCenter);
	searchgrouplayout->addLayout(template_layout, 1, 1);

	// filter groupbox
	QGroupBox *filtergroup = new QGroupBox(i18n("Directory Options"), page);
	QGridLayout *filtergrouplayout = new QGridLayout();
	filtergrouplayout->setMargin(KDialog::marginHint());
	filtergrouplayout->setSpacing(KDialog::spacingHint());
	filtergrouplayout->setAlignment(Qt::AlignTop);
	filtergroup->setLayout(filtergrouplayout);

	QLabel *files_label = new QLabel(i18n("Filter:"), filtergroup);
	if (files_label->sizeHint().width() > labelwidth) {
		labelwidth = files_label->sizeHint().width();
	}

	filter_combo = new KComboBox(true, filtergroup);
	filter_combo->setMinimumSize(filter_combo->sizeHint());
	files_label->setBuddy(filter_combo->focusProxy());

	QLabel *dir_label = new QLabel(i18n("Directory:"), filtergroup);
	if (dir_label->sizeHint().width() > labelwidth) {
		labelwidth = dir_label->sizeHint().width();
	}

	dir_combo = new KUrlRequester(new KComboBox(true, filtergroup), filtergroup);
	dir_combo->setObjectName("dir combo");
	dir_combo->completionObject()->setMode(KUrlCompletion::DirCompletion);
	dir_combo->setMode(KFile::Directory | KFile::LocalOnly | KFile::ExistingOnly);
	dir_label->setBuddy(dir_combo);

	recursive_box = new QCheckBox(i18n("Scan directories recursively"), filtergroup);
	recursive_box->setMinimumWidth(recursive_box->sizeHint().width());

	filtergrouplayout->addWidget(files_label, 0, 0);
	filtergrouplayout->addWidget(filter_combo, 0, 1);
	filtergrouplayout->addWidget(dir_label, 1, 0);
	filtergrouplayout->addWidget(dir_combo, 1, 1);
	filtergrouplayout->addWidget(recursive_box, 2, 1);
	filtergrouplayout->setColumnStretch(1, 1);

	// result box
	resultbox = new QListWidget(page);
	resultbox->setMinimumHeight(150);

	// button box
	KDialogButtonBox *actionbox = new KDialogButtonBox(page);
	search_button = actionbox->addButton(i18n("&Search"), QDialogButtonBox::ActionRole, this, SLOT(slotSearch()));
	search_button->setDefault(true);
	search_button->setEnabled(false);
	search_button->setIcon(KIcon("edit-find"));
	clear_button = actionbox->addButton(i18n("&Clear"), QDialogButtonBox::ActionRole, this, SLOT(slotClear()));
	clear_button->setEnabled(false);
	clear_button->setIcon(KIcon("edit-clear-locationbar"));
	close_button = actionbox->addButton(KStandardGuiItem::close(), QDialogButtonBox::DestructiveRole, this, SLOT(slotClose()));

	// adjust labels
	project_label->setFixedWidth(labelwidth);
	projectdir_label->setFixedWidth(labelwidth);
	pattern_label->setFixedWidth(labelwidth);
	template_label->setFixedWidth(labelwidth);
	files_label->setFixedWidth(labelwidth);
	dir_label->setFixedWidth(labelwidth);

	if (m_mode == KileGrep::Project) {
		filtergroup->hide();
		vbox->addWidget(projectgroup);
		vbox->addWidget(searchgroup);
	}
	else {
		projectgroup->hide();
		vbox->addWidget(searchgroup);
		vbox->addWidget(filtergroup);
	}
	vbox->addWidget(resultbox);
	vbox->addWidget(actionbox);

	// Produces error messages like
	// QListBox::property( "text" ) failed:
	//  property invalid or does not exist
	// Anyone an idea?
	KAcceleratorManager::manage(this);

	pattern_combo->setWhatsThis(
		i18n("Enter the regular expression you want to search for here.<br>"
				 "Possible meta characters are:<br>"
				 "<ul>"
				 "<li>&nbsp;<b>.</b> - Matches any character</li>"
				 "<li>&nbsp;<b>^</b> - Matches the beginning of a line</li>"
				 "<li>&nbsp;<b>$</b> - Matches the end of a line</li>"
				 "<li>&nbsp;<b>\\\\\\&lt;</b> - Matches the beginning of a word</li>"
				 "<li>&nbsp;<b>\\\\\\&gt;</b> - Matches the end of a word</li>"
				 "</ul>"
				 "The following repetition operators exist:"
				 "<ul>"
				 "<li>&nbsp;<b>?</b> - The preceding item is matched at most once</li>"
				 "<li>&nbsp;<b>*</b> - The preceding item is matched zero or more times</li>"
				 "<li>&nbsp;<b>+</b> - The preceding item is matched one or more times</li>"
				 "<li>&nbsp;<b>{<i>n</i>}</b> - The preceding item is matched exactly <i>n</i> times</li>"
				 "<li>&nbsp;<b>{<i>n</i>,}</b> - The preceding item is matched <i>n</i> or more times</li>"
				 "<li>&nbsp;<b>{,<i>n</i>}</b> - The preceding item is matched at most <i>n</i> times</li>"
				 "<li>&nbsp;<b>{<i>n</i>,<i>m</i>}</b> - The preceding item is matched at least <i>n</i>, "
				 "but at most <i>m</i> times.</li>"
				 "</ul>"
				 "Furthermore, backreferences to bracketed subexpressions are "
				 "available via the notation \\\\<i>n</i>."
				));
	filter_combo->setWhatsThis(
		i18n("Enter the file name pattern of the files to search here. "
				 "You may give several patterns separated by commas."));
	template_combo->setWhatsThis(
		i18n("Choose one search mode. For the first modes, the search pattern is "
				 "build from the editable template, where '%s' is replaced by the given pattern.<br><br>"
				 "There are additional fixed predefined modes for environments, graphics, labels, references "
				 "and input files. If the pattern is empty, Kile will search for all commands of this mode. "
				 "If a pattern is given, it will be inserted as a parameter. F.e., in environment mode with "
				 "pattern 'center', Kile will search for '\\begin{center}' and in graphics mode with "
				 "pattern '.*\\.png', Kile will search for all png files."));
	template_edit->setWhatsThis(
		i18n("For the first three modes you can choose a template for the pattern from the combo box "
				 "and edit it here. The string %s in the template is replaced "
				 "by the pattern input field, resulting in the regular expression "
				 "to search for. In all other modes this template is ignored."));
	dir_combo->setWhatsThis(
		i18n("Enter the directory which contains the files you want to search in."));
	recursive_box->setWhatsThis(
		i18n("Check this box to search in all subdirectories."));
	resultbox->setWhatsThis(
		i18n("The results of the grep run are listed here. Select a "
				 "filename/line number combination with a mouse click on the item "
				 "or with the cursor to show the respective line in the editor."));

	// read config and setup dialog for both modes
	readConfig();
	if (m_mode == KileGrep::Directory) {
		setCaption(i18n("Find in Files"));
		setupDirectory();
	}
	else {
		setCaption(i18n("Find in Project"));
		setupProject();
	}

	pattern_combo->setEditText(QString());
	template_edit->setText(m_TemplateList[0]);
	slotPatternTextChanged(QString());

	connect(pattern_combo->lineEdit(), SIGNAL(textChanged(const QString &)),
					SLOT(slotPatternTextChanged(const QString &)));
	connect(template_combo, SIGNAL(activated(int)),
					SLOT(slotTemplateActivated(int)));
	connect(resultbox, SIGNAL(currentTextChanged(const QString&)),
					SLOT(slotItemSelected(const QString&)));

	connect(this, SIGNAL(finished()), SLOT(slotFinished()));

	resize(450, sizeHint().height());
	KILE_DEBUG() << "==FindFilesDialog (create dialog)=============================";
}

FindFilesDialog::~FindFilesDialog()
{
	KILE_DEBUG() << "==FindFilesDialog (delete dialog)=============================";
	writeConfig();
}

///////////////////// config /////////////////////

void FindFilesDialog::readConfig()
{
	pattern_combo->addItems(readList(KileGrep::SearchItems));

	QString labels = getCommandList(KileDocument::CmdAttrLabel);
	QString references = getCommandList(KileDocument::CmdAttrReference);
	m_TemplateList = readList(KileGrep::SearchTemplates) ;
	if(m_TemplateList.count() != 3) {
		m_TemplateList.clear();
		m_TemplateList << "%s" << "\\\\%s\\{" << "\\\\%s(\\[[^]]*\\])?\\{";
	}
	m_TemplateList << "\\\\begin\\{"                             // to be closed with "%s\\}"
	<< "\\\\includegraphics(\\[[^]]*\\])?\\{"
	<< "\\\\(label" + labels + ")\\{"
	<< "\\\\(ref|pageref|vref|vpageref|fref|Fref|eqref" + references + ")(\\[[^]]*\\])?\\{"
	<< "\\\\(input|include)\\{"
	;

	if (m_mode == KileGrep::Directory) {
		dir_combo->comboBox()->addItems(readList(KileGrep::SearchPaths));
		recursive_box->setChecked(KileConfig::grepRecursive());
	}
}

void FindFilesDialog::writeConfig()
{
	KileConfig::setLastSearchItems(getListItems(pattern_combo));

	QStringList list;
	list << m_TemplateList[0] << m_TemplateList[1] << m_TemplateList[2];
	KileConfig::setLastSearchTemplates(list);

	if(m_mode == KileGrep::Directory) {
		KileConfig::setLastSearchPaths(getListItems(dir_combo->comboBox()));
		KileConfig::setGrepRecursive(recursive_box->isChecked());
	}
}

///////////////////// setup search modes /////////////////////

void FindFilesDialog::setupDirectory()
{
	setDirName(QDir::home().absolutePath());

	// use a filter for 'find in files' dialog
	KileDocument::Extensions *extensions = m_ki->extensions();
	QString filter = extensions->latexDocumentFileFilter() + '\n'
									 + extensions->latexPackageFileFilter() + '\n'
									 + extensions->bibtexFileFilter() + '\n'
									 + extensions->metapostFileFilter() + '\n'
									 + "*|" + i18n("All Files");
	setFilter(filter);
}

void FindFilesDialog::setupProject()
{
	KileProject *project = m_ki->docManager()->activeProject();
	if(project) {
		m_projectOpened = true;
		m_projectdir = project->baseURL().path();
		projectname_label->setText(project->name());
		projectdirname_label->setText(m_projectdir);

		m_projectfiles.clear();
		m_projectfiles = m_ki->docManager()->getProjectFiles();
	}
	else {
		m_projectOpened = false;
		projectname_label->setText(i18n("no project opened"));
		projectdirname_label->setText(QString());
	}
}

///////////////////// read entries /////////////////////

QStringList FindFilesDialog::readList(KileGrep::List listtype)
{
	QStringList strings, result;

	bool stripSlash = false;
	switch (listtype) {
		case KileGrep::SearchItems:
			strings = KileConfig::lastSearchItems();
			break;
		case KileGrep::SearchPaths:
			strings = KileConfig::lastSearchPaths();
			stripSlash = true;
			break;
		case KileGrep::SearchTemplates:
			strings = KileConfig::lastSearchTemplates();
			break;
	}

	while (strings.count() > 0) {
		if(stripSlash && strings[0].right(1) == "/") {
			strings[0].truncate(strings[0].length() - 1);
		}
		if(!strings[0].isEmpty()) {
			result.append(strings[0]);
		}
		strings.removeAll(strings[0]);
	}
	return result;
}

///////////////////// item selected /////////////////////

void FindFilesDialog::slotItemSelected(const QString& item)
{
	KILE_DEBUG() << "\tgrep: start item selected";
	int pos;
	QString filename, linenumber;

	QString str = item;
	if((pos = str.indexOf(':')) != -1) {
		filename = str.left(pos);
		str = str.right(str.length() - 1 - pos);
		if((pos = str.indexOf(':')) != -1) {
			linenumber = str.left(pos);
			if(m_mode == KileGrep::Project) {
				emit itemSelected(m_projectdir + '/' + filename, linenumber.toInt());
			}
			else {
				emit itemSelected(dir_combo->comboBox()->itemText(0) + '/' + filename, linenumber.toInt());
			}
		}
	}
}

///////////////////// grep /////////////////////

void FindFilesDialog::startGrep()
{
	m_proc = new KProcess(this);
	m_proc->setOutputChannelMode(KProcess::SeparateChannels);

	m_buf.clear();
	m_errbuf.clear();
	QString command;
	if (m_mode == KileGrep::Project) {
		command = buildProjectCommand() + ' ' + KShell::quoteArg(m_projectfiles[m_grepJobs-1]);
	}
	else {
		command = buildFilesCommand();
	}
	KILE_DEBUG() << "\tgrep (project): " <<  command;
	(*m_proc) << KShell::splitArgs(command);

	m_grepJobs--;

	connect(m_proc, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processExited(int, QProcess::ExitStatus)));
	connect(m_proc, SIGNAL(readyReadStandardOutput()), this, SLOT(processStandardOutputReady()));
	connect(m_proc, SIGNAL(readyReadStandardError()), this, SLOT(processErrorOutputReady()));

	m_proc->start();
}

void FindFilesDialog::processOutput()
{
	int pos;
	while((pos = m_buf.indexOf('\n')) != -1) {
		QString item = m_buf.left(pos);
		if(!item.isEmpty()) {
			if(m_mode == KileGrep::Project) {
				if (item.indexOf(m_projectdir + '/') == 0) {
					new QListWidgetItem(item.mid(m_projectdir.length() + 1), resultbox);
				}
				else {
					new QListWidgetItem(item, resultbox);
				}
			}
			else {
				new QListWidgetItem(item.mid(dir_combo->url().path().length() + 1), resultbox);
			}
		}
		m_buf = m_buf.right(m_buf.length() - pos - 1);
	}
	kapp->processEvents();
}

void FindFilesDialog::processStandardOutputReady()
{
	QByteArray outputBuffer = m_proc->readAllStandardOutput();
	m_buf += QString::fromLocal8Bit(outputBuffer.data(), outputBuffer.size());
	kDebug() << "buffer " << m_buf;
	processOutput();
}

void FindFilesDialog::processErrorOutputReady()
{
	QByteArray outputBuffer = m_proc->readAllStandardError();
	m_errbuf += QString::fromLocal8Bit(outputBuffer.data(), outputBuffer.size());
	kDebug() << "err buffer " << m_errbuf;
}

void FindFilesDialog::processExited(int /*exitCode*/, QProcess::ExitStatus /*exitStatus*/)
{
	if(!m_errbuf.isEmpty()) {
		KMessageBox::information(parentWidget(), i18n("<strong>Error:</strong><p>") + m_errbuf, i18n("Grep Tool Error"));
		m_errbuf.clear();
	}
	else {
		finish();
	}
}

void FindFilesDialog::finish()
{
	m_buf += '\n';
	processOutput();
	if(m_proc) {
		m_proc->deleteLater();
		m_proc = NULL;
	}

	if (shouldRestart()) {
		startGrep();
	}
	else {
		updateLists();

		resultbox->unsetCursor();
		clear_button->setEnabled(resultbox->count() > 0);
		search_button->setText(i18n("&Search"));
		slotPatternTextChanged(pattern_combo->lineEdit()->text());
	}
}

void FindFilesDialog::updateLists()
{
	updateListItems(pattern_combo);
	if(m_mode == KileGrep::Directory) {
		updateListItems(dir_combo->comboBox());
	}
}

///////////////////// build commands /////////////////////

QString FindFilesDialog::getPattern()
{
	QString pattern;
	int template_mode = template_combo->currentIndex();
	if (template_mode < KileGrep::tmEnv) {
		pattern = template_edit->text();
		if (pattern.isEmpty()) {
			pattern = pattern_combo->currentText();
		}
		else {
			pattern.replace("%s", pattern_combo->currentText());
		}
	}
	else {
		pattern = m_TemplateList[template_mode];
		if (!pattern_combo->currentText().isEmpty()) {
			pattern += pattern_combo->currentText()  + "\\}";
		}
	}

	return pattern;
}

QString FindFilesDialog::getShellPattern()
{
	QString pattern = getPattern();
	pattern.replace('\'', "'\\''");
	return KShell::quoteArg(pattern);
}


QString FindFilesDialog::buildFilesCommand()
{
	QString files, files_temp;

	if(filter_combo->currentIndex() >= 0) {
		files_temp = m_filterList[filter_combo->currentIndex()];
	}
	else {
		files_temp = filter_combo->currentText();
	}

	QStringList tokens = files_temp.split(' ', QString::SkipEmptyParts);
	QStringList::Iterator it = tokens.begin();
	if (it != tokens.end()) {
		files = " '" + (*it) + '\'';
		++it;
	}

	for(; it != tokens.end(); ++it) {
		files = files + " -o -name " + '\'' + (*it) + '\'';
	}

	QString shell_command;
	shell_command += "find ";
	shell_command += KShell::quoteArg(dir_combo->url().pathOrUrl());
	shell_command += " \\( -name ";
	shell_command += files;
	shell_command += " \\)";
	if (!recursive_box->isChecked()) {
		shell_command += " -maxdepth 1";
	}
	shell_command += " -exec grep -n -E -I -H -e " + getShellPattern() + " {} \\;";

	return shell_command;
}

QString FindFilesDialog::buildProjectCommand()
{
	return "grep -n -E -I -H -e " + getShellPattern();
}

///////////////////// Search /////////////////////

void FindFilesDialog::slotSearch()
{
	KILE_DEBUG() << "\tgrep: start slot search";
	slotClear();

	if (template_combo->currentIndex() < KileGrep::tmEnv && pattern_combo->currentText().isEmpty()) {
		return;
	}

	if (m_proc && m_proc->state() == QProcess::Running) {
		m_proc->kill();
		return;
	}

	KILE_DEBUG() << "\tgrep: start new search";
	QRegExp re(getPattern());
	if(!re.isValid()) {
		KMessageBox::error(m_ki->mainWindow(), i18n("Invalid regular expression: %1", re.errorString()), i18n("Grep Tool Error"));
		return;
	}

	resultbox->setCursor(QCursor(Qt::WaitCursor));
	search_button->setText(i18n("&Cancel"));
	if (template_combo->currentIndex() < KileGrep::tmEnv) {
		m_TemplateList[m_lastTemplateIndex] = template_edit->text();
	}

	// start grep command
	m_grepJobs = (m_mode == KileGrep::Project) ? m_projectfiles.count() : 1;
	startGrep();
}

void FindFilesDialog::slotSearchFor(const QString &pattern)
{
	slotClear();
	pattern_combo->setEditText(pattern);
	slotSearch();
}

void FindFilesDialog::slotClear()
{
	//KILE_DEBUG() << "\tgrep: slot clear";
	clearGrepJobs();
	resultbox->clear();
	finish();
}

void FindFilesDialog::slotClose()
{
	//KILE_DEBUG() << "\tgrep: slot close";
	clearGrepJobs();
	finish();
	delayedDestruct();
}

void FindFilesDialog::slotFinished()
{
	//KILE_DEBUG() << "\tgrep: slot finished";
	finish();
	delayedDestruct();
}

///////////////////// templates /////////////////////

void FindFilesDialog::slotPatternTextChanged(const QString &)
{
	updateWidgets();
}

void FindFilesDialog::slotTemplateActivated(int index)
{
	if (index < KileGrep::tmEnv) {
		m_TemplateList[m_lastTemplateIndex] = template_edit->text();
		template_edit->setText(m_TemplateList[index]);
	}
	else {
		template_edit->setText(QString());
	}
	m_lastTemplateIndex = index;

	updateWidgets();
}

void FindFilesDialog::updateWidgets()
{
	bool search_state = (m_mode == KileGrep::Directory) || (m_mode == KileGrep::Project && m_projectOpened);

	if (template_combo->currentIndex() < KileGrep::tmEnv) {
		template_edit->setEnabled(true);
		search_button->setEnabled(search_state && !pattern_combo->currentText().isEmpty());
	}
	else {
		template_edit->setEnabled(false);
		search_button->setEnabled(search_state);
	}
}

///////////////////// directory /////////////////////

void FindFilesDialog::setDirName(const QString &dir)
{
	KComboBox *combo = dir_combo->comboBox();

	if (findListItem(combo, dir) < 0) {
		combo->addItem(dir);
	}
	if (combo->itemText(0) != dir) {
		slotClear();
	}
}

///////////////////// filter /////////////////////

void FindFilesDialog::setFilter(const QString &filter)
{
	m_filterList.clear();
	filter_combo->clear();
	if (!filter.isEmpty()) {
		QStringList filter_lst = filter.split('\n');
		for (QStringList::Iterator it = filter_lst.begin(); it != filter_lst.end(); ++it) {
			QStringList filter_split = (*it).split('|');
			m_filterList.append(filter_split[0]);
			filter_combo->addItem(filter_split[1]);
		}
	}
}

void FindFilesDialog::appendFilter(const QString &name, const QString &filter)
{
	filter_combo->addItem(name);
	m_filterList.append(filter);
}

///////////////////// template /////////////////////

void FindFilesDialog::appendTemplate(const QString &name, const QString &regexp)
{
	template_combo->addItem(name);
	m_TemplateList.append(regexp);
}

void FindFilesDialog::clearTemplates()
{
	template_combo->clear();
	m_TemplateList.clear();
}

///////////////////// KComboBox /////////////////////

QStringList FindFilesDialog::getListItems(KComboBox *combo)
{
	QStringList list;
	for (int i = 0; i < combo->count() && i < KILEGREP_MAX; ++i) {
		list.append(combo->itemText(i));
	}
	return list;
}

int FindFilesDialog::findListItem(KComboBox *combo, const QString &s)
{
	for (int i = 0; i < combo->count(); ++i) {
		if (combo->itemText(i) == s) {
			return i;
		}
	}
	return -1;
}

void FindFilesDialog::updateListItems(KComboBox *combo)
{
	QString s = combo->currentText();
	if (s.isEmpty()) {
		return;
	}

	int index = findListItem(combo, s);
	if (index > 0) {                                 // combo already contains s
		combo->removeItem(index);                   // remove this item
	}
	else {
		if (index == -1) {                          // combo doesn't contain s
			if (combo->count() >= KILEGREP_MAX) {
				combo->removeItem(combo->count() - 1);   // remove last item
			}
		}
	}

	if(index != 0) {
		combo->insertItem(0, s);                    // insert this item as first item
		combo->setCurrentIndex(0);                   // and select it
	}
}

///////////////////// template /////////////////////

QString FindFilesDialog::getCommandList(KileDocument::CmdAttribute attrtype)
{
	QStringList cmdlist;
	QStringList::ConstIterator it;

	// get info about user defined references
	KileDocument::LatexCommands *cmd = m_ki->latexCommands();
	cmd->commandList(cmdlist, attrtype, true);

	// build list of references
	QString commands;
	for (it = cmdlist.begin(); it != cmdlist.end(); ++it) {
		commands += '|' + (*it).mid(1);
	}
	return commands;
}

}

#include "findfilesdialog.moc"