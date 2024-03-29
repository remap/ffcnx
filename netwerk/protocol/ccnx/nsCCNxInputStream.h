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

#ifndef nsCCNxInputStream_h__
#define nsCCNxInputStream_h__

#include "nsIAsyncInputStream.h"
#include "nsIEventTarget.h"
#include "nsCOMPtr.h"

class nsCCNxTransport;

class nsCCNxInputStream : public nsIAsyncInputStream {
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  nsCCNxInputStream(nsCCNxTransport *);
  virtual ~nsCCNxInputStream();

  // called by the ndn transport on the ndn thread ??
  void OnCCNxReady(nsresult condition);

  bool IsReferenced()     { return mReaderRefCnt > 0; }
  nsresult Condition()    { return mCondition; }
  //  PRUint64 ByteCount()    { return mByteCount; }

private:
  nsCCNxTransport                    *mTransport;
  nsrefcnt                            mReaderRefCnt;
  PRUint64                            mByteCount;

  // access to these is protected by mTransport->mLock
  nsresult                            mCondition;
  nsCOMPtr<nsIInputStreamCallback>    mCallback;
  PRUint32                            mCallbackFlags;

};

#endif // nsCCNxInputStream_h__
