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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Doug Turner <dougt@meer.net> (original author)
 *   Darin Fisher <darin@meer.net>
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

#ifndef nsCCNxChannel_h__
#define nsCCNxChannel_h__

//#include "nsStringAPI.h"
#include "nsString.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsIIOService.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"

#include "nsHashPropertyBag.h"
#include "nsInputStreamPump.h"
#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"

#define NS_CCNX_CHANNEL_CLASSNAME               \
  "nsCCNxChannel"

/* cb5380c1-8767-4004-8c86-55f38013dbb2 */
#define NS_CCNX_CHANNEL_CID                             \
  { 0xcb5380c1, 0x8767, 0x4004,                         \
    {0x8c, 0x86, 0x55, 0xf3, 0x80, 0x13, 0xdb, 0xb2} }

class nsCCNxChannel : public nsIChannel
                    , public nsHashPropertyBag
                    , private nsIStreamListener {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICHANNEL
  NS_DECL_NSIREQUEST
  //  NS_DECL_NSICCNxCHANNEL

  nsCCNxChannel(nsIURI *aURI);
  virtual ~nsCCNxChannel();

  nsresult Init();

  nsIURI *URI() {
    return mURI;
  }
  void SetURI(nsIURI *uri) {
    mURI = uri;
    mOriginalURI = uri;
  }
  nsIURI *OriginalURI() {
    return mOriginalURI;
  }

  bool IsPending() const {
    // TODO
    return mPump;
  }

  // Set the content length that should be reported for this channel.  Pass -1
  // to indicate an unspecified content length.
  void SetContentLength64(PRInt64 len);
  PRInt64 ContentLength64();

private:
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

  void CallbacksChanged() {
    mProgressSink = nsnull;
    mQueriedProgressSink = false;
    //    OnCallbacksChanged();
  }

  // Called to setup mPump and call AsyncRead on it.
  nsresult BeginPumpingData();

  nsresult OpenContentStream(bool async, nsIInputStream **stream,
                             nsIChannel** channel);

private:
  nsRefPtr<nsInputStreamPump>         mPump;

  nsCOMPtr<nsIURI>                    mOriginalURI;
  nsCOMPtr<nsIURI>                    mURI;

  nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;
  nsCOMPtr<nsIProgressEventSink>      mProgressSink;
  nsCOMPtr<nsISupports>               mSecurityInfo;
  nsCOMPtr<nsISupports>               mOwner;
  nsCString                           mContentType;
  nsCString                           mContentCharset;
  nsCOMPtr<nsIIOService>              mIOService;
  nsCOMPtr<nsIChannel>                mTransport;
  nsresult                            mStatus;
  nsCOMPtr<nsILoadGroup>              mLoadGroup;
  PRUint32                            mLoadFlags;
  bool                                mQueriedProgressSink;
  bool                                mWaitingOnAsyncRedirect;

protected:
  nsCOMPtr<nsIStreamListener>         mListener;
  nsCOMPtr<nsISupports>               mListenerContext;
};

#endif /* nsCCNxChannel_h__ */
