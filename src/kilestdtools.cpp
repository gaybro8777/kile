/**************************************************************************************
    begin                : Thu Nov 27 2003
    copyright            : (C) 2003 by Jeroen Wijnhout (Jeroen.Wijnhout@kdemail.net)
                           (C) 2011 by Michel Ludwig (michel.ludwig@kdemail.net)
 **************************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kilestdtools.h"

#include <QFileInfo>
#include <QRegExp>

#include <KAction>
#include <KActionCollection>
#include <KConfig>
#include <KLocale>
#include <KStandardDirs>
#include <KProcess>

#include "kileconfig.h"
#include "kiletool.h"
#include "kiletoolmanager.h"
#include "kiletool_enums.h"
#include "kileinfo.h"
#include "kilelistselector.h"
#include "kiledocmanager.h"
#include "documentinfo.h"
#include "outputinfo.h"

namespace KileTool
{
	Factory::Factory(Manager *mngr, KConfig *config, KActionCollection *actionCollection)
	: m_manager(mngr), m_config(config), m_actionCollection(actionCollection)
	{
		m_standardToolConfigurationFileName = KGlobal::dirs()->findResource("appdata", "kilestdtools.rc");
	}

	Factory::~Factory()
	{
	}

	static const QString shortcutGroupName = "Shortcuts";

	Base* Factory::create(const QString& toolName, const QString& config, bool prepare /* = true */)
	{
		KILE_DEBUG() << toolName << config << prepare;
		KileTool::Base *tool = NULL;
		//perhaps we can find the tool in the config file
		if (m_config->hasGroup(groupFor(toolName, m_config))) {
			KConfigGroup configGroup = m_config->group(groupFor(toolName, m_config));
			QString toolClass = configGroup.readEntry("class", QString());

			if(toolClass == "LaTeX") {
				tool = new LaTeX(toolName, m_manager, prepare);
			}
			else if(toolClass == "LaTeXpreview") {
				tool = new PreviewLaTeX(toolName, m_manager, prepare);
			}
			else if(toolClass == "LaTeXLivePreview") {
				tool = new LivePreviewLaTeX(toolName, m_manager, prepare);
			}
			else if(toolClass == "ForwardDVI") {
				tool = new ForwardDVI(toolName, m_manager, prepare);
			}
			else if(toolClass == "ViewHTML") {
				tool = new ViewHTML(toolName, m_manager, prepare);
			}
			else if(toolClass == "ViewBib") {
				tool = new ViewBib(toolName, m_manager, prepare);
			}
			else if(toolClass == "Base") {
				tool = new Base(toolName, m_manager, prepare);
			}
			else if(toolClass == "Compile") {
				tool = new Compile(toolName, m_manager, prepare);
			}
			else if(toolClass == "Convert") {
				tool = new Convert(toolName, m_manager, prepare);
			}
			else if(toolClass == "Archive") {
				tool = new Archive(toolName, m_manager, prepare);
			}
			else if(toolClass == "View") {
				tool = new View(toolName, m_manager, prepare);
			}
			else if(toolClass == "Sequence") {
				tool = new Sequence(toolName, m_manager, prepare);
			}
		}
		if(!tool) {
			return NULL;
		}

		if(!m_manager->configure(tool, config)) {
			delete tool;
			return NULL;
		}
		tool->setToolConfig(config);

		return tool;
	}

	void Factory::readStandardToolConfig()
	{
		KConfig stdToolConfig(m_standardToolConfigurationFileName, KConfig::NoGlobals);
		QStringList groupList = stdToolConfig.groupList();
		for(QStringList::iterator it = groupList.begin(); it != groupList.end(); ++it) {
			QString groupName = *it;
			if(groupName != shortcutGroupName) {
				KConfigGroup configGroup = stdToolConfig.group(groupName);
				m_config->deleteGroup(groupName);
				KConfigGroup newGroup = m_config->group(groupName);
				configGroup.copyTo(&newGroup, KConfigGroup::Persistent);
			}
		}
	}

	/////////////// LaTeX ////////////////

	LaTeX::LaTeX(const QString& tool, Manager *mngr, bool prepare) : Compile(tool, mngr, prepare)
	{
	}

	LaTeX::~LaTeX()
	{
	}

	int LaTeX::m_reRun = 0;

	// FIXME don't hardcode bbl and ind suffix here.
	bool LaTeX::updateBibs()
	{
		KileDocument::TextInfo *docinfo = manager()->info()->docManager()->textInfoFor(source());
		if(docinfo) {
			if(manager()->info()->allBibliographies(docinfo).count() > 0) {
				return needsUpdate(targetDir() + '/' + S() + ".bbl", manager()->info()->lastModifiedFile(docinfo));
			}
		}

		return false;
	}

	bool LaTeX::updateIndex()
	{
		KileDocument::TextInfo *docinfo = manager()->info()->docManager()->textInfoFor(source());
		if(docinfo) {
			QStringList pckgs = manager()->info()->allPackages(docinfo);
			if(pckgs.contains("makeidx")) {
				return needsUpdate(targetDir() + '/' + S() + ".ind", manager()->info()->lastModifiedFile(docinfo));
			}
		}

		return false;
	}
	
	bool LaTeX::updateAsy()
	{
		KileDocument::TextInfo *docinfo = manager()->info()->docManager()->textInfoFor(source());
		if(docinfo) {
			QStringList pckgs = manager()->info()->allPackages(docinfo);
			// As asymptote doesn't properly notify the user when it needs to be rerun, we run
			// it every time LaTeX is run (but only for m_reRun == 0 if LaTeX has to be rerun).
			if(pckgs.contains("asymptote")) {
				return true;
			}
		}
		return false;
	}

	bool LaTeX::finish(int r)
	{
		KILE_DEBUG() << "==bool LaTeX::finish(" << r << ")=====";

		if(r != Success) {
			return Compile::finish(r);
		}

		int nErrors = 0, nWarnings = 0;
		if(filterLogfile()) {
			checkErrors(nErrors,nWarnings);
		}
		
		if(readEntry("autoRun") == "yes") {
			checkAutoRun(nErrors, nWarnings);
		}

		return Compile::finish(r);
	}

	bool LaTeX::filterLogfile()
	{
		manager()->info()->outputFilter()->setSource(source());
		QString log = targetDir() + '/' + S() + ".log";
		return manager()->info()->outputFilter()->Run(log);
	}

	void LaTeX::checkErrors(int &nErrors, int &nWarnings)
	{
		int nBadBoxes = 0;
		
		manager()->info()->outputFilter()->sendProblems();
		manager()->info()->outputFilter()->getErrorCount(&nErrors, &nWarnings, &nBadBoxes);
		// work around the 0 cases as the i18np call can cause some confusion when 0 is passed to it (#275700)
		QString es = (nErrors == 0 ? i18n("0 errors") : i18np("1 error", "%1 errors", nErrors));
		QString ws = (nWarnings == 0 ? i18n("0 warnings") : i18np("1 warning", "%1 warnings", nWarnings));
		QString bs = (nBadBoxes == 0 ? i18n("0 badboxes") : i18np("1 badbox", "%1 badboxes", nBadBoxes));

		sendMessage(Info, i18nc("String displayed in the log panel showing the number of errors/warnings/badboxes",
		                        "%1, %2, %3").arg(es).arg(ws).arg(bs));

		//jump to first error
		if(!isPartOfLivePreview() && nErrors > 0 && (readEntry("jumpToFirstError") == "yes")) {
			connect(this, SIGNAL(jumpToFirstError()), manager(), SIGNAL(jumpToFirstError()));
			emit(jumpToFirstError());
		}
	}

	void LaTeX::configureLaTeX(KileTool::Base *tool, const QString& source)
	{
		tool->setSource(source, workingDir());
	}

	void LaTeX::configureBibTeX(KileTool::Base *tool, const QString& source)
	{
		tool->setSource(source, workingDir());
	}

	void LaTeX::configureMakeIndex(KileTool::Base *tool, const QString& source)
	{
		tool->setSource(source, workingDir());
	}

	void LaTeX::configureAsymptote(KileTool::Base *tool, const QString& source)
	{
		tool->setSource(source, workingDir());
	}

	void LaTeX::checkAutoRun(int nErrors, int nWarnings)
	{
		KILE_DEBUG() << "check for autorun, m_reRun is " << m_reRun;
		if(m_reRun >= 2) {
			KILE_DEBUG() << "Already rerun twice, doing nothing.";
			m_reRun = 0;
			return;
		}
		if(nErrors > 0) {
			KILE_DEBUG() << "Errors found, not running again.";
			m_reRun = 0;
			return;
		}
		bool reRunWarningFound = false;
		//check for "rerun LaTeX" warnings
		if(nWarnings > 0) {
			int sz =  manager()->info()->outputInfo()->size();
			for(int i = 0; i < sz; ++i) {
				if ((*manager()->info()->outputInfo())[i].type() == LatexOutputInfo::itmWarning
				&&  (*manager()->info()->outputInfo())[i].message().contains("Rerun")) {
					reRunWarningFound = true;
					break;
				}
			}
		}

		bool asy = (m_reRun == 0) && updateAsy();
		bool bibs = updateBibs();
		bool index = updateIndex();
		KILE_DEBUG() << "asy:" << asy << "bibs:" << bibs << "index:" << index << "reRunWarningFound:" << reRunWarningFound;
		// Currently, we don't properly detect yet whether asymptote has to be run.
		// So, if asymtote figures are present, we run it each time after the first LaTeX run.
		bool reRun = (asy || bibs || index || reRunWarningFound);
		KILE_DEBUG() << "reRun:" << reRun;

		if(reRun) {
			KILE_DEBUG() << "rerunning LaTeX, m_reRun is now " << m_reRun;
			Base *tool = manager()->createTool(name(), toolConfig());
			configureLaTeX(tool, source());
			// e.g. for LivePreview, it is necessary that the paths are copied to child processes
			tool->copyPaths(this);
			runChildNext(tool);
		}

		if(bibs) {
			KILE_DEBUG() << "need to run BibTeX";
			Base *tool = manager()->createTool("BibTeX", QString());
			configureBibTeX(tool, targetDir() + '/' + S() + '.' + tool->from());
			// e.g. for LivePreview, it is necessary that the paths are copied to child processes
			tool->copyPaths(this);
			runChildNext(tool);
		}

		if(index) {
			KILE_DEBUG() << "need to run MakeIndex";
			Base *tool = manager()->createTool("MakeIndex", QString());
			KILE_DEBUG() << targetDir() << S() << tool->from();
			configureMakeIndex(tool, targetDir() + '/' + S() + '.' + tool->from());
			// e.g. for LivePreview, it is necessary that the paths are copied to child processes
			tool->copyPaths(this);
			runChildNext(tool);
		}

		if(asy) {
			KILE_DEBUG() << "need to run asymptote";
			int sz = manager()->info()->allAsyFigures().size();
			for(int i = sz -1; i >= 0; --i) {
			  Base *tool = manager()->createTool("Asymptote", QString());
			  configureAsymptote(tool, targetDir() + '/' + S() + "-" + QString::number(i + 1) + '.' + tool->from());
			  // e.g. for LivePreview, it is necessary that the paths are copied to child processes
			  tool->copyPaths(this);
			  KILE_DEBUG();
			  KILE_DEBUG() << "calling manager()->runNext(";
			  runChildNext(tool);
			}
		}

		if(reRun) {
			m_reRun++;
		}
		else {
			m_reRun = 0;
		}
	}
	
	
	/////////////// PreviewLaTeX (dani) ////////////////

	PreviewLaTeX::PreviewLaTeX(const QString& tool, Manager *mngr, bool prepare) : LaTeX(tool, mngr, prepare)
	{
	}

	// PreviewLatex makes three steps:
	// - filterLogfile()  : parse logfile and read info into InfoLists
	// - updateInfoLists(): change entries of temporary file into normal tex file
	// - checkErrors()    : count errors and warnings and emit signals   
	bool PreviewLaTeX::finish(int r)
	{
		KILE_DEBUG() << "==bool PreviewLaTeX::finish(" << r << ")=====";
		
		int nErrors = 0, nWarnings = 0;
		if(filterLogfile()) {
			manager()->info()->outputFilter()->updateInfoLists(m_filename,m_selrow,m_docrow);
			checkErrors(nErrors,nWarnings);
		}
		
		return Compile::finish(r);
	}

	void PreviewLaTeX::setPreviewInfo(const QString &filename, int selrow,int docrow)
	{
		m_filename = filename;
		m_selrow = selrow;
		m_docrow = docrow;
	}

	/////////////// LivePreviewLaTeX ////////////////

	LivePreviewLaTeX::LivePreviewLaTeX(const QString& tool, Manager *mngr, bool prepare)
	: LaTeX(tool, mngr, prepare)
	{
	}

	bool LivePreviewLaTeX::updateBibs()
	{
		return LaTeX::updateBibs();
	}

	void LivePreviewLaTeX::configureLaTeX(KileTool::Base *tool, const QString& source)
	{
		LaTeX::configureLaTeX(tool, source);
		tool->setTargetDir(targetDir());
	}

	void LivePreviewLaTeX::configureBibTeX(KileTool::Base *tool, const QString& source)
	{
		tool->setSource(source, targetDir());
	}

	void LivePreviewLaTeX::configureMakeIndex(KileTool::Base *tool, const QString& source)
	{
		tool->setSource(source, targetDir());
	}

	void LivePreviewLaTeX::configureAsymptote(KileTool::Base *tool, const QString& source)
	{
		tool->setSource(source, targetDir());
	}
	// PreviewLatex makes three steps:
	// - filterLogfile()  : parse logfile and read info into InfoLists
	// - updateInfoLists(): change entries of temporary file into normal tex file
	// - checkErrors()    : count errors and warnings and emit signals   
// 	bool LivePreviewLaTeX::finish(int r)
// 	{
// 		KILE_DEBUG() << "==bool PreviewLaTeX::finish(" << r << ")=====";
// 		
// 		int nErrors = 0, nWarnings = 0;
// 		if(filterLogfile()) {
// 			manager()->info()->outputFilter()->updateInfoLists(m_filename,m_selrow,m_docrow);
// 			checkErrors(nErrors,nWarnings);
// 		}
// 		
// 		return Compile::finish(r);
// 	}
// 
// 	void LivePreviewLaTeX::setPreviewInfo(const QString &filename, int selrow,int docrow)
// 	{
// 		m_filename = filename;
// 		m_selrow = selrow;
// 		m_docrow = docrow;
// 	}


	ForwardDVI::ForwardDVI(const QString& tool, Manager *mngr, bool prepare) : View(tool, mngr, prepare)
	{
	}

	bool ForwardDVI::checkPrereqs ()
	{
          KProcess okularVersionTester;
	  okularVersionTester.setOutputChannelMode(KProcess::MergedChannels);
	  okularVersionTester.setProgram("okular", QStringList("--version"));
	  okularVersionTester.start();
	  
	  if (okularVersionTester.waitForFinished()){
	    QString output = okularVersionTester.readAll();
	    QRegExp regExp = QRegExp("Okular: (\\d+).(\\d+).(\\d+)");

	    if(output.contains(regExp)){
     	      int majorVersion = regExp.cap(1).toInt();
      	      int minorVersion = regExp.cap(2).toInt();
	      int veryMinorVersion = regExp.cap(3).toInt();
		      
	      //  see http://mail.kde.org/pipermail/okular-devel/2009-May/003741.html
	      // 	the required okular version is > 0.8.5
	      if(  majorVersion > 0  ||
		( majorVersion == 0 && minorVersion > 8 ) ||
		( majorVersion == 0 && minorVersion == 8 && veryMinorVersion > 5 ) ){
	    	; // everything okay
	      }
	      else{
  	        sendMessage(Error,i18n("The version %1.%2.%3 of okular is too old for ForwardDVI. Please update okular to version 0.8.6 or higher",majorVersion,minorVersion,veryMinorVersion));
	      }
	    }
	  }
	    // don't return false here because we don't know for sure if okular is used
	  return true;
	}

	bool ForwardDVI::determineTarget()
	{
		if (!View::determineTarget()) {
			return false;
		}

		int para = manager()->info()->lineNumber();
		KTextEditor::Document *doc = manager()->info()->activeTextDocument();

		if (!doc) {
			return false;
		}

		QString filepath = doc->url().toLocalFile();

		QString texfile = KUrl::relativePath(baseDir(),filepath);
		QString relativeTarget = "file:" + targetDir() + '/' + target() + "#src:" + QString::number(para+1) + ' ' + texfile; // space added, for files starting with numbers
		QString absoluteTarget = "file:" + targetDir() + '/' + target() + "#src:" + QString::number(para+1) + filepath;
		addDict("%dir_target", QString());
		addDict("%target", relativeTarget);
		addDict("%absolute_target", absoluteTarget);
		KILE_DEBUG() << "==KileTool::ForwardDVI::determineTarget()=============\n";
		KILE_DEBUG() << "\tusing  (absolute)" << absoluteTarget;
		KILE_DEBUG() << "\tusing  (relative)" << relativeTarget;

		return true;
	}

	ViewBib::ViewBib(const QString& tool, Manager *mngr, bool prepare) : View(tool, mngr, prepare)
	{
	}

	bool ViewBib::determineSource()
	{
		KILE_DEBUG() << "==ViewBib::determineSource()=======";
		if (!View::determineSource()) {
			return false;
		}

		QString path = source(true);
		QFileInfo info(path);

		//get the bibliographies for this source
		QStringList bibs = manager()->info()->allBibliographies(manager()->info()->docManager()->textInfoFor(path));
		KILE_DEBUG() << "\tfound " << bibs.count() << " bibs";
		if(bibs.count() > 0) {
			QString bib = bibs.front();
			if (bibs.count() > 1) {
				//show dialog
				bool bib_selected = false;
				KileListSelector *dlg = new KileListSelector(bibs, i18n("Select Bibliography"),i18n("Select a bibliography"));
				if (dlg->exec()) {
					bib = bibs[dlg->currentItem()];
					bib_selected = true;
					KILE_DEBUG() << "Bibliography selected : " << bib;
				}
				delete dlg;
				
				if(!bib_selected) {
					sendMessage(Warning, i18n("No bibliography selected."));
					return false;
				}
			}
			KILE_DEBUG() << "filename before: " << info.path();
			setSource(manager()->info()->checkOtherPaths(info.path(),bib + ".bib",KileInfo::bibinputs));	
		}
		else if(info.exists()) { //active doc is a bib file
			KILE_DEBUG() << "filename before: " << info.path();
			setSource(manager()->info()->checkOtherPaths(info.path(),info.fileName(),KileInfo::bibinputs));
		}
		else {
			sendMessage(Error, i18n("No bibliographies found."));
			return false;
		}
		return true;
	}

	ViewHTML::ViewHTML(const QString& tool, Manager *mngr, bool prepare) : View(tool, mngr, prepare)
	{
	}

	bool ViewHTML::determineTarget()
	{
		if (target().isNull()) {
			//setRelativeBaseDir(S());
			QString dir = readEntry("relDir");
			QString trg = readEntry("target");

			if(!dir.isEmpty()) {
				translate(dir);
				setRelativeBaseDir(dir);
			}

			if(!trg.isEmpty()) {
				translate(trg);
				setTarget(trg);
			}

			//auto-detect the file to view
			if(dir.isEmpty() && trg.isEmpty()) {
				QFileInfo file1 = QFileInfo(baseDir() + '/' + S() + "/index.html");
				QFileInfo file2 = QFileInfo(baseDir() + '/' + S() + ".html");

				bool read1 = file1.isReadable();
				bool read2 = file2.isReadable();

				if(!read1 && !read2) {
					sendMessage(Error, i18n("Unable to find %1 or %2; if you are trying to view some other HTML file, go to Settings->Configure Kile->Tools->ViewHTML->Advanced.", file1.absoluteFilePath(), file2.absoluteFilePath()));
					return false;
				}

				//both exist, take most recent
				if(read1 && read2) {
					read1 = file1.lastModified() > file2.lastModified();
					read2 = !read1;
				}

				if(read1) {
					dir = S();
					trg = "index.html";

					translate(dir);
					setRelativeBaseDir(dir);
					translate(trg);
					setTarget(trg);
				}
			}
		}

		return View::determineTarget();
	}
}

#include "kilestdtools.moc"

