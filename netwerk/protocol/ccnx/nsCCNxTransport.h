/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jiwen Cai <jwcai@cs.ucla.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsCCNxTransport_h__
#define nsCCNxTransport_h__

#include "nsCCNxInputStream.h"
#include "nsCCNxTransportService.h"

#include "mozilla/Mutex.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsITransport.h"

extern "C" {
#include <ccn/ccn.h>
#include <ccn/charbuf.h>
#include <ccn/uri.h>
#include <ccn/fetch.h>
}

class nsCCNxTransport : public nsITransport {
  typedef mozilla::Mutex Mutex;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITRANSPORT

  nsCCNxTransport();
  virtual ~nsCCNxTransport();

  // this method instructs the CCNx transport to open a transport of a
  // given type(s) to the given name
  nsresult Init(const char *ndnName);

private:

  void CCNX_Close();
  void CCNX_MakeTemplate(int allow_stale);
  //
  // mCCNx access methods: called with mLock held.
  //
  struct ccn_fetch_stream *CCNX_GetLocked();
  void CCNX_ReleaseLocked(struct ccn_fetch_stream* ccnfs);

private:

  Mutex                            mLock;
  // connector to ccnd
  struct ccn                       *mCCNx;
  struct ccn_fetch                 *mCCNxfetch;
  struct ccn_charbuf               *mCCNxname;
  struct ccn_charbuf               *mCCNxtmpl;
  struct ccn_fetch_stream          *mCCNxstream;

  // mCCNx is closed when mFDref goes to zero
  nsrefcnt                          mCCNxref;
  bool                              mCCNxonline;
  bool                              mInputClosed;

  nsCCNxInputStream                 mInput;
  nsCCNxTransportService*           mService;

  friend class nsCCNxInputStream;
};

#endif // nsCCNxTransport_h__
