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

#include "FileServerMediaSubsession_BC.hh"

FileServerMediaSubsession_BC
::FileServerMediaSubsession_BC(UsageEnvironment& env, char const* fileName,
			    Boolean reuseFirstSource)
  : OnDemandServerMediaSubsession_BC(env, reuseFirstSource),
    fFileSize(0) {
  fFileName = strDup(fileName);
}

FileServerMediaSubsession_BC::~FileServerMediaSubsession_BC() {
  delete[] (char*)fFileName;
}
