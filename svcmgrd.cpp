// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Service Manager
// -------------------------------------
// file       : svcmgrd.cpp
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

/*! \file svcmgrd.cpp
* \brief Service Manager Daemon
*
* Manages non-root services.
*/
// {{{ includes
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
using namespace std;
#include <Central>
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
#define mUSAGE(A) cout << endl << "Usage:  "<< A << " [options]"  << endl << endl << " -c, --conf=[CONF]" << endl << "     Provides the configuration path." << endl << endl << " -d, --daemon" << endl << "     Turns the process into a daemon." << endl << endl << "     --data=[PATH]" << endl << "     Sets the data directory." << endl << endl << " -e EMAIL, --email=EMAIL" << endl << "     Provides the email address for default notifications." << endl << endl << " -h, --help" << endl << "     Displays this usage screen." << endl << endl << " -v, --version" << endl << "     Displays the current version of this software." << endl << endl
/*! \def mVER_USAGE(A,B)
* \brief Prints the version number.
*/
#define mVER_USAGE(A,B) cout << endl << A << " Version: " << B << endl << endl
/*! \def PID
* \brief Contains the PID path.
*/
#define PID "/.pid"
/*! \def START
* \brief Contains the start path.
*/
#define START "/.start"
/*! \def UNIX_SOCKET
* \brief Contains the unix socket path.
*/
#ifndef UNIX_SOCKET
#define UNIX_SOCKET "/tmp/svcmgr"
#endif
// }}}
// {{{ structs
struct service
{
  bool bDetached;
  bool bStopped;
  pid_t nPid;
  list<string> environment;
  size_t unCrashes;
  string strDescription;
  string strExecStart;
  string strExecStartPost;
  string strExecStartPre;
  string strExecStopPost;
  string strLimitCore;
  string strLimitNoFile;
  string strPidFile;
  string strRestart;
  time_t CStart;
};
// }}}
// {{{ global variables
char **environ;
bool gbDaemon = false; //!< Global daemon variable.
bool gbShutdown = false; //!< Global shutdown variable.
map<string, service *> gServices; //!< Global services.
rlim_t gResourceLimitCoreSoft; //!< Global core soft limit.
rlim_t gResourceLimitCoreHard; //!< Global core hard limit.
rlim_t gResourceLimitNoFileSoft; //!< Global file descriptor soft limit.
rlim_t gResourceLimitNoFileHard; //!< Global file descriptor hard limit.
string gstrApplication = "Service Manager"; //!< Global application name.
string gstrData = "/data/svcmgr"; //!< Global data path.
string gstrEmail; //!< Global notification email address.
Central *gpCentral = NULL; //!< Contains the Central class.
// }}}
// {{{ prototypes
/*! \fn bool serviceActive(const string strService, string &strError)
* \brief Active service.
* \param strService Contains the service.
* \param strError Contains the error.
* \return Returns a boolean true/false value.
*/
bool serviceActive(const string strService, string &strError);
/*! \fn bool serviceAdd(const string strService, string &strError)
* \brief Add service.
* \param strService Contains the service.
* \param strError Contains the error.
* \return Returns a boolean true/false value.
*/
bool serviceAdd(const string strService, string &strError);
/*! \fn bool serviceDisable(const string strService, string &strError)
* \brief Disable service.
* \param strService Contains the service.
* \param strError Contains the error.
* \return Returns a boolean true/false value.
*/
bool serviceDisable(const string strService, string &strError);
/*! \fn bool serviceEnable(const string strService, string &strError)
* \brief Enable service.
* \param strService Contains the service.
* \param strError Contains the error.
* \return Returns a boolean true/false value.
*/
bool serviceEnable(const string strService, string &strError);
/*! \fn bool serviceExist(const string strService, string &strError)
* \brief Exist service.
* \param strService Contains the service.
* \param strError Contains the error.
* \return Returns a boolean true/false value.
*/
bool serviceExist(const string strService, string &strError);
/*! \fn bool serviceLink(const string strService, string &strError)
* \brief Link service.
* \param strService Contains the service.
* \param strError Contains the error.
* \return Returns a boolean true/false value.
*/
bool serviceLink(const string strService, string &strError);
/*! \fn bool serviceReload(const string strService, string &strError)
* \brief Reload service.
* \param strService Contains the service.
* \param strError Contains the error.
* \return Returns a boolean true/false value.
*/
bool serviceReload(const string strService, string &strError);
/*! \fn bool serviceRemove(const string strService, string &strError)
* \brief Remove service.
* \param strService Contains the service.
* \param strError Contains the error.
* \return Returns a boolean true/false value.
*/
bool serviceRemove(const string strService, string &strError);
/*! \fn bool serviceRestart(const string strService, string &strError)
* \brief Restart service.
* \param strService Contains the service.
* \param strError Contains the error.
* \return Returns a boolean true/false value.
*/
bool serviceRestart(const string strService, string &strError);
/*! \fn bool serviceStart(const string strService, string &strError)
* \brief Start service.
* \param strService Contains the service.
* \param strError Contains the error.
* \return Returns a boolean true/false value.
*/
bool serviceStart(const string strService, string &strError);
/*! \fn bool serviceStop(const string strService, string &strError)
* \brief Stop service.
* \param strService Contains the service.
* \param strError Contains the error.
* \return Returns a boolean true/false value.
*/
bool serviceStop(const string strService, string &strError);
/*! \fn bool serviceUnlink(const string strService, string &strError)
* \brief Unlink service.
* \param strService Contains the service.
* \param strError Contains the error.
* \return Returns a boolean true/false value.
*/
bool serviceUnlink(const string strService, string &strError);
/*! \fn bool serviceValid(const string strService, string &strError)
* \brief Valid service.
* \param strService Contains the service.
* \param strError Contains the error.
* \return Returns a boolean true/false value.
*/
bool serviceValid(const string strService, string &strError);
/*! \fn void sighandle(const int nSignal, siginfo_t *ptInfo, void *vptContext)
* \brief Establishes signal forwarding for the application.
* \param nSignal Contains the caught signal.
* \param ptInfo Contains the source information.
* \param ptContext Contains the context.
*/
void sighandle(const int nSignal, siginfo_t *ptInfo, void *vptContext);
// }}}
// {{{ main()
/*! \fn int main(int argc, char *argv[])
* \brief This is the main function.
* \return Exits with a return code for the operating system.
*/
int main(int argc, char *argv[])
{
  struct sigaction act;
  string strError, strPrefix = "main()";
  stringstream ssMessage;

  // {{{ set signal handling
  memset(&act, 0, sizeof(struct sigaction));
  sigemptyset(&act.sa_mask);
  act.sa_sigaction = sighandle;
  act.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGTERM, &act, NULL);
  // }}}
  gpCentral = new Central(strError);
  // {{{ command line arguments
  for (int i = 1; i < argc; i++)
  {
    string strArg = argv[i];
    if (strArg == "-c" || (strArg.size() > 7 && strArg.substr(0, 7) == "--conf="))
    {
      string strConf;
      if (strArg == "-c" && i + 1 < argc && argv[i+1][0] != '-')
      {
        strConf = argv[++i];
      }
      else
      {
        strConf = strArg.substr(7, strArg.size() - 7);
      }
      strError.clear();
      gpCentral->manip()->purgeChar(strConf, strConf, "'");
      gpCentral->manip()->purgeChar(strConf, strConf, "\"");
      gpCentral->utility()->setConfPath(strConf, strError);
    }
    else if (strArg == "-d" || strArg == "--daemon")
    {
      gbDaemon = true;
    }
    else if (strArg.size() > 7 && strArg.substr(0, 7) == "--data=")
    {
      gstrData = strArg.substr(7, strArg.size() - 7);
      gpCentral->manip()->purgeChar(gstrData, gstrData, "'");
      gpCentral->manip()->purgeChar(gstrData, gstrData, "\"");
    }
    else if (strArg == "-e" || (strArg.size() > 8 && strArg.substr(0, 8) == "--email="))
    {
      if (strArg == "-e" && i + 1 < argc && argv[i+1][0] != '-')
      {
        gstrEmail = argv[++i];
      }
      else
      {
        gstrEmail = strArg.substr(8, strArg.size() - 8);
      }
      gpCentral->manip()->purgeChar(gstrEmail, gstrEmail, "'");
      gpCentral->manip()->purgeChar(gstrEmail, gstrEmail, "\"");
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
  if (strError.empty())
  {
    gpCentral->setApplication(gstrApplication);
    gpCentral->setEmail(gstrEmail);
    gpCentral->setLog(gstrData, "svcmgrd_", "monthly", true, true);
    // {{{ normal run
    if (!gstrEmail.empty())
    {
      bool bExit = false;
      char szBuffer[4096];
      int fdUnix = -1, nReturn;
      list<int> removals;
      list<string> files;
      map<int, vector<string> > sockets;
      pollfd *fds;
      rlimit tResourceLimit;
      size_t unIndex, unPosition;
      string strJson;
      struct stat tStat;
      time_t CUnixSocketTime[2] = {0, 0};
      // {{{ prep
      if (gbDaemon)
      {
        gpCentral->utility()->daemonize();
      }
      setlocale(LC_ALL, "");
      SSL_library_init();
      OpenSSL_add_all_algorithms();
      SSL_load_error_strings();
      tzset();
      ofstream outPid((gstrData + PID).c_str());
      if (outPid.good())
      {
        outPid << getpid() << endl;
      }
      outPid.close();
      ofstream outStart((gstrData + START).c_str());
      outStart.close();
      if ((nReturn = chdir((gstrData + (string)"/cores").c_str())) == 0)
      {
        ssMessage.str("");
        ssMessage << strPrefix << "->chdir() [" << gstrData << "/cores]:  Changed the working directory.";
        gpCentral->log(ssMessage.str());
      }
      else
      {
        ssMessage.str("");
        ssMessage << strPrefix << "->chdir(" << nReturn << ") [" << gstrData << "/cores]:  " << strerror(errno);
        gpCentral->notify(ssMessage.str());
      }
      // {{{ core limit
      if (getrlimit(RLIMIT_CORE, &tResourceLimit) == 0)
      {
        ssMessage.str("");
        ssMessage << strPrefix << "->getrlimit() [RLIMIT_CORE]:  Retrieved the core limit soft value of ";
        gResourceLimitCoreSoft = tResourceLimit.rlim_cur;
        if (gResourceLimitCoreSoft == RLIM_INFINITY)
        {
          ssMessage << "infinity";
        }
        else
        {
          ssMessage << gResourceLimitCoreSoft;
        }
        ssMessage << " and hard value of ";
        gResourceLimitCoreHard = tResourceLimit.rlim_max;
        if (gResourceLimitCoreHard == RLIM_INFINITY)
        {
          ssMessage << "infinity";
        }
        else
        {
          ssMessage << gResourceLimitCoreHard;
        }
        ssMessage << ".";
        gpCentral->log(ssMessage.str());
        if (gResourceLimitCoreSoft != RLIM_INFINITY && (gResourceLimitCoreHard == RLIM_INFINITY || gResourceLimitCoreSoft < gResourceLimitCoreHard))
        {
          if (gResourceLimitCoreHard == RLIM_INFINITY)
          {
            tResourceLimit.rlim_cur = RLIM_INFINITY;
          }
          else
          {
            tResourceLimit.rlim_cur = gResourceLimitCoreHard;
          }
          if (setrlimit(RLIMIT_CORE, &tResourceLimit) == 0)
          {
            gResourceLimitCoreSoft = tResourceLimit.rlim_cur;
            ssMessage.str("");
            ssMessage << strPrefix << "->setrlimit() [RLIMIT_CORE]:  Set the core limit to a soft value of ";
            if (tResourceLimit.rlim_cur == RLIM_INFINITY)
            {
              ssMessage << "infinity";
            }
            else
            {
              ssMessage << tResourceLimit.rlim_cur;
            }
            ssMessage << " and a hard value of ";
            if (tResourceLimit.rlim_max == RLIM_INFINITY)
            {
              ssMessage << "infinity";
            }
            else
            {
              ssMessage << tResourceLimit.rlim_max;
            }
            ssMessage << ".";
            gpCentral->log(ssMessage.str());
          }
          else
          {
            ssMessage.str("");
            ssMessage << strPrefix << "->setrlimit(" << errno << ") error [RLIMIT_CORE]:  " << strerror(errno);
            gpCentral->notify(ssMessage.str());
          }
        }
      }
      else
      {
        ssMessage.str("");
        ssMessage << strPrefix << "->getrlimit(" << errno << ") error [RLIMIT_CORE]:  " << strerror(errno);
        gpCentral->notify(ssMessage.str());
      }
      // }}}
      // {{{ file descriptor limit
      if (getrlimit(RLIMIT_NOFILE, &tResourceLimit) == 0)
      {
        ssMessage.str("");
        ssMessage << strPrefix << "->getrlimit() [RLIMIT_NOFILE]:  Retrieved the file descriptor limit soft value of ";
        gResourceLimitNoFileSoft = tResourceLimit.rlim_cur;
        if (gResourceLimitNoFileSoft == RLIM_INFINITY)
        {
          ssMessage << "infinity";
        }
        else
        {
          ssMessage << gResourceLimitNoFileSoft;
        }
        ssMessage << " and hard value of ";
        gResourceLimitNoFileHard = tResourceLimit.rlim_max;
        if (gResourceLimitNoFileHard == RLIM_INFINITY)
        {
          ssMessage << "infinity";
        }
        else
        {
          ssMessage << gResourceLimitNoFileHard;
        }
        ssMessage << ".";
        gpCentral->log(ssMessage.str());
        if (gResourceLimitNoFileSoft != RLIM_INFINITY && (gResourceLimitNoFileHard == RLIM_INFINITY || gResourceLimitNoFileSoft < gResourceLimitNoFileHard))
        {
          if (gResourceLimitNoFileHard == RLIM_INFINITY)
          {
            tResourceLimit.rlim_cur = RLIM_INFINITY;
          }
          else
          {
            tResourceLimit.rlim_cur = gResourceLimitNoFileHard;
          }
          if (setrlimit(RLIMIT_NOFILE, &tResourceLimit) == 0)
          {
            gResourceLimitNoFileSoft = tResourceLimit.rlim_cur;
            ssMessage.str("");
            ssMessage << strPrefix << "->setrlimit() [RLIMIT_FILE]:  Set the file descriptor limit to a soft value of ";
            if (tResourceLimit.rlim_cur == RLIM_INFINITY)
            {
              ssMessage << "infinity";
            }
            else
            {
              ssMessage << tResourceLimit.rlim_cur;
            }
            ssMessage << " and a hard value of ";
            if (tResourceLimit.rlim_max == RLIM_INFINITY)
            {
              ssMessage << "infinity";
            }
            else
            {
              ssMessage << tResourceLimit.rlim_max;
            }
            ssMessage << ".";
            gpCentral->log(ssMessage.str());
          }
          else
          {
            ssMessage.str("");
            ssMessage << strPrefix << "->setrlimit(" << errno << ") error [RLIMIT_FILE]:  " << strerror(errno);
            gpCentral->notify(ssMessage.str());
          }
        }
      }
      else
      {
        ssMessage.str("");
        ssMessage << strPrefix << "->getrlimit(" << errno << ") error [RLIMIT_NOFILE]:  " << strerror(errno);
        gpCentral->notify(ssMessage.str());
      }
      // }}}
      gpCentral->file()->directoryList(gstrData + "/enabled", files);
      for (list<string>::iterator i = files.begin(); i != files.end(); i++)
      {
        if ((*i) != "." && (*i) != ".." && i->size() > 8 && i->substr((i->size() - 8), 8) == ".service")
        {
          struct stat tStat;
          if (stat((gstrData + (string)"/enabled/" + (*i)).c_str(), &tStat) == 0)
          {
            if (serviceEnable(i->substr(0, (i->size() - 8)), strError))
            {
              if (!serviceStart(i->substr(0, (i->size() - 8)), strError))
              {
                ssMessage.str("");
                ssMessage << strPrefix << "->serviceStart() error [" << i->substr(0, (i->size() - 8)) << "]:  " << strError;
                gpCentral->notify(ssMessage.str());
              }
            }
            else
            {
              ssMessage.str("");
              ssMessage << strPrefix << "->serviceEnable() error [" << i->substr(0, (i->size() - 8)) << "]:  " << strError;
              gpCentral->notify(ssMessage.str());
            }
          }
          else
          {
            ssMessage.str("");
            ssMessage << strPrefix << "->stat(" << errno << ") error [" << gstrData << "/enabled/" << (*i) << "]:  " << strerror(errno);
            gpCentral->notify(ssMessage.str());
          }
        }
      }
      files.clear();
      umask(strtol("0007", 0, 8));
      ssMessage.str("");
      ssMessage << strPrefix << "->umask() [0007]:  Set the umask.";
      gpCentral->log(ssMessage.str());
      // }}}
      while (!gbShutdown && !bExit)
      {
        // {{{ prep
        fds = new pollfd[sockets.size()+1];
        unIndex = 0;
        time(&(CUnixSocketTime[1]));
        if ((CUnixSocketTime[1] - CUnixSocketTime[0]) >= 30)
        {
          CUnixSocketTime[0] = CUnixSocketTime[1];
          if (fdUnix == -1 || (stat(UNIX_SOCKET, &tStat) != 0 || !S_ISSOCK(tStat.st_mode)))
          {
            if (stat(UNIX_SOCKET, &tStat) == 0 && remove(UNIX_SOCKET) != 0)
            {
              ssMessage.str("");
              ssMessage << strPrefix << "->remove(" << errno << ") error [" << UNIX_SOCKET << "]:  " << strerror(errno);
              gpCentral->notify(ssMessage.str());
            }
            if (fdUnix != -1)
            {
              close(fdUnix);
              ssMessage.str("");
              ssMessage << strPrefix << "->close() [" << UNIX_SOCKET << "]:  Closed socket.";
              gpCentral->log(ssMessage.str());
            }
            if ((fdUnix = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0)
            {
              sockaddr_un addr;
              ssMessage.str("");
              ssMessage << strPrefix << "->socket() [" << UNIX_SOCKET << "," << fdUnix << "]:  Created socket.";
              gpCentral->log(ssMessage.str());
              memset(&addr, 0, sizeof(sockaddr_un));
              addr.sun_family = AF_UNIX;
              strncpy(addr.sun_path, UNIX_SOCKET, sizeof(addr.sun_path) - 1);
              if (bind(fdUnix, (sockaddr *)&addr, sizeof(sockaddr_un)) == 0)
              {
                ssMessage.str("");
                ssMessage << strPrefix << "->bind() [" << UNIX_SOCKET << "," << fdUnix << "]:  Bound socket.";
                gpCentral->log(ssMessage.str());
                if (listen(fdUnix, 5) == 0)
                {
                  ssMessage.str("");
                  ssMessage << strPrefix << "->listen() [" << UNIX_SOCKET << "," << fdUnix << "]:  Listening to socket.";
                  gpCentral->log(ssMessage.str());
                }
                else
                {
                  close(fdUnix);
                  ssMessage.str("");
                  ssMessage << strPrefix << "->listen(" << errno << ") error [" << UNIX_SOCKET << "," << fdUnix << "]:  " << strerror(errno);
                  gpCentral->notify(ssMessage.str());
                }
              }
              else
              {
                close(fdUnix);
                ssMessage.str("");
                ssMessage << strPrefix << "->bind(" << errno << ") error [" << UNIX_SOCKET << "," << fdUnix << "]:  " << strerror(errno);
                gpCentral->notify(ssMessage.str());
              }
            }
            else
            {
              ssMessage.str("");
              ssMessage << strPrefix << "->socket(" << errno << ") error [" << UNIX_SOCKET << "]:  " << strerror(errno);
              gpCentral->notify(ssMessage.str());
            }
          }
        }
        fds[unIndex].fd = fdUnix;
        fds[unIndex].events = POLLIN;
        unIndex++;
        for (map<int, vector<string> >::iterator i = sockets.begin(); i != sockets.end(); i++)
        {
          fds[unIndex].fd = i->first;
          fds[unIndex].events = POLLIN;
          if (!i->second[1].empty())
          {
            fds[unIndex].events |= POLLOUT;
          }
          unIndex++;
        }
        // }}}
        if ((nReturn = poll(fds, unIndex, 250)) > 0)
        {
          // {{{ accept
          if (fds[0].revents & POLLIN)
          {
            int fdClient;
            sockaddr_un cli_addr;
            socklen_t clilen = sizeof(sockaddr_un);
            if ((fdClient = accept(fdUnix, (sockaddr *)&cli_addr, &clilen)) >= 0)
            {
              vector<string> buffers;
              buffers.push_back("");
              buffers.push_back("");
              sockets[fdClient] = buffers;
              buffers.clear();
            }
            else
            {
              ssMessage.str("");
              ssMessage << strPrefix << "->accept(" << errno << ") error [" << fdUnix << "]:  " << strerror(errno);
              gpCentral->notify(ssMessage.str());
              close(fdUnix);
              ssMessage.str("");
              ssMessage << strPrefix << "->close() [" << UNIX_SOCKET << "," << fdUnix << "]:  Closed socket.";
              gpCentral->log(ssMessage.str());
              fdUnix = -1;
            }
          }
          // }}}
          // {{{ clients
          for (size_t i = 1; i < unIndex; i++)
          {
            if (sockets.find(fds[i].fd) != sockets.end())
            {
              // {{{ read
              if (fds[i].revents & POLLIN)
              {
                if ((nReturn = read(fds[i].fd, szBuffer, 4096)) > 0)
                {
                  sockets[fds[i].fd][0].append(szBuffer, nReturn);
                  while ((unPosition = sockets[fds[i].fd][0].find("\n")) != string::npos)
                  {
                    bool bProcessed = false;
                    Json *ptJson = new Json(sockets[fds[i].fd][0].substr(0, unPosition));
                    sockets[fds[i].fd][0].erase(0, (unPosition + 1));
                    strError.clear();
                    if (ptJson->m.find("Function") != ptJson->m.end() && !ptJson->m["Function"]->v.empty())
                    {
                      string strService;
                      if (ptJson->m.find("Service") != ptJson->m.end() && !ptJson->m["Service"]->v.empty())
                      {
                        strService = ptJson->m["Service"]->v;
                      }
                      // {{{ disable
                      if (ptJson->m["Function"]->v == "disable")
                      {
                        bProcessed = serviceDisable(strService, strError);
                      }
                      // }}}
                      // {{{ enable
                      else if (ptJson->m["Function"]->v == "enable")
                      {
                        bProcessed = serviceEnable(strService, strError);
                      }
                      // }}}
                      // {{{ list
                      else if (ptJson->m["Function"]->v == "list")
                      {
                        if (!strService.empty())
                        {
                          if (gServices.find(strService) != gServices.end())
                          {
                            bProcessed = true;
                            ptJson->m["Response"] = new Json;
                            ptJson->m["Response"]->v = ((gServices[strService]->nPid != -1)?"active":"enabled");
                          }
                          else if (gpCentral->file()->fileExist(gstrData + (string)"/enabled/" + strService + (string)".service"))
                          {
                            bProcessed = true;
                            ptJson->m["Response"] = new Json;
                            ptJson->m["Response"]->v = "enabled";
                          }
                          else if (gpCentral->file()->fileExist(gstrData + (string)"/services/" + strService + (string)".service"))
                          {
                            bProcessed = true;
                            ptJson->m["Response"] = new Json;
                            ptJson->m["Response"]->v = "disabled";
                          }
                          else
                          {
                            strError = "Failed to find service.";
                          }
                        }
                        else
                        {
                          map<string, string> services;
                          bProcessed = true;
                          gpCentral->file()->directoryList(gstrData + "/services", files);
                          for (list<string>::iterator j = files.begin(); j != files.end(); j++)
                          {
                            if ((*j) != "." && (*j) != ".." && j->size() > 8 && j->substr((j->size() - 8), 8) == ".service")
                            {
                              struct stat tStat;
                              if (stat((gstrData + (string)"/services/" + (*j)).c_str(), &tStat) == 0)
                              {
                                services[j->substr(0, (j->size() - 8))] = "disabled";
                              }
                            }
                          }
                          files.clear();
                          gpCentral->file()->directoryList(gstrData + "/enabled", files);
                          for (list<string>::iterator j = files.begin(); j != files.end(); j++)
                          {
                            if ((*j) != "." && (*j) != ".." && j->size() > 8 && j->substr((j->size() - 8), 8) == ".service")
                            {
                              struct stat tStat;
                              if (stat((gstrData + (string)"/enabled/" + (*j)).c_str(), &tStat) == 0)
                              {
                                services[j->substr(0, (j->size() - 8))] = "enabled";
                              }
                            }
                          }
                          for (map<string, service *>::iterator j = gServices.begin(); j != gServices.end(); j++)
                          {
                            services[j->first] = ((j->second->nPid != -1)?"active":"enabled");
                          }
                          ptJson->m["Response"] = new Json(services);
                          services.clear();
                        }
                      }
                      // }}}
                      // {{{ reload
                      else if (ptJson->m["Function"]->v == "reload")
                      {
                        bProcessed = serviceReload(strService, strError);
                      }
                      // }}}
                      // {{{ restart
                      else if (ptJson->m["Function"]->v == "restart")
                      {
                        bProcessed = serviceRestart(strService, strError);
                      }
                      // }}}
                      // {{{ start
                      else if (ptJson->m["Function"]->v == "start")
                      {
                        bProcessed = serviceStart(strService, strError);
                      }
                      // }}}
                      // {{{ stop
                      else if (ptJson->m["Function"]->v == "stop")
                      {
                        bProcessed = serviceStop(strService, strError);
                      }
                      // }}}
                      // {{{ invalid 
                      else
                      {
                        strError = "Please a valid Function:  disable, enable, list, reload, restart, start, stop.";
                      }
                      // }}}
                    }
                    else
                    {
                      strError = "Please provide the Function.";
                    }
                    ptJson->insert("Status", ((bProcessed)?"okay":"error"));
                    if (!strError.empty())
                    {
                      ptJson->insert("Error", strError);
                    }
                    sockets[fds[i].fd][1].append(ptJson->json(strJson)+"\n");
                    delete ptJson;
                  }
                }
                else
                {
                  removals.push_back(fds[i].fd);
                  if (nReturn < 0)
                  {
                    ssMessage.str("");
                    ssMessage << strPrefix << "->read(" << errno << ") error [" << fdUnix << "," << fds[i].fd << "]:  " << strerror(errno);
                    gpCentral->log(ssMessage.str());
                  }
                }
              }
              // }}}
              // {{{ write
              if (fds[i].revents & POLLOUT)
              {
                if ((nReturn = write(fds[i].fd, sockets[fds[i].fd][1].c_str(), sockets[fds[i].fd][1].size())) > 0)
                {
                  sockets[fds[i].fd][1].erase(0, nReturn);
                }
                else
                {
                  removals.push_back(fds[i].fd);
                  if (nReturn < 0)
                  {
                    ssMessage.str("");
                    ssMessage << strPrefix << "->write(" << errno << ") error [" << fdUnix << "," << fds[i].fd << "]:  " << strerror(errno);
                    gpCentral->log(ssMessage.str());
                  }
                }
              }
              // }}}
            }
          }
          // }}}
        }
        else if (nReturn < 0 && errno != EINTR)
        {
          bExit = true;
          ssMessage.str("");
          ssMessage << strPrefix << "->poll(" << errno << ") error:  " << strerror(errno);
          gpCentral->notify(ssMessage.str());
        }
        delete[] fds;
        while (!removals.empty())
        {
          if (sockets.find(removals.front()) != sockets.end())
          {
            sockets[removals.front()].clear();
            sockets.erase(removals.front());
            close(removals.front());
          }
          removals.pop_front();
        }
        for (map<string, service *>::iterator i = gServices.begin(); i != gServices.end(); i++)
        {
          if (i->second->nPid != -1)
          {
            stringstream ssProc;
            ssProc << "/proc/" << i->second->nPid;
            if (!i->second->bStopped && !gpCentral->file()->directoryExist(ssProc.str().c_str()))
            {
              bool bCrashed = true;
              if (!i->second->strPidFile.empty() && !i->second->bDetached)
              {
                time_t CTime[2];
                ifstream inPid;
                pid_t nPid = 0;
                gpCentral->log((string)"main() [" + i->first + (string)"]:  Service detached.");
                time(&(CTime[0]));
                usleep(250000);
                time(&(CTime[1]));
                while (nPid == 0 && (CTime[1] - CTime[0]) < 5)
                {
                  inPid.open(i->second->strPidFile.c_str());
                  if (inPid)
                  {
                    inPid >> nPid;
                  }
                  inPid.close();
                  usleep(100000);
                  time(&(CTime[1]));
                }
                if (nPid != 0)
                {
                  ofstream outPid;
                  i->second->bDetached = true;
                  i->second->nPid = nPid;
                  outPid.open((gstrData + (string)"/active/" + i->first + (string)".pid").c_str());
                  if (outPid)
                  {
                    bCrashed = false;
                    outPid << nPid << endl;
                  }
                  else
                  {
                    ssMessage.str("");
                    ssMessage << "main()->ifstream::open(" << errno << ") error [" << i->first << "," << gstrData << "/active/" << i->first << ".pid]:  " << strerror(errno);
                    gpCentral->log(ssMessage.str());
                  }
                  outPid.close();
                }
                else
                {
                  ssMessage.str("");
                  ssMessage << "main()->ifstream::open(" << errno << ") error [" << i->first << (string)"," + i->second->strPidFile << "]:  " << strerror(errno);
                  gpCentral->log(ssMessage.str());
                }
              }
              if (bCrashed)
              {
                gpCentral->log((string)"main() [" + i->first + (string)"]:  Service crashed.");
                if (serviceStop(i->first, strError))
                {
                  if (i->second->strRestart == "always")
                  {
                    time_t CTime;
                    time(&CTime);
                    if ((CTime - i->second->CStart) < 60)
                    {
                      i->second->unCrashes++;
                    }
                    else
                    {
                      i->second->unCrashes = 0;
                    }
                    if (i->second->unCrashes <= 1)
                    {
                      if (!serviceStart(i->first, strError))
                      {
                        gpCentral->log((string)"main()->serviceStart() error [" + i->first + (string)"]:  " + strError);
                      }
                    }
                  }
                }
                else
                {
                  gpCentral->log((string)"main()->serviceStop() error [" + i->first + (string)"]:  " + strError);
                }
              }
            }
          }
          if (i->second->unCrashes > 0)
          {
            if (i->second->strRestart == "always")
            {
              if (i->second->unCrashes >= 10)
              {
                i->second->unCrashes = 0;
                gpCentral->log((string)"main() [" + i->first + (string)"]:  Leaving service stopped due to too many crashes");
              }
              else
              {
                time_t CTime;
                time(&CTime);
                if ((CTime - i->second->CStart) >= 60)
                {
                  if (!serviceStart(i->first, strError))
                  {
                    gpCentral->log((string)"main()->serviceStart() error [" + i->first + (string)"]:  " + strError);
                  }
                }
              }
            }
            else
            {
              i->second->unCrashes = 0;
            }
          }
        }
      }
      while (!sockets.empty())
      {
        close(sockets.begin()->first);
        sockets.begin()->second.clear();
        sockets.erase(sockets.begin()->first);
      }
      if (fdUnix != -1)
      {
        close(fdUnix);
        ssMessage.str("");
        ssMessage << strPrefix << "->close() [" << UNIX_SOCKET << "," << fdUnix << "]:  Closed socket.";
        gpCentral->log(ssMessage.str());
        remove(UNIX_SOCKET);
        ssMessage.str("");
        ssMessage << strPrefix << "->remove() [" << UNIX_SOCKET << "]:  Removed socket.";
        gpCentral->log(ssMessage.str());
      }
      while (!gServices.empty())
      {
        if (!serviceRemove(gServices.begin()->first, strError))
        {
          ssMessage.str("");
          ssMessage << strPrefix << "->serviceRemove() error [" << gServices.begin()->first << "]:  " << strError;
          gpCentral->log(ssMessage.str());
          gServices.begin()->second->environment.clear();
          delete gServices.begin()->second;
          gServices.erase(gServices.begin());
        }
      }
      // {{{ check pid file
      if (gpCentral->file()->fileExist(gstrData + PID))
      {
        remove((gstrData + PID).c_str());
      }
      // }}}
    }
    // }}}
    // {{{ usage statement
    else
    {
      mUSAGE(argv[0]);
    }
    // }}}
  }
  else
  {
    cerr << strPrefix << "->Central error:  " << strError << endl;
  }
  delete gpCentral;

  return 0;
}
// }}}
// {{{ service
// {{{ serviceActive()
bool serviceActive(const string strService, string &strError)
{
  bool bResult = false;

  if (serviceExist(strService, strError) && gServices[strService]->nPid != -1)
  {
    bResult = true;
  }

  return bResult;
}
// }}}
// {{{ serviceAdd()
bool serviceAdd(const string strService, string &strError)
{
  bool bResult = false;

  if (serviceExist(strService, strError))
  {
    bResult = true;
  }
  else
  {
    ifstream inService((gstrData + (string)"/services/" + strService + (string)".service").c_str());
    string strLine;
    stringstream ssJson;
    Json *ptJson;
    while (getline(inService, strLine))
    {
      ssJson << strLine;
    }
    inService.close();
    ptJson = new Json(ssJson.str());
    if (ptJson->m.find("ExecStart") != ptJson->m.end() && !ptJson->m["ExecStart"]->v.empty())
    {
      service *ptService = new service;
      bResult = true;
      ptService->bDetached = false;
      ptService->bStopped = false;
      ptService->CStart = 0;
      ptService->nPid = -1;
      ptService->unCrashes = 0;
      ptService->strExecStart = ptJson->m["ExecStart"]->v;
      if (ptJson->m.find("ExecStartPost") != ptJson->m.end() && !ptJson->m["ExecStartPost"]->v.empty())
      {
        ptService->strExecStartPost = ptJson->m["ExecStartPost"]->v;
      }
      if (ptJson->m.find("ExecStartPre") != ptJson->m.end() && !ptJson->m["ExecStartPre"]->v.empty())
      {
        ptService->strExecStartPre = ptJson->m["ExecStartPre"]->v;
      }
      if (ptJson->m.find("ExecStopPost") != ptJson->m.end() && !ptJson->m["ExecStopPost"]->v.empty())
      {
        ptService->strExecStopPost = ptJson->m["ExecStopPost"]->v;
      }
      if (ptJson->m.find("Environment") != ptJson->m.end() && !ptJson->m["Environment"]->l.empty())
      {
        for (list<Json *>::iterator i = ptJson->m["Environment"]->l.begin(); i != ptJson->m["Environment"]->l.end(); i++)
        {
          if (!(*i)->v.empty())
          {
            ptService->environment.push_back((*i)->v);
          }
        }
      }
      ptService->strLimitCore = "0";
      if (ptJson->m.find("LimitCORE") != ptJson->m.end() && !ptJson->m["LimitCORE"]->v.empty())
      {
        ptService->strLimitCore = ptJson->m["LimitCORE"]->v;
      }
      ptService->strLimitNoFile = "1024";
      if (ptJson->m.find("LimitNOFILE") != ptJson->m.end() && !ptJson->m["LimitNOFILE"]->v.empty())
      {
        ptService->strLimitNoFile = ptJson->m["LimitNOFILE"]->v;
      }
      if (ptJson->m.find("PIDFile") != ptJson->m.end() && !ptJson->m["PIDFile"]->v.empty())
      {
        ptService->strPidFile = ptJson->m["PIDFile"]->v;
      }
      ptService->strRestart = "always";
      if (ptJson->m.find("Restart") != ptJson->m.end() && !ptJson->m["Restart"]->v.empty())
      {
        ptService->strRestart = ptJson->m["Restart"]->v;
      }
      gServices[strService] = ptService;
    }
    else
    {
      strError = "Please provide the Command within the Service configuration.";
    }
    delete ptJson;
    strError.clear();
  }

  return bResult;
}
// }}}
// {{{ serviceDisable()
bool serviceDisable(const string strService, string &strError)
{
  bool bResult = false;

  if (serviceRemove(strService, strError) && serviceUnlink(strService, strError))
  {
    bResult = true;
    gpCentral->log((string)"serviceDisable() [" + strService + (string)"]:  Disabled service.");
  }

  return bResult;
}
// }}}
// {{{ serviceEnable()
bool serviceEnable(const string strService, string &strError)
{
  bool bResult = false;

  if (serviceLink(strService, strError) && serviceAdd(strService, strError))
  {
    bResult = true;
    gpCentral->log((string)"serviceEnable() [" + strService + (string)"]:  Enabled service.");
  }

  return bResult;
}
// }}}
// {{{ serviceExist()
bool serviceExist(const string strService, string &strError)
{
  bool bResult = false;

  if (serviceValid(strService, strError))
  {
    if (gServices.find(strService) != gServices.end())
    {
      bResult = true;
    }
    else
    {
      strError = "Please provide a valid Service.";
    }
  }

  return bResult;
}
// }}}
// {{{ serviceLink()
bool serviceLink(const string strService, string &strError)
{
  bool bResult = false;
  stringstream ssMessage;

  if (serviceValid(strService, strError))
  {
    struct stat tStat;
    if (stat((gstrData + (string)"/services/" + strService + (string)".service").c_str(), &tStat) == 0)
    {
      if (stat((gstrData + (string)"/enabled/" + strService + (string)".service").c_str(), &tStat) == 0 || symlink((gstrData + (string)"/services/" + strService + (string)".service").c_str(), (gstrData + (string)"/enabled/" + strService + (string)".service").c_str()) == 0)
      {
        bResult = true;
      }
      else
      {
        ssMessage.str("");
        ssMessage << "symlink(" << errno << ") " << strerror(errno);
        strError = ssMessage.str();
      }
    }
    else
    {
      strError = "Please provide a valid Service.";
    }
  }

  return bResult;
}
// }}}
// {{{ serviceReload()
bool serviceReload(const string strService, string &strError)
{
  bool bResult = false;
  stringstream ssMessage;

  if (serviceActive(strService, strError))
  {
    gpCentral->log((string)"serviceReload() [" + strService + (string)"]:  Reloading service.");
    if (kill(gServices[strService]->nPid, SIGHUP) == 0)
    {
      bResult = true;
      gpCentral->log((string)"serviceReload() [" + strService + (string)"]:  Reloaded service.");
    }
    else
    {
      ssMessage.str("");
      ssMessage << "kill(" << errno << ") " << strerror(errno);
      strError = ssMessage.str();
    }
  }

  return bResult;
}
// }}}
// {{{ serviceRemove()
bool serviceRemove(const string strService, string &strError)
{
  bool bResult = false;

  if (serviceExist(strService, strError) && (!serviceActive(strService, strError) || serviceStop(strService, strError)))
  {
    bResult = true;
    gServices[strService]->environment.clear();
    delete gServices[strService];
    gServices.erase(strService);
  }

  return bResult;
}
// }}}
// {{{ serviceRestart()
bool serviceRestart(const string strService, string &strError)
{
  bool bResult = false;

  if (serviceStop(strService, strError) && serviceStart(strService, strError))
  {
    bResult = true;
  }

  return bResult;
}
// }}}
// {{{ serviceStart()
bool serviceStart(const string strService, string &strError)
{
  bool bResult = false;
  stringstream ssMessage;

  if (serviceExist(strService, strError) && !serviceActive(strService, strError))
  {
    char *args[100], *env[100], *pszArgument;
    pid_t nPid;
    size_t unArgIndex = 0, unEnvIndex = 0;
    string strArgument;
    stringstream ssExecStart;
    if (!gServices[strService]->strExecStartPre.empty())
    {
      system(gServices[strService]->strExecStartPre.c_str());
    }
    ssExecStart.str(gServices[strService]->strExecStart);
    while (ssExecStart >> strArgument)
    {
      pszArgument = new char[strArgument.size() + 1];
      strcpy(pszArgument, strArgument.c_str());
      args[unArgIndex++] = pszArgument;
    }
    args[unArgIndex] = NULL;
    if (!gServices[strService]->environment.empty())
    {
      for (list<string>::iterator i = gServices[strService]->environment.begin(); i != gServices[strService]->environment.end(); i++)
      {
        strArgument = (*i);
        pszArgument = new char[strArgument.size() + 1];
        strcpy(pszArgument, strArgument.c_str());
        env[unEnvIndex++] = pszArgument;
      }
      env[unEnvIndex] = NULL;
    }
    strError.clear();
    gpCentral->log((string)"serviceStart() [" + strService + (string)"]:  Starting service.");
    if ((nPid = fork()) == 0)
    {
      rlimit tResourceLimit;
      // {{{ core limit
      if (gServices[strService]->strLimitCore == "infinity")
      {
        tResourceLimit.rlim_cur = RLIM_INFINITY;
      }
      else
      {
        stringstream ssLimit(gServices[strService]->strLimitCore);
        ssLimit >> tResourceLimit.rlim_cur;
      }
      tResourceLimit.rlim_max = gResourceLimitCoreHard;
      if (gResourceLimitCoreSoft != RLIM_INFINITY && (tResourceLimit.rlim_cur == RLIM_INFINITY || tResourceLimit.rlim_cur > gResourceLimitCoreSoft))
      {
        tResourceLimit.rlim_cur = gResourceLimitCoreSoft;
      }
      if (tResourceLimit.rlim_cur != gResourceLimitCoreSoft)
      {
        if (setrlimit(RLIMIT_CORE, &tResourceLimit) == 0)
        {
          ssMessage.str("");
          ssMessage << "serviceStart()->setrlimit() [" + strService + ",RLIMIT_CORE]:  Set the core limit to ";
          if (tResourceLimit.rlim_cur == RLIM_INFINITY)
          {
            ssMessage << "infinity";
          }
          else
          {
            ssMessage << tResourceLimit.rlim_cur;
          }
          ssMessage << ".";
          gpCentral->log(ssMessage.str());
        }
        else
        {
          ssMessage.str("");
          ssMessage << "serviceStart()->setrlimit(" << errno << ") error [" + strService + ",RLIMIT_CORE]:  " << strerror(errno);
          gpCentral->notify(ssMessage.str());
        }
      }
      // }}}
      // {{{ file descriptor
      if (gServices[strService]->strLimitNoFile == "infinity")
      {
        tResourceLimit.rlim_cur = RLIM_INFINITY;
      }
      else
      {
        stringstream ssLimit(gServices[strService]->strLimitNoFile);
        ssLimit >> tResourceLimit.rlim_cur;
      }
      tResourceLimit.rlim_max = gResourceLimitNoFileHard;
      if (gResourceLimitNoFileSoft != RLIM_INFINITY && (tResourceLimit.rlim_cur == RLIM_INFINITY || tResourceLimit.rlim_cur > gResourceLimitNoFileSoft))
      {
        tResourceLimit.rlim_cur = gResourceLimitNoFileSoft;
      }
      if (tResourceLimit.rlim_cur != gResourceLimitNoFileSoft)
      {
        if (setrlimit(RLIMIT_NOFILE, &tResourceLimit) == 0)
        {
          ssMessage.str("");
          ssMessage << "serviceStart()->setrlimit() [" + strService + ",RLIMIT_NOFILE]:  Set the file descriptor limit to ";
          if (tResourceLimit.rlim_cur == RLIM_INFINITY)
          {
            ssMessage << "infinity";
          }
          else
          {
            ssMessage << tResourceLimit.rlim_cur;
          }
          ssMessage << ".";
          gpCentral->log(ssMessage.str());
        }
        else
        {
          ssMessage.str("");
          ssMessage << "serviceStart()->setrlimit(" << errno << ") error [" + strService + ",RLIMIT_NOFILE]:  " << strerror(errno);
          gpCentral->notify(ssMessage.str());
        }
      }
      // }}}
      execve(args[0], args, ((unEnvIndex > 0)?env:environ));
      ssMessage.str("");
      ssMessage << "serviceStart()->execve(" << errno << ") error [" << args[0] << "]:  " << strerror(errno);
      gpCentral->log(ssMessage.str());
      _exit(1);
    }
    else if (nPid > 0)
    {
      ofstream outService;
      bResult = true;
      time(&(gServices[strService]->CStart));
      gServices[strService]->nPid = nPid;
      outService.open((gstrData + (string)"/active/" + strService + (string)".pid").c_str());
      if (outService)
      {
        outService << nPid << endl;
      }
      else
      {
        ssMessage.str("");
        ssMessage << "serviceStart()->ifstream::open(" << errno << ") error [" << gstrData << "/active/" << strService << ".pid]:  " << strerror(errno);
        gpCentral->log(ssMessage.str());
      }
      outService.close();
      gServices[strService]->bStopped = false;
      if (!gServices[strService]->strExecStartPost.empty())
      {
        system(gServices[strService]->strExecStartPost.c_str());
      }
      gpCentral->log((string)"serviceStart() [" + strService + (string)"]:  Started service.");
    }
    else
    {
      ssMessage.str("");
      ssMessage << "fork(" << errno << ") " << strerror(errno);
      strError = ssMessage.str();
    }
    for (size_t i = 0; i < unArgIndex; i++)
    {
      delete args[i];
    }
    for (size_t i = 0; i < unEnvIndex; i++)
    {
      delete env[i];
    }
  }

  return bResult;
}
// }}}
// {{{ serviceStop()
bool serviceStop(const string strService, string &strError)
{
  bool bResult = false;
  stringstream ssMessage;

  if (serviceActive(strService, strError))
  {
    int nStatus;
    gpCentral->log((string)"serviceStop() [" + strService + (string)"]:  Stopping service.");
    gServices[strService]->bStopped = true;
    if (kill(gServices[strService]->nPid, SIGTERM) == 0 || errno == ESRCH)
    {
      bool bExit = false;
      time_t CTime[2];
      time(&(CTime[0]));
      while (!bExit)
      {
        if (waitpid(gServices[strService]->nPid, &nStatus, 0) == gServices[strService]->nPid || errno == ECHILD)
        {
          bExit = bResult = true;
          gServices[strService]->bDetached = false;
          gServices[strService]->nPid = -1;
          remove((gstrData + (string)"/active/" + strService + (string)".pid").c_str());
          if (!gServices[strService]->strExecStopPost.empty())
          {
            system(gServices[strService]->strExecStopPost.c_str());
          }
          gpCentral->log((string)"serviceStop() [" + strService + (string)"]:  Stopped service.");
        }
        else if (errno != EINTR)
        {
          bExit = true;
          ssMessage.str("");
          ssMessage << "waitpid(" << errno << ") " << strerror(errno);
          strError = ssMessage.str();
        }
        if (!bResult)
        {
          usleep(250000);
          time(&(CTime[1]));
          if ((CTime[1] - CTime[0]) >= 300)
          {
            bExit = true;
          }
        }
      }
      if (!bResult)
      {
        if (kill(gServices[strService]->nPid, SIGKILL) == 0 || errno == ESRCH)
        {
          bResult = true;
          gServices[strService]->nPid = -1;
          remove((gstrData + (string)"/active/" + strService + (string)".pid").c_str());
          if (!gServices[strService]->strExecStopPost.empty())
          {
            system(gServices[strService]->strExecStopPost.c_str());
          }
          gpCentral->log((string)"serviceStop() [" + strService + (string)"]:  Stopped service forcefully.");
        }
        else
        {
          ssMessage.str("");
          ssMessage << "kill(" << errno << ") " << strerror(errno);
          strError = ssMessage.str();
        }
      }
    }
    else
    {
      ssMessage.str("");
      ssMessage << "kill(" << errno << ") " << strerror(errno);
      strError = ssMessage.str();
    }
  }

  return bResult;
}
// }}}
// {{{ serviceUnlink()
bool serviceUnlink(const string strService, string &strError)
{
  bool bResult = false;
  stringstream ssMessage;

  if (serviceValid(strService, strError))
  {
    struct stat tStat;
    if (stat((gstrData + (string)"/enabled/" + strService + (string)".service").c_str(), &tStat) == 0)
    {
      if (remove((gstrData + (string)"/enabled/" + strService + (string)".service").c_str()) == 0)
      {
        bResult = true;
      }
      else
      {
        ssMessage.str("");
        ssMessage << "remove(" << errno << ") " << strerror(errno);
        strError = ssMessage.str();
      }
    }
    else
    {
      ssMessage.str("");
      ssMessage << "stat(" << errno << ") " << strerror(errno);
      strError = ssMessage.str();
    }
  }

  return bResult;
}
// }}}
// {{{ serviceValid()
bool serviceValid(const string strService, string &strError)
{
  bool bResult = false;

  if (!strService.empty())
  {
    if (strService.find("/") == string::npos)
    {
      bResult = true;
    }
    else
    {
      strError = "The Service cannot contain the following characters:  /.";
    }
  }
  else
  {
    strError = "Please provide the Service.";
  }

  return bResult;
}
// }}}
// }}}
// {{{ sighandle()
void sighandle(const int nSignal, siginfo_t *ptInfo, void *vptContext)
{
  pid_t nPid = getpid();
  stringstream ssMessage;

  if (nPid != ptInfo->si_pid && nSignal != SIGCHLD)
  {
    bool bUse = true;
    for (map<string, service *>::iterator i = gServices.begin(); bUse && i != gServices.end(); i++)
    {
      if (i->second->nPid == ptInfo->si_pid)
      {
        bUse = false;
      }
    }
    if (bUse)
    {
      string strError, strSignal;
      stringstream ssMessage, ssPrefix;
      ssPrefix << "sighandle(" << nSignal << ")";
      ssMessage.str("");
      ssMessage << ssPrefix.str() << ":  " << sigstring(strSignal, nSignal);
      if (nSignal == SIGINT || nSignal == SIGTERM)
      {
        gpCentral->log(ssMessage.str());
      }
      else
      {
        gpCentral->notify(ssMessage.str());
      }
      gbShutdown = true;
    }
  }
}
// }}}
