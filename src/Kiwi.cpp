/*
 *  Copyright (c) 2011 Ahmad Amireh <ahmad@amireh.net>
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 */

#include "Kiwi.h"

extern int bsdiff(const char* inOld, const char* inNew, const char* inDest);

#if PIXY_PLATFORM == PIXY_PLATFORM_WIN32
  #include <io.h>
  #define ssize_t SSIZE_T
#endif
namespace Pixy
{
	Kiwi* Kiwi::__instance = 0;

	Kiwi::Kiwi() {
    mRepo = new Repository(Version(0,0,0));
	}

	Kiwi::~Kiwi() {
    delete mRepo;
    mApp = 0;
	}

	Kiwi* Kiwi::getSingletonPtr() {
		if( !__instance ) {
		    __instance = new Kiwi();
		}

		return __instance;
	}

	Kiwi& Kiwi::getSingleton() {
		return *getSingletonPtr();
	}

	void Kiwi::go(int argc, char** argv) {

    mApp = new QApplication(argc, argv);
    mApp->setOrganizationName("Kiwi");
    mApp->setApplicationName("Kiwi");

    this->setupWidgets();

    mApp->exec();
	}

	void Kiwi::setupWidgets() {

    mWindow = new QMainWindow();
    mUi.setupUi(mWindow);

    mDlgAbout = new QDialog(mWindow);
    mDlgAboutUi.setupUi(mDlgAbout);

    this->bindWidgets();

    mWindow->show();
	};

  void Kiwi::bindWidgets() {
    // Menu actions
    connect(mUi.actionAbout, SIGNAL(activated()), this, SLOT(evtShowAboutDialog()));

    // General
    connect(mUi.tabWidget, SIGNAL(currentChanged(int)), this, SLOT(evtTabChanged(int)));

    // General tab
    connect(mUi.btnChangeRoot, SIGNAL(released()), this, SLOT(evtClickChangeRoot()));
    connect(mUi.btnUpdateRoot, SIGNAL(released()), this, SLOT(evtClickUpdateRoot()));
    connect(mUi.radioFlat, SIGNAL(clicked(bool)), this, SLOT(evtChangeStructure(bool)));
    connect(mUi.radioMirror, SIGNAL(clicked(bool)), this, SLOT(evtChangeStructure(bool)));

    // Edit tab
    connect(mUi.btnCreate, SIGNAL(released()), this, SLOT(evtClickCreate()));
    connect(mUi.btnModify, SIGNAL(released()), this, SLOT(evtClickModify()));
    connect(mUi.btnRename, SIGNAL(released()), this, SLOT(evtClickRename()));
    connect(mUi.btnDelete, SIGNAL(released()), this, SLOT(evtClickDelete()));
    connect(mUi.btnRemoveC, SIGNAL(released()), this, SLOT(evtClickRemoveC()));
    connect(mUi.btnRemoveM, SIGNAL(released()), this, SLOT(evtClickRemoveM()));
    connect(mUi.btnRemoveR, SIGNAL(released()), this, SLOT(evtClickRemoveR()));
    connect(mUi.btnRemoveD, SIGNAL(released()), this, SLOT(evtClickRemoveD()));


    // Commit tab
    connect(mUi.btnGenerateScript, SIGNAL(released()), this, SLOT(evtClickGenerateScript()));
    connect(mUi.btnGenerateTarball, SIGNAL(released()), this, SLOT(evtClickGenerateTarball()));

    // Tools tab
    connect(mUi.btnFindDiffOriginal, SIGNAL(released()), this, SLOT(evtClickFindDiffOriginal()));
    connect(mUi.btnFindDiffModified, SIGNAL(released()), this, SLOT(evtClickFindDiffModified()));
    connect(mUi.btnFindDiffDest, SIGNAL(released()), this, SLOT(evtClickFindDiffDest()));
    connect(mUi.btnDiff, SIGNAL(released()), this, SLOT(evtClickDiff()));
    connect(mUi.btnFindMD5Source, SIGNAL(released()), this, SLOT(evtClickFindMD5Source()));
    connect(mUi.btnGenerateMD5, SIGNAL(released()), this, SLOT(evtClickGenerateMD5()));
  };

  void Kiwi::addTreeEntry(PatchEntry* inEntry) {
    QTreeWidgetItem* lItem = new QTreeWidgetItem();

    QString lLocal, lRemote, lAux;

    // strip the root
    lLocal = QString::fromStdString(inEntry->Local);
    if (inEntry->Remote != "")
      lRemote = QString::fromStdString((mRepo->isFlat()) ? inEntry->Flat : inEntry->Remote);

    switch (inEntry->Op) {
      case P_CREATE:
        lItem->setData(0, Qt::DisplayRole, lLocal);
        lItem->setData(1, Qt::DisplayRole, lRemote);
        lItem->setData(2, Qt::DisplayRole, QString(inEntry->Checksum.c_str()));
        mUi.treeCreations->addTopLevelItem(lItem);

        inEntry->Remote = lRemote.toStdString();
        break;
      case P_MODIFY:
        lItem->setData(0, Qt::DisplayRole, lLocal);
        lItem->setData(1, Qt::DisplayRole, lRemote);
        lItem->setData(2, Qt::DisplayRole, QString(inEntry->Checksum.c_str()));
        mUi.treeMods->addTopLevelItem(lItem);

        inEntry->Remote = lRemote.toStdString();
        break;
      case P_RENAME:
        lItem->setData(0, Qt::DisplayRole, lLocal);
        lItem->setData(1, Qt::DisplayRole, lRemote);
        lItem->setData(2, Qt::DisplayRole, QString(inEntry->Checksum.c_str()));
        mUi.treeRenames->addTopLevelItem(lItem);
        break;
      case P_DELETE:
        lItem->setData(0, Qt::DisplayRole, lLocal);
        mUi.treeDeletions->addTopLevelItem(lItem);
        break;
    }

    inEntry->Widget = lItem;
    lItem = 0;
  }

  void Kiwi::refreshTree() {
    mUi.treeCreations->setHeaderHidden(false);
    mUi.treeMods->setHeaderHidden(false);
    mUi.treeRenames->setHeaderHidden(false);
    mUi.treeDeletions->setHeaderHidden(false);
  }

  void Kiwi::evtTabChanged(int inIdx) {
    if (inIdx == 0 || mRepo->isRootSet())
      return;

    QMessageBox::warning(
      mWindow,
      tr("Locate application"),
      tr("You have to choose the directory that contains \
      \nthe application and set it as the repository's \
      \nroot before proceeding."));
  }

  void Kiwi::evtShowAboutDialog() {
    mDlgAbout->exec();
  }

  void Kiwi::setRoot(const QString& inRoot) {
    QFileInfo lRoot(inRoot);
    if (!lRoot.exists() || !lRoot.isDir()) {
      QMessageBox::critical(
        mWindow,
        tr("Invalid directory!"),
        tr("The directory you located does not seeem to exist!")
      );
      return;
    }

    mRepo->setRoot(lRoot.absoluteFilePath().toStdString());

    mUi.txtRoot->setText(inRoot);
    mUi.tabEdit->setEnabled(true);

    mUi.txtRoot->setEnabled(false);
    mUi.btnUpdateRoot->setEnabled(false);
    mUi.btnChangeRoot->setEnabled(false);
  };

  void Kiwi::evtClickUpdateRoot() {
    this->setRoot(mUi.txtRoot->text());
  }
  void Kiwi::evtClickChangeRoot() {

    QString dir =
      QFileDialog::getExistingDirectory(
        mUi.centralwidget,
        tr("Choose Application Root"),
        "",
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (dir != "")
      this->setRoot(dir);
  }

  bool Kiwi::validateEntry(const QString& inPath) {
    QFileInfo lRoot(QString::fromStdString(mRepo->getRoot()));
    QFileInfo lDest(inPath);

    std::cout << "Root: " << lRoot.absolutePath().toStdString() << "\n";
    std::cout << "Dest: " << lDest.absolutePath().toStdString() << "\n";

    if (!lDest.exists()) {
      QMessageBox::critical(
        mWindow,
        tr("Invalid entry!"),
        tr("The file you located does not seeem to exist!")
      );
      return false;
    }

    // if the path doesn't start with mRepo->Root then it's invalid
    bool isChild = lDest.absolutePath().contains(lRoot.absolutePath());
    if (!isChild) {
      QMessageBox::critical(
        mWindow,
        tr("Invalid entry!"),
        tr("The file you located does not reside within the application root. Please note that all repository files must reside within the application root.")
      );
      return false;
    } else {
      return true;
    }

  };

  void Kiwi::evtClickCreate() {

    QFileDialog dialog(mUi.centralwidget);
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setViewMode(QFileDialog::Detail);
    if (mRepo->isRootSet())
      dialog.setDirectory(QString::fromStdString(mRepo->getRoot()));

    QStringList fileNames;
    if (dialog.exec())
      fileNames = dialog.selectedFiles();
    else
      return;

    MD5 md5;
    QString lLocal, lRemote;
    QString lRoot = QString::fromStdString(mRepo->getRoot());
    PatchEntry* lEntry = 0;
    for (int i=0; i < fileNames.size(); ++i) {

      if (!this->validateEntry(fileNames.at(i)))
        break;

      lLocal = QString(fileNames.at(i)).remove(lRoot);
      lRemote = QString(fileNames.at(i)).remove(lRoot);
      std::string lChecksum = md5.digestFile((char*)(fileNames.at(i).toStdString()).c_str());

      // only add it if it hasn't been added yet
      lEntry = mRepo->registerEntry(P_CREATE, lLocal.toStdString(), lRemote.toStdString(), "", lChecksum);
      if (!lEntry)
        continue;

      lEntry->Flat = lRemote.replace("/", "_").replace(0,1,"/").toStdString();

      this->addTreeEntry(lEntry);

    }
    this->refreshTree();
    lEntry = 0;
  }

  void Kiwi::evtClickModify() {
    QString lSrc, lDiff, lRoot;
    lRoot = QString::fromStdString(mRepo->getRoot());

    QFileDialog dialog(mUi.centralwidget);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setViewMode(QFileDialog::Detail);
    if (mRepo->isRootSet())
      dialog.setDirectory(lRoot);

    if (dialog.exec())
      lSrc = QString(dialog.selectedFiles().at(0)).remove(lRoot);
    else
      return;

    if (!this->validateEntry(dialog.selectedFiles().at(0)))
      return;

    if (dialog.exec())
      lDiff = QString(dialog.selectedFiles().at(0)).remove(lRoot);
    else
      return;

    if (!this->validateEntry(dialog.selectedFiles().at(0)))
      return;

    MD5 md5;
    std::string lChecksum = md5.digestFile((char*)((dialog.selectedFiles().at(0).toStdString()).c_str()));

    PatchEntry *lEntry = mRepo->registerEntry(P_MODIFY, lSrc.toStdString(), lDiff.toStdString(), "", lChecksum);
    if (!lEntry)
      return;

    lEntry->Flat = lDiff.replace("/", "_").replace(0,1,"/").toStdString();

    this->addTreeEntry(lEntry);
    this->refreshTree();

    lEntry = 0;
  }

  void Kiwi::evtClickRename() {
    QString lSrc, lDest, lRoot;
    lRoot = QString::fromStdString(mRepo->getRoot());

    QFileDialog dialog(mUi.centralwidget);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setViewMode(QFileDialog::Detail);
    if (mRepo->isRootSet())
      dialog.setDirectory(lRoot);

    if (dialog.exec())
      lSrc = QString(dialog.selectedFiles().at(0)).remove(lRoot);
    else
      return;

    if (!this->validateEntry(dialog.selectedFiles().at(0)))
      return;

    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    if (dialog.exec())
      lDest = QString(dialog.selectedFiles().at(0)).remove(lRoot);
    else
      return;

    if (!this->validateEntry(dialog.selectedFiles().at(0)))
      return;

    PatchEntry *lEntry = mRepo->registerEntry(P_RENAME, lSrc.toStdString(), lDest.toStdString());
    if (!lEntry)
      return;

    this->addTreeEntry(lEntry);
    this->refreshTree();

    lEntry = 0;
  }

  void Kiwi::evtClickDelete() {
    QFileDialog dialog(mUi.centralwidget);
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setViewMode(QFileDialog::Detail);
    if (mRepo->isRootSet())
      dialog.setDirectory(QString::fromStdString(mRepo->getRoot()));

    QStringList fileNames;
    if (dialog.exec())
      fileNames = dialog.selectedFiles();
    else
      return;

    QString lRoot = QString::fromStdString(mRepo->getRoot());
    PatchEntry *lEntry = 0;
    for (int i=0; i < fileNames.size(); ++i) {

      if (!this->validateEntry(fileNames.at(i)))
        break;

      std::string lFilename = QString(fileNames.at(i)).remove(lRoot).toStdString();

      // only add it if it hasn't been added yet
      lEntry = mRepo->registerEntry(P_DELETE, lFilename);
      if (!lEntry)
        continue;

      this->addTreeEntry(lEntry);
    }
    this->refreshTree();
    lEntry = 0;
  }

  void Kiwi::evtClickFindDiffOriginal() {
    QString file =
      QFileDialog::getOpenFileName(
        mUi.centralwidget,
        tr("Choose the old file"),
        "",
        tr("All files (*)"));

    mUi.txtDiffOriginal->setText(file);
  }

  void Kiwi::evtClickFindDiffModified() {
    QString file =
      QFileDialog::getOpenFileName(
        mUi.centralwidget,
        tr("Choose the new file"),
        "",
        tr("All files (*)"));

    mUi.txtDiffModified->setText(file);
  }

  void Kiwi::evtClickFindDiffDest() {
    QString file =
      QFileDialog::getSaveFileName(
        mUi.centralwidget,
        tr("Choose a destination"),
        "",
        tr("All files (*)"));

    mUi.txtDiffDest->setText(file);
  }

  void Kiwi::evtClickDiff() {

    // sources must exist
    if (!QFile::exists(mUi.txtDiffOriginal->text()) ||
        !QFile::exists(mUi.txtDiffModified->text())) {
      QMessageBox::critical(
        mWindow,
        tr("Invalid sources"),
        tr("Please make sure that both files you've located exist and are readable.")
      );
      return;
    }

    // destination must not exist
    if (QFile::exists(mUi.txtDiffDest->text())) {
      QMessageBox::critical(
        mWindow,
        tr("Invalid destination"),
        tr("The destination file you've specified seems to already exist, will not overwrite.")
      );
      return;
    }

    // try opening the dest file
    FILE* fd = fopen(mUi.txtDiffDest->text().toStdString().c_str(), "w");
    if (!fd) {
      QMessageBox::critical(
        mWindow,
        tr("Invalid destination"),
        tr("Unable to open destination file for writing, the location might be invalid or there's not enough permission to write.")
      );
      return;
    }
    else {
      fclose(fd);
      remove(mUi.txtDiffDest->text().toStdString().c_str());
    }

    // sources must not be the same
    if (mUi.txtDiffOriginal->text() == mUi.txtDiffModified->text()) {
      QMessageBox::critical(
        mWindow,
        tr("Invalid sources"),
        tr("It appears that you've chosen the same file as both the old and new version, can not diff same source.")
      );
      return;
    }

    bsdiff(
      (mUi.txtDiffOriginal->text().toStdString()).c_str(),
      (mUi.txtDiffModified->text().toStdString()).c_str(),
      (mUi.txtDiffDest->text().toStdString()).c_str()
    );

    QMessageBox::information(
      mWindow,
      tr("Diff generated"),
      tr("Diff patch was successfully generated."));
  }

  void Kiwi::evtClickFindMD5Source() {

    QString file =
      QFileDialog::getOpenFileName(
        mUi.centralwidget,
        tr("Choose a file"),
        "",
        tr("All files (*)"));

    mUi.txtMD5Source->setText(file);
  }

  void Kiwi::evtClickGenerateMD5() {
    if (!QFile::exists(mUi.txtMD5Source->text())) {
      QMessageBox::critical(
        mWindow,
        tr("Invalid file"),
        tr("Please make sure that the file you've located exists and is readable.")
      );
      return;
    }

    MD5 md5;
    std::string lChecksum = md5.digestFile((char*)(mUi.txtMD5Source->text().toStdString()).c_str());
    mUi.txtMD5Result->setText(lChecksum.c_str());
  }

  void Kiwi::evtChangeStructure(bool fToggled) {
    if (mUi.radioFlat->isChecked()) {
      mRepo->setFlat(true);
    } else {
      mRepo->setFlat(false);
    }

    std::vector<PatchEntry*> lEntries = mRepo->getEntries();
    std::vector<PatchEntry*>::const_iterator entry;
    QString tmp;
    for (entry = lEntries.begin(); entry != lEntries.end(); ++entry) {
      if ((*entry)->Op != P_CREATE && (*entry)->Op != P_MODIFY)
        continue;

      if (mRepo->isFlat())
        tmp = QString::fromStdString((*entry)->Flat);
      else
        tmp = QString::fromStdString((*entry)->Remote);

      (*entry)->Widget->setData(1, Qt::DisplayRole, tmp);
    }

  }

  void Kiwi::evtClickGenerateScript() {
    if (mRepo->getEntries().empty()) {
      QMessageBox::information(
        mWindow,
        tr("Repository is empty!"),
        tr("You need to add entries to the repository before attempting to generate a patch script. Please go to the Edit tab."));
      return;
    }

    mRepo->setVersion(
      Version(
        mUi.spinVersionMajor->value(),
        mUi.spinVersionMinor->value(),
        mUi.spinVersionBuild->value()
      )
    );

    if (mRepo->getVersion().toNumber() == "0.0.0") {
      QMessageBox::information(
        mWindow,
        tr("Repository version is missing!"),
        tr("You need to specify the version of this repository in the Edit tab."));
      return;
    };

    std::ofstream of;
    std::string ofp = mRepo->getRoot() + "/patch.txt";
    of.open(ofp.c_str(), std::ios::trunc);

    if (!of.is_open() || !of.good()) {
      QMessageBox::critical(mWindow, tr("Couldn't open file"), tr("Unable to open file for writing."));
      return;
    }

    mUi.txtConsole->append(tr("Opened patch script for writing at ") + tr(ofp.c_str()));
    std::vector<PatchEntry*> lEntries = mRepo->getEntries();
    std::vector<PatchEntry*>::const_iterator entry;

    of << mRepo->getVersion().Value << "\n";
    of << "-\n";
    QString lMsg;
    for (entry = lEntries.begin(); entry != lEntries.end(); ++entry) {
      of << (*entry)->toString() << "\n";
      switch ((*entry)->Op) {
        case P_CREATE:
          lMsg = "* Registered patch entry of type CREATE";
          break;
        case P_MODIFY:
          lMsg = "* Registered patch entry of type MODIFY";
          break;
        case P_RENAME:
          lMsg = "* Registered patch entry of type RENAME";
          break;
        case P_DELETE:
          lMsg = "* Registered patch entry of type DELETE";
          break;
      }
      mUi.txtConsole->append(lMsg);
    }
    mUi.txtConsole->append(tr("* Number of patch entries: ") + QString::number(lEntries.size()));
    mUi.txtConsole->append(tr("Patch script generated successfully."));
    of.close();
  }

  void Kiwi::evtClickGenerateTarball() {
    // not available on windows yet
#if PIXY_PLATFORM == PIXY_PLATFORM_WIN32
    QMessageBox::information(mWindow, tr("Sorry!"), tr("This feature is not available yet for Windows."));
    return;
#endif
    if (mRepo->getEntries().empty() ) {
      QMessageBox::information(mWindow, tr("Repository is empty"), tr("There are no files to archive!"));
      return;
    } else if (mRepo->getEntries(P_CREATE).empty() && mRepo->getEntries(P_MODIFY).empty()) {
      QMessageBox::information(mWindow, tr("No new files added"), tr("The repository contains no newly created files or modified ones, there's nothing to archive!"));
      return;
    }

    std::string ofp = mRepo->getRoot() + "/patch_" + mRepo->getVersion().toNumber() + ".tar";
    mUi.txtConsole->append(tr("Preparing tar archive.") + tr(ofp.c_str()));
    std::fstream out(ofp.c_str(), std::ios::out);
    if(!out.is_open())
    {
      QMessageBox::critical(mWindow, tr("Could not open archive"), tr("Unable to open archive for writing."));
      return;
    }

    lindenb::io::Tar tarball(out);
    std::vector<PatchEntry*> lEntries = mRepo->getEntries(P_CREATE);
    std::vector<PatchEntry*>::const_iterator entry;

    std::string src, dest;
    std::string basepath = mRepo->getVersion().toNumber();
    for (entry = lEntries.begin(); entry != lEntries.end(); ++entry) {
      src = mRepo->getRoot() + (*entry)->Local;
      dest = basepath + ((mRepo->isFlat()) ? (*entry)->Flat : (*entry)->Remote);

      mUi.txtConsole->append(tr("* Adding file to archive: ") + src.c_str() + tr(" : ") + dest.c_str());
      tarball.putFile(src.c_str(), dest.c_str());
    }

    lEntries = mRepo->getEntries(P_MODIFY);
    for (entry = lEntries.begin(); entry != lEntries.end(); ++entry) {
      src = mRepo->getRoot() + (*entry)->Aux;
      dest = basepath + ((mRepo->isFlat()) ? (*entry)->Flat : (*entry)->Remote);

      mUi.txtConsole->append(tr("* Adding file to archive: ") + src.c_str() + tr(" : ") + dest.c_str());
      tarball.putFile(src.c_str(), dest.c_str());
    }

    tarball.finish();
    out.close();

    mUi.txtConsole->append(tr("Tar archive generated successfully."));

    mUi.txtConsole->append(tr("Compressing archive using BZip2..."));

#if PIXY_PLATFORM == PIXY_PLATFORM_WIN32
  #define open _open
  #define read _read
  #define close _close
#endif
    int tarFD = open(ofp.c_str(), O_RDONLY);

    std::string gofp = mRepo->getRoot() + std::string("/patch_") + mRepo->getVersion().toNumber() + std::string(".tar.bz2");
    FILE *tbz2File = fopen(gofp.c_str(), "wb");
    int bzError;
    const int BLOCK_MULTIPLIER = 7;
    BZFILE *pBz = BZ2_bzWriteOpen(&bzError, tbz2File, BLOCK_MULTIPLIER, 0, 0);

    const int BUF_SIZE = 10000;
    char* buf = new char[BUF_SIZE];
    ssize_t bytesRead;
    while((bytesRead = read(tarFD, buf, BUF_SIZE)) > 0)
    {
      BZ2_bzWrite(&bzError, pBz, buf, bytesRead);
    }
    BZ2_bzWriteClose(&bzError, pBz, 0, NULL, NULL);
    close(tarFD);
    remove(ofp.c_str());

    delete[] buf;

    fclose(tbz2File);

#if PIXY_PLATFORM == PIXY_PLATFORM_WIN32
  #undef open
  #undef read
  #undef close
#endif
    mUi.txtConsole->append(tr("Archive compressed successfully."));

  }

  void Kiwi::evtClickRemoveC() {
    mRepo->removeEntry(mUi.treeCreations->currentItem());
    mUi.treeCreations->takeTopLevelItem(
      mUi.treeCreations->indexOfTopLevelItem(
        mUi.treeCreations->currentItem()));
  };
  void Kiwi::evtClickRemoveM() {
    mRepo->removeEntry(mUi.treeMods->currentItem());
    mUi.treeMods->takeTopLevelItem(
      mUi.treeMods->indexOfTopLevelItem(
        mUi.treeMods->currentItem()));
  };
  void Kiwi::evtClickRemoveR() {
    mRepo->removeEntry(mUi.treeRenames->currentItem());
    mUi.treeRenames->takeTopLevelItem(
      mUi.treeRenames->indexOfTopLevelItem(
        mUi.treeRenames->currentItem()));
  };
  void Kiwi::evtClickRemoveD() {
    mRepo->removeEntry(mUi.treeDeletions->currentItem());
    mUi.treeDeletions->takeTopLevelItem(
      mUi.treeDeletions->indexOfTopLevelItem(
        mUi.treeDeletions->currentItem()));
  };

} // end of namespace Pixy
