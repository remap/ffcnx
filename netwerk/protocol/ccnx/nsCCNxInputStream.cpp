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

#include "mozilla/Mutex.h"
#include "nsStreamUtils.h"

#include "nsCCNxInputStream.h"
#include "nsCCNxTransport.h"
#include "nsCCNxError.h"

using namespace mozilla;

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gCCNxLog;
#endif
#define LOG(args)         PR_LOG(gCCNxLog, PR_LOG_DEBUG, args)

NS_IMPL_QUERY_INTERFACE2(nsCCNxInputStream,
                         nsIInputStream,
                         nsIAsyncInputStream);

nsCCNxInputStream::nsCCNxInputStream(nsCCNxTransport* trans)
    : mTransport(trans)
    , mReaderRefCnt(0)
    , mByteCount(0)
    , mCondition(NS_OK)
    , mCallbackFlags(0) {
  LOG(("create nsCCNxInputStream @%p", this));
}

nsCCNxInputStream::~nsCCNxInputStream() {
  LOG(("destroy nsCCNxInputStream @%p", this));
}

void
nsCCNxInputStream::OnCCNxReady(nsresult condition) {
    // not impletmented yet
}


//-----------------------------------------------------------------------------
// nsISupports Methods

NS_IMETHODIMP_(nsrefcnt)
nsCCNxInputStream::AddRef()
{
  NS_AtomicIncrementRefcnt(mReaderRefCnt);
  LOG(("AddRef nsCCNxInputStream @%p", this));

  return mTransport->AddRef();
}

NS_IMETHODIMP_(nsrefcnt)
nsCCNxInputStream::Release()
{
  if (NS_AtomicDecrementRefcnt(mReaderRefCnt) == 0)
    Close();
  LOG(("Release nsCCNxInputStream @%p", this));
  return mTransport->Release();
}

//-----------------------------------------------------------------------------
// nsIInputStream Methods

NS_IMETHODIMP
nsCCNxInputStream::Close() {
  return CloseWithStatus(NS_BASE_STREAM_CLOSED);
}

NS_IMETHODIMP
nsCCNxInputStream::Available(PRUint32 *avail) {
  // currently not being used
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCCNxInputStream::Read(char *buf, PRUint32 count, PRUint32 *countRead) {
  int res;

  //  return NS_ERROR_NOT_IMPLEMENTED;
  *countRead = 0;

  struct ccn_fetch_stream *ccnfs;
  {
    MutexAutoLock lock(mTransport->mLock);

    // TODO: how this works?
    if (NS_FAILED(mCondition))
      return (mCondition == NS_BASE_STREAM_CLOSED) ? NS_OK : mCondition;

    ccnfs = mTransport->CCNX_GetLocked();
    if (!ccnfs)
      return NS_BASE_STREAM_CLOSED;
  }

  // Actually reading process
  // ccn_fetch_read is non-blocking, so we have to block it manually
  // however, the bright side is that we don't need to move ccn_fetch_open
  // here. It must be non-blocking as well.
  //  res = ccn_fetch_read(ccnfs, buf, count);

  nsresult rv;
  {
    MutexAutoLock lock(mTransport->mLock);
    while ((res = ccn_fetch_read(ccnfs, buf, count)) != 0) {
      if (res > 0) {
        *countRead += res;
      } else if (res == CCN_FETCH_READ_NONE) {
        if (ccn_run(mTransport->mCCNx, 1000) < 0) {
          res = CCN_FETCH_READ_NONE;
        }
      } else if (res == CCN_FETCH_READ_TIMEOUT) {
        ccn_reset_timeout(ccnfs);
        if (ccn_run(mTransport->mCCNx, 1000) < 0) {
          res = CCN_FETCH_READ_NONE;
          break;
        }
      } else {
        // CCN_FETCH_READ_NONE
        // CCN_FETCH_READ_END
        // and other errors
        break;
      }
    }
    mTransport->CCNX_ReleaseLocked(ccnfs);
  }

  mCondition = ErrorAccordingToCCND(res);
  mByteCount = *countRead;
  rv = mCondition;
  LOG(("nsCCNxInputStream::Read %d", mByteCount));
  return rv;
}

NS_IMETHODIMP
nsCCNxInputStream::ReadSegments(nsWriteSegmentFun writer, void *closure,
                                PRUint32 count, PRUint32 *countRead) {
  LOG(("nsCCNxInputStream::ReadSegments NOT_IMPLEMENTED"));
  // CCNx stream is unbuffered
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCCNxInputStream::IsNonBlocking(bool *nonblocking) {
  *nonblocking = true;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIAsyncInputStream Methods

NS_IMETHODIMP
nsCCNxInputStream::CloseWithStatus(nsresult reason) {
  // minic nsSocketInputStream::CloseWithStatus

  // may be called from any thread
  nsresult rv;
  {
    MutexAutoLock lock(mTransport->mLock);

    if (NS_SUCCEEDED(mCondition))
      rv = mCondition = reason;
    else
      rv = NS_OK;
  }
  //  if (NS_FAILED(rv))
  //    mTransport->OnInputClosed(rv);
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxInputStream::AsyncWait(nsIInputStreamCallback *callback,
                             PRUint32 flags,
                             PRUint32 amount,
                             nsIEventTarget *target) {
  // copied from nsSocketInputStream::AsyncWait
  nsCOMPtr<nsIInputStreamCallback> directCallback;
  {
    MutexAutoLock lock(mTransport->mLock);

    if (callback && target) {
      nsCOMPtr<nsIInputStreamCallback> temp;
      nsresult rv = NS_NewInputStreamReadyEvent(getter_AddRefs(temp),
                                                callback, target);
      if (NS_FAILED(rv)) return rv;
      mCallback = temp;
    }
    else {
      mCallback = callback;
    }
    
    if (NS_FAILED(mCondition))
      directCallback.swap(mCallback);
    else
      mCallbackFlags = flags;
  }
  if (directCallback)
    directCallback->OnInputStreamReady(this);
  else
    return NS_ERROR_NOT_IMPLEMENTED;

  LOG(("nsCCNxInputStream::AsyncWait"));
  return NS_OK;
}

