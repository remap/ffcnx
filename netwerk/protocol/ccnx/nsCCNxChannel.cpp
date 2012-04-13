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

#include "nsCCNxChannel.h"
#include "nsCCNxCore.h"

#include "nsChannelProperties.h"
#include "nsMimeTypes.h"
#include "nsCOMArray.h"
#include "nsIContentSniffer.h"
#include "nsIOService.h"
#include "nsILoadGroup.h"
#include "nsIURL.h"

//#include <ccn/ccn.h>
#define NS_GENERIC_CONTENT_SNIFFER \
  "@mozilla.org/network/content-sniffer;1"

NS_IMPL_ISUPPORTS4(nsCCNxChannel,
                   nsIChannel,
                   nsIRequest,
                   nsIStreamListener,
                   nsIRequestObserver)

// This class is used to suspend a request across a function scope.
class ScopedRequestSuspender {
public:
  ScopedRequestSuspender(nsIRequest *request)
    : mRequest(request) {
    if (mRequest && NS_FAILED(mRequest->Suspend())) {
      NS_WARNING("Couldn't suspend pump");
      mRequest = nsnull;
    }
  }
  ~ScopedRequestSuspender() {
    if (mRequest)
      mRequest->Resume();
  }
private:
  nsIRequest *mRequest;
};

// Used to suspend data events from mPump within a function scope.  This is
// usually needed when a function makes callbacks that could process events.
#define SUSPEND_PUMP_FOR_SCOPE() \
  ScopedRequestSuspender pump_suspender__(mPump);

nsCCNxChannel::nsCCNxChannel(nsIURI *aURI)
    : mStatus(NS_OK) 
    , mLoadFlags(LOAD_NORMAL)
    , mQueriedProgressSink(true)
      //    , mSynthProgressEvents(flase)
      //    , mWasOpened(false)
    , mWaitingOnAsyncRedirect(false) {
  SetURI(aURI);
  mContentType.AssignLiteral(UNKNOWN_CONTENT_TYPE);
}

nsCCNxChannel::~nsCCNxChannel() {
}

// This method must be called to initialize the basechannel instance.
nsresult nsCCNxChannel::Init() {
  //  nsresult rv;
  return nsHashPropertyBag::Init();
}

void
nsCCNxChannel::SetContentLength64(PRInt64 len) {
  // XXX: Storing the content-length as a property may not be what we want.
  //      It has the drawback of being copied if we redirect this channel.
  //      Maybe it is time for nsIChannel2.
  SetPropertyAsInt64(NS_CHANNEL_PROP_CONTENT_LENGTH, len);
}

PRInt64
nsCCNxChannel::ContentLength64() {
  PRInt64 len;
  nsresult rv = GetPropertyAsInt64(NS_CHANNEL_PROP_CONTENT_LENGTH, &len);
  return NS_SUCCEEDED(rv) ? len : -1;
}

nsresult
nsCCNxChannel::BeginPumpingData() {
  nsresult rv;
  nsCOMPtr<nsIInputStream> stream;
  nsCOMPtr<nsIChannel> channel;

  rv = OpenContentStream(true, getter_AddRefs(stream),
                         getter_AddRefs(channel));

  if (NS_FAILED(rv))
    return rv;

  NS_ASSERTION(!stream || !channel, "Got both a channel and a stream?");

  /*
  if (channel) {
      rv = NS_DispatchToCurrentThread(new RedirectRunnable(this, channel));
      if (NS_SUCCEEDED(rv))
          mWaitingOnAsyncRedirect = true;
      return rv;
  }
  */

  rv = nsInputStreamPump::Create(getter_AddRefs(mPump), stream, -1, -1, 0, 0,
                                 true);
  if (NS_SUCCEEDED(rv))
    rv = mPump->AsyncRead(this, nsnull);

  return rv;
}

nsresult
nsCCNxChannel::OpenContentStream(bool async, nsIInputStream **stream,
                                nsIChannel** channel) {
  // 
  if (!async)
    return NS_ERROR_NOT_IMPLEMENTED;

  nsCCNxCore *ndncore = new nsCCNxCore();
  if (!ndncore)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(ndncore);

  nsresult rv = ndncore->Init(this);
  if (NS_FAILED(rv)) {
    NS_RELEASE(ndncore);
    return rv;
  }

  *stream = ndncore;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsCCNxChannel::nsIRequest

NS_IMETHODIMP
nsCCNxChannel::GetName(nsACString &result)
{
  if (!mURI) {
    result.Truncate();
    return NS_OK;
  }
  return mURI->GetSpec(result);
}

NS_IMETHODIMP
nsCCNxChannel::IsPending(bool *result) {
  *result = IsPending();
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxChannel::GetStatus(nsresult *status) {
  *status = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxChannel::Cancel(nsresult status) {
  NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
  nsresult rv = NS_ERROR_FAILURE;

  mStatus = status;
  if (mTransport) {
    rv = mTransport->Cancel(status);
  }
  return rv;
}

NS_IMETHODIMP
nsCCNxChannel::Suspend(void) {
  NS_ENSURE_TRUE(mPump, NS_ERROR_NOT_INITIALIZED);
  return mPump->Suspend();
}

NS_IMETHODIMP
nsCCNxChannel::Resume(void) {
  NS_ENSURE_TRUE(mPump, NS_ERROR_NOT_INITIALIZED);
  return mPump->Resume();
}

NS_IMETHODIMP
nsCCNxChannel::GetLoadFlags(nsLoadFlags *aLoadFlags) {
  *aLoadFlags = mLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxChannel::SetLoadFlags(nsLoadFlags aLoadFlags) {
  mLoadFlags = aLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup) {
  *aLoadGroup = mLoadGroup;
  NS_IF_ADDREF(*aLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxChannel::SetLoadGroup(nsILoadGroup* aLoadGroup) {
  mLoadGroup = aLoadGroup;
  CallbacksChanged();
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsCCNxChannel::nsIChannel

NS_IMETHODIMP
nsCCNxChannel::GetOriginalURI(nsIURI **aURI) {
  *aURI = OriginalURI();
  NS_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxChannel::SetOriginalURI(nsIURI *aURI) {
  NS_ENSURE_ARG_POINTER(aURI);
  mOriginalURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxChannel::GetURI(nsIURI **aURI) {
  NS_IF_ADDREF(*aURI = mURI);
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxChannel::GetOwner(nsISupports **aOwner) {
  NS_IF_ADDREF(*aOwner = mOwner);
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxChannel::SetOwner(nsISupports *aOwner) {
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxChannel::GetNotificationCallbacks(nsIInterfaceRequestor **aCallbacks) {
  NS_IF_ADDREF(*aCallbacks = mCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxChannel::SetNotificationCallbacks(nsIInterfaceRequestor *aCallbacks) {
  mCallbacks = aCallbacks;
  CallbacksChanged();
  return NS_OK;
}

NS_IMETHODIMP 
nsCCNxChannel::GetSecurityInfo(nsISupports **aSecurityInfo) {
  NS_IF_ADDREF(*aSecurityInfo = mSecurityInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxChannel::GetContentType(nsACString &aContentType) {
  aContentType = mContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxChannel::SetContentType(const nsACString &aContentType) {
  // mContentCharset is unchanged if not parsed
  bool dummy;
  net_ParseContentType(aContentType, mContentType, mContentCharset, &dummy);
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxChannel::GetContentCharset(nsACString &aContentCharset) {
  aContentCharset = mContentCharset;
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxChannel::SetContentCharset(const nsACString &aContentCharset) {
  mContentCharset = aContentCharset;
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxChannel::GetContentLength(PRInt32 *aContentLength) {
  PRInt64 len = ContentLength64();
  if (len > PR_INT32_MAX || len < 0)
    *aContentLength = -1;
  else
    *aContentLength = (PRInt32) len;
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxChannel::SetContentLength(PRInt32 aContentLength)
{
  SetContentLength64(aContentLength);
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxChannel::Open(nsIInputStream **result) {
  // OpenInputStream in old API?
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCCNxChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt) {
  // AsyncRead in old API?
  nsresult rv;

  mListener = listener;
  mListenerContext = ctxt;

  rv = BeginPumpingData();
  if (NS_FAILED(rv)) {
    mPump = nsnull;
    mListener = nsnull;
    mListenerContext = nsnull;
    mCallbacks = nsnull;
    return rv;
  }

  // At this point, we are going to return success no mater what
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxChannel::GetContentDisposition(PRUint32 *aContentDisposition) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCCNxChannel::GetContentDispositionFilename(nsAString &aFilename) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCCNxChannel::GetContentDispositionHeader(nsACString &aHeader)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// nsCCNxChannel::nsIStreamListener

NS_IMETHODIMP
nsCCNxChannel::OnDataAvailable(nsIRequest *request, nsISupports *ctxt,
                               nsIInputStream *stream, PRUint32 offset,
                               PRUint32 count) {
  SUSPEND_PUMP_FOR_SCOPE();

  nsresult rv = mListener->OnDataAvailable(this, mListenerContext, stream,
                                           offset, count);
  /*
  if (mSynthProgressEvents && NS_SUCCEEDED(rv)) {
    PRUint64 prog = PRUint64(offset) + count;
    PRUint64 progMax = ContentLength64();
    OnTransportStatus(nsnull, nsITransport::STATUS_READING, prog, progMax);
  }
  */
  return rv;
}

/*
NS_IMETHODIMP
nsCCNxChannel::OnRedirectVerifyCallback(nsresult result) {
  return NS_ERROR_NOT_IMPLEMENTED;
}
*/

//-----------------------------------------------------------------------------
// nsCCNxChannel::nsIRequestObserver

static void
CallTypeSniffers(void *aClosure, const PRUint8 *aData, PRUint32 aCount);

static void
CallUnknownTypeSniffer(void *aClosure, const PRUint8 *aData, PRUint32 aCount);

NS_IMETHODIMP
nsCCNxChannel::OnStartRequest(nsIRequest *request, nsISupports *ctxt) {
  // If our content type is unknown, then use the content type sniffer.  If the
  // sniffer is not available for some reason, then we just keep going as-is.
  if (NS_SUCCEEDED(mStatus) && mContentType.EqualsLiteral(UNKNOWN_CONTENT_TYPE)) {
    mPump->PeekStream(CallUnknownTypeSniffer, static_cast<nsIChannel*>(this));
  }

  // Now, the general type sniffers. Skip this if we have none.
  /*
  if ((mLoadFlags & LOAD_CALL_CONTENT_SNIFFERS) &&
      gIOService->GetContentSniffers().Count() != 0)
    mPump->PeekStream(CallTypeSniffers, static_cast<nsIChannel*>(this));
  */
  SUSPEND_PUMP_FOR_SCOPE();

  return mListener->OnStartRequest(this, mListenerContext);
}

NS_IMETHODIMP
nsCCNxChannel::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                            nsresult status) {
  // If both mStatus and status are failure codes, we keep mStatus as-is since
  // that is consistent with our GetStatus and Cancel methods.
  if (NS_SUCCEEDED(mStatus))
    mStatus = status;

  // Cause IsPending to return false.
  mPump = nsnull;

  mListener->OnStopRequest(this, mListenerContext, mStatus);
  mListener = nsnull;
  mListenerContext = nsnull;

  // No need to suspend pump in this scope since we will not be receiving
  // any more events from it.

  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nsnull, mStatus);

  // Drop notification callbacks to prevent cycles.
  mCallbacks = nsnull;
  CallbacksChanged();

  return NS_OK;
}

//-----------------------------------------------------------------------------
// Static helpers

static void
CallTypeSniffers(void *aClosure, const PRUint8 *aData, PRUint32 aCount) {
  nsIChannel *chan = static_cast<nsIChannel*>(aClosure);

  const nsCOMArray<nsIContentSniffer>& sniffers =
    gIOService->GetContentSniffers();
  PRUint32 length = sniffers.Count();
  for (PRUint32 i = 0; i < length; ++i) {
    nsCAutoString newType;
    nsresult rv =
      sniffers[i]->GetMIMETypeFromContent(chan, aData, aCount, newType);
    if (NS_SUCCEEDED(rv) && !newType.IsEmpty()) {
      chan->SetContentType(newType);
      break;
    }
  }
}

static void
CallUnknownTypeSniffer(void *aClosure, const PRUint8 *aData, PRUint32 aCount) {
  nsIChannel *chan = static_cast<nsIChannel*>(aClosure);

  nsCOMPtr<nsIContentSniffer> sniffer =
    do_CreateInstance(NS_GENERIC_CONTENT_SNIFFER);
  if (!sniffer)
    return;

  nsCAutoString detected;
  nsresult rv = sniffer->GetMIMETypeFromContent(chan, aData, aCount, detected);
  if (NS_SUCCEEDED(rv))
    chan->SetContentType(detected);
}
