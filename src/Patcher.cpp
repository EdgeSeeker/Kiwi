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

#include "Patcher.h"
#include "Kiwi.h"

extern "C" int bspatch(const char* src, const char* dest, const char* diff);

namespace Pixy {
	Patcher* Patcher::__instance = NULL;

	Patcher::Patcher() {
	  mLog = new log4cpp::FixedContextCategory(PIXY_LOG_CATEGORY, "Patcher");
		mLog->infoStream() << "firing up";

		fValidated = false;
		fPatched = false;

		using boost::filesystem::exists;
		using std::fstream;

		// get our current version, and set a default one
		mCurrentVersion = Version(std::string(PIXY_APP_VERSION));
		fstream res;
		if (!exists(PIXY_RESOURCE_PATH)) {
      res.open(PIXY_RESOURCE_PATH, fstream::out);

      if (!res.is_open() || !res.good()) {

		  } else {
		    res << PIXY_APP_VERSION;
		    res.close();
		  }

		} else {
      res.open(PIXY_RESOURCE_PATH, fstream::in);

		  if (res.is_open() && res.good()) {
		    std::string v;
		    getline(res, v);
		    mCurrentVersion = Version(v);
		    res.close();
		  } else {
		    // TODO: inject renderer with error: missing data file
		  }
		}

		mLog->infoStream() << "Application version: " << mCurrentVersion.Value;
		mPatchScriptPath = std::string(PROJECT_TEMP_DIR) + "patch.txt";

		mProcessors.insert(std::make_pair<PATCHOP, t_proc>(CREATE, &Patcher::processCreate));
		mProcessors.insert(std::make_pair<PATCHOP, t_proc>(DELETE, &Patcher::processDelete));
		mProcessors.insert(std::make_pair<PATCHOP, t_proc>(MODIFY, &Patcher::processModify));
		mProcessors.insert(std::make_pair<PATCHOP, t_proc>(RENAME, &Patcher::processRename));

  }

	Patcher::~Patcher() {

		mLog->infoStream() << "shutting down";

		Repository* lRepo = 0;
		while (!mRepos.empty()) {
		  lRepo = mRepos.back();
		  mRepos.pop_back();
		  delete lRepo;
		}
		lRepo = 0;

    mProcessors.clear();

#ifndef PIXY_PERSISTENT
    // TODO: boost error checking
    boost::filesystem::remove_all(PROJECT_TEMP_DIR);
#endif

		if (mLog)
		  delete mLog;

	}

	Patcher* Patcher::getSingletonPtr() {
		if( !__instance ) {
		    __instance = new Patcher();
		}

		return __instance;
	}

	Patcher& Patcher::getSingleton() {
		return *getSingletonPtr();
	}

  void Patcher::update() {
  }

	void Patcher::validate() {

    fValidated = true;
    return;
	};

	void Patcher::buildRepositories() {
    using std::string;

    mLog->debugStream() << "building repositories";

    string line;
    std::ifstream mPatchScript;
    mPatchScript.open(mPatchScriptPath.c_str());

    if (!mPatchScript.is_open()) {
      mLog->errorStream() << "could not read patch list!";
      throw BadFileStream("unable to read patch list!");
    }

    /* we need to locate our current version in the patch list, and track all
     * the entries in between
     */
    bool located = false;
    while ( !located && mPatchScript.good() )
    {
      getline(mPatchScript,line);

      // determine what kind of script entry it is:
      if (line == "" || line == "-") { // 1) garbage
        continue;

      std::cout << "Line: " << line << "\n";
      } else if (line.find("VERSION") != string::npos) { // 2) a version signature
        if (Version(line) == mCurrentVersion) {
          // we're done parsing, this is our current version
          located = true;
          mLog->debugStream() << "found our version, no more files to parse";
          break;
        } else {
          // this is another version we need to patch up to
          mLog->debugStream() << "New version found: creating a repository... " << line;

          // create a new repository for this version
          Repository *lRepo = new Repository(Version(line));
          mRepos.push_front(lRepo);
          lRepo = 0;


          continue;
        }
      }

      Repository *lRepo = mRepos.front();

      // 3) an entry, let's parse it
      // __DEBUG__
      std::cout << "Line: " << line << "\n";
      fflush(stdout);

      std::vector<std::string> elements = Utility::split(line.c_str(), ' ');

      if (!validateLine(elements, line)) {
        continue;
      };

      PATCHOP op;
      try {
        op = opFromChar(elements[0].c_str());
      } catch (std::runtime_error& e) {
        mLog->errorStream() << e.what() << " from " << elements[0];
        continue;
      }

      // "local" entry paths need to be adjusted to reflect the project root
      // TODO: use tokens in patch script to manually specify project root
      // and special paths
      elements[1] = PROJECT_ROOT + elements[1];

      /* TODO: refactor
       * for C and M ops we need to stage the remote files in a temp directory
       * and so we resolve the path here according to the following scheme:
       * TMP_DIR/REPO_VERSION/REMOTE_FILE_NAME.EXT
       */
      using boost::filesystem::path;
      path lTempPath;
      if (op == CREATE || op == MODIFY) {
        lTempPath = path(
          std::string(PROJECT_TEMP_DIR)  +
          lRepo->getVersion().PathValue + "/" +
          path(elements[2]).filename().string()
        );

        if (!exists(lTempPath.parent_path()))
         create_directory(lTempPath.parent_path());
      }

      switch(op) {
        case CREATE:
        case MODIFY:
          lRepo->registerEntry(op, elements[1], elements[2], lTempPath.string(), elements[3]);
          break;
        case DELETE:
          lRepo->registerEntry(op, elements[1]);
          break;
        case RENAME:
          elements[2] = PROJECT_ROOT + elements[2]; // we assume the path is relative to the root
          lRepo->registerEntry(op, elements[1], elements[2]);
          break;
      }

      lRepo = 0;
    } // patchlist file parsing loop

    mPatchScript.close();

    // this really shouldn't happen
    if (!located) {
      mLog->warnStream() << "possible file or local version corruption: could not locate our version in patch list";
      throw BadVersion("could not locate our version in patch list " + mCurrentVersion.Value);
    }

    mLog->debugStream() << "repositories built";
	};

	bool Patcher::validateLine(std::vector<std::string>& elements, std::string line) {

    // minimum script entry (DELETE)
    if (elements.size() < 2) {
      mLog->errorStream() << "malformed line: '" << line << "', skipping";
      return false;
    }

    // parse the operation type and make sure the required fields exist
    if (elements[0] == "C") {
      if (elements.size() < 4) {
        // CREATE entries must have at least 4 fields
        mLog->errorStream() << "malformed CREATE line: '" << line << "', skipping";
        return false;
      }
    } else if (elements[0] == "M") {
      if (elements.size() < 4) {
        // MODIFY entries must have at least 4 fields
        mLog->errorStream() << "malformed MODIFY line: '" << line << "', skipping";
        return false;
      }
    } else if (elements[0] == "R") {
      if (elements.size() < 3) {
        // RENAME entries must have at least 3 fields
        mLog->errorStream() << "malformed RENAME line: '" << line << "', skipping";
        return false;
      }
    } else {
      mLog->errorStream()
        << "undefined operation symbol: "
        << elements[0] << " from line " << line;
      return false;
    }

	  return true;
	};

	bool Patcher::preprocess(Repository* inRepo) {
	  std::vector<PatchEntry*> entries = inRepo->getEntries();
    bool success = true;
    std::vector<PatchEntry*>::const_iterator entry;
    for (entry = entries.begin(); entry != entries.end() && success; ++entry)
      try {
        (this->*mProcessors[(*entry)->Op])((*entry), false);
      } catch (FileDoesNotExist& e) {
        success = false;
        // TODO: inject renderer with the error
      } catch (FileAlreadyCreated& e) {
        success = false;
      }

    return success;
	};

	void Patcher::operator()() {
    if (!fValidated)
      return (void)validate();

    patch();
	}


	void Patcher::patch() {
	  if (fPatched)
	    return;

	  if (!fValidated) // this really shouldn't happen, but just guard against it
	    validate();

	  try {
	    buildRepositories();
	  } catch (BadVersion& e) {
	  } catch (BadFileStream& e) {
	  }

	  mLog->infoStream() << "Total patches: " << mRepos.size();

	  bool success = true;
	  std::list<Repository*>::const_iterator repo;
	  for (repo = mRepos.begin(); repo != mRepos.end(); ++repo) {
	    mLog->infoStream() << "----";
	    mLog->infoStream() << "Patching to " << (*repo)->getVersion().Value;


	    // validate the repository
	    if (!preprocess(*repo)) {
	      // we cannot patch

	      success = false;
	      break;
	    }

	    // download the patch files
	    //Downloader::getSingleton()._fetchRepository(*repo);

	    // TODO: perform a check before committing any changes so we can rollback (DONE)
	    std::vector<PatchEntry*> entries = (*repo)->getEntries();
	    std::vector<PatchEntry*>::const_iterator entry;
	    for (entry = entries.begin(); entry != entries.end(); ++entry)
        (this->*mProcessors[(*entry)->Op])((*entry), true);

      mLog->infoStream()
        << "Application successfully patched to "
        << (*repo)->getVersion().Value;

      this->updateVersion((*repo)->getVersion());

	  }

	  if (success) {
	  }

	  fPatched = true;
	  return;
	}

	void Patcher::processCreate(PatchEntry* inEntry, bool fCommit) {
	  using boost::filesystem::exists;
	  using boost::filesystem::is_directory;
	  using boost::filesystem::create_directory;
	  using boost::filesystem::path;
	  using boost::filesystem::rename;
	  using boost::filesystem::copy_file;

	  // TODO: boost error checking

	  path local(inEntry->Local);
	  path temp(inEntry->Temp);

	  // make sure the file doesn't already exist, if it does.. abort
	  if (exists(local)) {
	    mLog->errorStream() << "file to be created already exists! " << local << " aborting...";
	    throw FileAlreadyCreated("Could not create file!", inEntry);
	  }

	  if (!fCommit)
	    return;

	  mLog->debugStream() << "creating a file " << local;

	  // create the parent directory if it doesn't exist
	  if (!is_directory(local.parent_path()))
	    create_directory(local.parent_path());

	  // now we move the file

	  // TODO: fix this, we shouldn't be copying, we should be moving
	  // bug #1: attempting to create 1+ files from the same remote source
	  // will result in undefined behaviour as the remote file is stored in the
	  // temp location and it's being removed on the first creation
	  //rename(temp, local);
	  copy_file(temp,local);
	};

	void Patcher::processDelete(PatchEntry* inEntry, bool fCommit) {
	  using boost::filesystem::exists;
	  using boost::filesystem::is_directory;
	  using boost::filesystem::create_directory;
	  using boost::filesystem::path;

	  path local(inEntry->Local);

	  // make sure the file exists!
	  if (!exists(local)) {
	    mLog->errorStream() << "no such file to delete! " << local;
	    throw FileDoesNotExist("Could not delete file!", inEntry);
	  }

    if (!fCommit)
	    return;

	  mLog->debugStream() << "deleting a file " << local;

	  // TODO: boost error checking
    if (is_directory(local))
      boost::filesystem::remove_all(local);
    else
      boost::filesystem::remove(local);
	};

	void Patcher::processModify(PatchEntry* inEntry, bool fCommit) {
	  using boost::filesystem::exists;
	  using boost::filesystem::path;
	  using boost::filesystem::rename;
	  using boost::filesystem::copy_file;

	  if (!exists(inEntry->Local)) {
	    mLog->errorStream() << "No such file to modify! " << inEntry->Local;
      throw FileDoesNotExist("No such file to modify!", inEntry);
	  }

	  if (!fCommit)
	    return;

	  mLog->debugStream() << "modifying a file " << inEntry->Local;

	  // TODO: boost error checking
	  // __DEBUG__
	  std::string tmp = inEntry->Temp;
	  tmp += ".foobar";


	  //patch(inEntry->Local.c_str(), inEntry->Local.c_str(), inEntry->Temp.c_str());
	  copy_file(inEntry->Local, tmp);
	  bspatch(tmp.c_str(), tmp.c_str(), inEntry->Temp.c_str());
	  //remove(path(inEntry->Temp));
	  remove(path(inEntry->Local));
	  rename(tmp, inEntry->Local);


	  // TODO: check if the file was already patched
	};

	void Patcher::processRename(PatchEntry* inEntry, bool fCommit) {
	  using boost::filesystem::exists;
	  using boost::filesystem::is_directory;
	  using boost::filesystem::create_directory;
	  using boost::filesystem::path;
	  using boost::filesystem::rename;
	  using boost::filesystem::copy_file;

	  // TODO: boost error checking

	  path dest(inEntry->Local);
	  path src(inEntry->Remote);

    // the source must exist and the destination must not
	  if (!exists(src)) {
	    mLog->errorStream() << "file to be moved doesn't exist! " << src << " aborting...";
	    throw FileDoesNotExist("Could not move file!", inEntry);
	  }

	  if (exists(dest)) {
	    mLog->errorStream() << "destination of rename already exists! " << dest << " aborting...";
	    throw FileAlreadyCreated("Could not move file!", inEntry);
	  }

	  if (!fCommit)
	    return;

	  // create the parent directory if it doesn't exist
	  if (!is_directory(dest.parent_path()))
	    create_directory(dest.parent_path());

	  mLog->debugStream() << "renaming a file " << src << " to " << dest;

	  rename(src, dest);
	};

	void Patcher::updateVersion(const Version& inVersion) {
	  mCurrentVersion = Version(inVersion);

	  using std::fstream;
	  fstream res;
	  res.open(PIXY_RESOURCE_PATH, fstream::out);
	  if (!res.is_open() || !res.good()) {
	    mLog->errorStream() << "couldn't open version resource file " << PIXY_RESOURCE_PATH << ", aborting version update";
	    return;
	  }
	  res.write(mCurrentVersion.Value.c_str(), mCurrentVersion.Value.size());
	  res.close();

	};

	PATCHOP Patcher::opFromChar(const char* inC) {

	  if (*inC == 'M')
	    return MODIFY;
	  else if (*inC == 'C')
	    return CREATE;
	  else if (*inC == 'D')
	    return DELETE;
	  else if (*inC == 'R')
	    return RENAME;
	  else {
	    throw std::runtime_error("Invalid OP character!");
	  }
	}
};
