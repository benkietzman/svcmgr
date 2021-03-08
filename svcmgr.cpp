// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Service Manager
// -------------------------------------
// file       : svcmgr.cpp
// author     : Ben Kietzman
// begin      : 2019-02-18
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

/*! \file svcmgr.cpp
* \brief Service Manager Client
*
* Manages non-root services.
*/
// {{{ includes
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
using namespace std;
#include <Json>
using namespace common;
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
#define mUSAGE(A) cout << endl << "Usage:  "<< A << " [function: disable, enable, list, reload, restart, start, stop] [service]" << endl << endl
/*! \def mVER_USAGE(A,B)
* \brief Prints the version number.
*/
#define mVER_USAGE(A,B) cout << endl << A << " Version: " << B << endl << endl
/*! \def UNIX_SOCKET
* \brief Contains the unix socket path.
*/
#ifndef UNIX_SOCKET
#define UNIX_SOCKET "/tmp/svcmgr"
#endif
// }}}
// {{{ main()
/*! \fn int main(int argc, char *argv[])
* \brief This is the main function.
* \return Exits with a return code for the operating system.
*/
int main(int argc, char *argv[])
{
  // {{{ normal run
  if (argc >= 2)
  {
    string strFunction = argv[1], strService = ((argc == 3)?argv[2]:"");
    struct stat tStat;
    if (stat(UNIX_SOCKET, &tStat) == 0)
    {
      int fdUnix;
      if ((fdUnix = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0)
      {
        sockaddr_un addr;
        memset(&addr, 0, sizeof(sockaddr_un));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, UNIX_SOCKET, (sizeof(addr.sun_path) - 1));
        if (connect(fdUnix, (sockaddr *)&addr, sizeof(sockaddr_un)) == 0)
        {
          bool bExit = false;
          char szBuffer[4096];
          int nReturn;
          size_t unPosition;
          string strBuffer[2];
          Json *ptJson = new Json;
          ptJson->insert("Function", strFunction);
          ptJson->insert("Service", strService);
          ptJson->json(strBuffer[1]);
          delete ptJson;
          strBuffer[1] += "\n";
          while (!bExit)
          {
            pollfd fds[1];
            fds[0].fd = fdUnix;
            fds[0].events = POLLIN;
            if (!strBuffer[1].empty())
            {
              fds[0].events |= POLLOUT;
            }
            if ((nReturn = poll(fds, 1, 250)) > 0)
            {
              if (fds[0].revents & POLLIN)
              {
                if ((nReturn = read(fds[0].fd, szBuffer, 4096)) > 0)
                {
                  strBuffer[0].append(szBuffer, nReturn);
                  if ((unPosition = strBuffer[0].find("\n")) != string::npos)
                  {
                    bExit = true;
                    ptJson = new Json(strBuffer[0].substr(0, unPosition));
                    strBuffer[0].erase(0, (unPosition + 1));
                    if (ptJson->m.find("Status") != ptJson->m.end() && ptJson->m["Status"]->v == "okay")
                    {
                      if (ptJson->m.find("Response") != ptJson->m.end())
                      {
                        if (strFunction == "list")
                        {
                          size_t unMax[2] = {0, 0};
                          for (map<string, Json *>::iterator i = ptJson->m["Response"]->m.begin(); i != ptJson->m["Response"]->m.end(); i++)
                          {
                            if (i->first.size() > unMax[0])
                            {
                              unMax[0] = i->first.size();
                            }
                            if (i->second->v.size() > unMax[1])
                            {
                              unMax[1] = i->second->v.size();
                            }
                          }
                          for (map<string, Json *>::iterator i = ptJson->m["Response"]->m.begin(); i != ptJson->m["Response"]->m.end(); i++)
                          {
                            cout << setw(unMax[0]) << setfill(' ') << i->first << ":  " << setw(unMax[1]) << setfill(' ') << i->second->v << endl;
                          }
                        }
                        else
                        {
                          cout <<  endl << ptJson->m["Response"] << endl;
                        }
                      }
                    }
                    else if (ptJson->m.find("Error") != ptJson->m.end() && !ptJson->m["Error"]->v.empty())
                    {
                      cerr << ptJson->m["Error"]->v << endl;
                    }
                    else
                    {
                      cerr << "Encountered an unknown error." << endl;
                    }
                    delete ptJson;
                  }
                }
                else
                {
                  bExit = true;
                  if (nReturn < 0)
                  {
                    cerr << "read(" << errno << ") error:  " << strerror(errno) << endl;
                  }
                }
              }
              if (fds[0].revents & POLLOUT)
              {
                if ((nReturn = write(fds[0].fd, strBuffer[1].c_str(), strBuffer[1].size())) > 0)
                {
                  strBuffer[1].erase(0, nReturn);
                }
                else
                {
                  bExit = true;
                  if (nReturn < 0)
                  {
                    cerr << "write(" << errno << ") error:  " << strerror(errno) << endl;
                  }
                }
              }
            }
            else if (nReturn < 0)
            {
              bExit = true;
              cerr << "poll(" << errno << ") error:  " << strerror(errno) << endl;
            }
          }
        }
        else
        {
          cerr << "connect(" << errno << ") error:  " << strerror(errno) << endl;
        }
        close(fdUnix);
      }
      else
      {
        cerr << "socket(" << errno << ") error:  " << strerror(errno) << endl;
      }
    }
    else
    {
      cerr << "stat(" << errno << ") error:  " << strerror(errno) << endl;
    }
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
