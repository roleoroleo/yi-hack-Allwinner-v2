/*
 * Copyright (c) 2025 roleo.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * A ServerMediaSubsession object
 */

#ifndef _FILE_SERVER_MEDIA_SUBSESSION_BC_HH
#define _FILE_SERVER_MEDIA_SUBSESSION_BC_HH

#ifndef _SERVER_MEDIA_SESSION_HH
#include "ServerMediaSession.hh"
#endif
#ifndef _ON_DEMAND_SERVER_MEDIA_SUBSESSION_BC_HH
#include "OnDemandServerMediaSubsession_BC.hh"
#endif

class FileServerMediaSubsession_BC: public OnDemandServerMediaSubsession_BC {
protected: // we're a virtual base class
  FileServerMediaSubsession_BC(UsageEnvironment& env, char const* fileName,
			       Boolean reuseFirstSource);
  virtual ~FileServerMediaSubsession_BC();

protected:
  char const* fFileName;
  u_int64_t fFileSize; // if known
};

#endif
