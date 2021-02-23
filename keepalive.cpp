// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Service Manager
// -------------------------------------
// file       : keepalive.cpp
// author     : Ben Kietzman
// begin      : 2019-02-20
// copyright  : kietzman.org
// email      : ben@kietzman.org
///////////////////////////////////////////

/**************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
**************************************************************************/

/*! \file keepalive.cpp
* \brief Service Manager Keep-Alive
*
* Keeps the svcmgrd daemon alive.
*/
// {{{ includes
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
using namespace std;
// }}}
// {{{ defines
#ifdef VERSION
#undef VERSION
#endif
/*! \def VERSION
* \brief Contains the application version number.
*/
#define VERSION "1.0"
/*! \def mUSAGE(A)
* \brief Prints the usage statement.
*/
#define mUSAGE(A) cout << endl << "Usage:  "<< A << " [options]"  << endl << endl << "     --command=[COMMAND]" << endl << "     Sets the command." << endl << endl << "     --data=[PATH]" << endl << "     Sets the data directory." << endl << endl << " -h, --help" << endl << "     Displays this usage screen." << endl << endl << " -v, --version" << endl << "     Displays the current version of this software." << endl << endl
/*! \def mVER_USAGE(A,B)
* \brief Prints the version number.
*/
#define mVER_USAGE(A,B) cout << endl << A << " Version: " << B << endl << endl
/*! \def PID
* \brief Contains the PID path.
*/
#define PID "/.pid"
// }}}
// {{{ global variables
string gstrApplication = "Service Manager"; //!< Global application name.
string gstrCommand; //!< Global command.
string gstrData = "/data/svcmgr"; //!< Global data path.
// }}}
// {{{ main()
/*! \fn int main(int argc, char *argv[])
* \brief This is the main function.
* \return Exits with a return code for the operating system.
*/
int main(int argc, char *argv[])
{
  string strError, strPrefix = "main()";
  stringstream ssMessage;

  // {{{ command line arguments
  for (int i = 1; i < argc; i++)
  {
    size_t unPosition;
    string strArg = argv[i];
    if (strArg.size() > 10 && strArg.substr(0, 10) == "--command=")
    {
      gstrCommand = strArg.substr(10, strArg.size() - 10);
      while ((unPosition = gstrCommand.find("'")) != string::npos || (unPosition = gstrCommand.find("\"")) != string::npos)
      {
        gstrCommand.erase(unPosition, 1);
      }
    }
    else if (strArg.size() > 7 && strArg.substr(0, 7) == "--data=")
    {
      gstrData = strArg.substr(7, strArg.size() - 7);
      while ((unPosition = gstrData.find("'")) != string::npos || (unPosition = gstrData.find("\"")) != string::npos)
      {
        gstrData.erase(unPosition, 1);
      }
    }
    else if (strArg == "-h" || strArg == "--help")
    {
      mUSAGE(argv[0]);
      return 0;
    }
    else if (strArg == "-v" || strArg == "--version")
    {
      mVER_USAGE(argv[0], VERSION);
      return 0;
    }
  }
  // }}}
  // {{{ normal run
  if (!gstrCommand.empty())
  {
    ifstream inPid((gstrData + PID).c_str());
    if (inPid)
    {
      pid_t nPid;
      stringstream ssProc;
      struct stat tStat;
      inPid >> nPid;
      ssProc << "/proc/" << nPid;
      if (stat(ssProc.str().c_str(), &tStat) != 0 || !S_ISDIR(tStat.st_mode))
      {
        system(gstrCommand.c_str());
      }
    }
    else
    {
      system(gstrCommand.c_str());
    }
    inPid.close();
  }
  // }}}
  // {{{ usage statement
  else
  {
    mUSAGE(argv[0]);
  }
  // }}}

  return 0;
}
// }}}
