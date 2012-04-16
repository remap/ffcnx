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

#include "nsCCNxTransportService.h"

using namespace mozilla;

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gCCNxLog;
#endif
#define LOG(args)         PR_LOG(gCCNxLog, PR_LOG_DEBUG, args)

NS_IMPL_THREADSAFE_ISUPPORTS2(nsCCNxTransportService,
                              nsIEventTarget,
                              nsIRunnable)

/*
NS_IMPL_THREADSAFE_ADDREF(nsCCNxTransportService)
NS_IMPL_THREADSAFE_RELEASE(nsCCNxTransportService)

NS_INTERFACE_MAP_BEGIN(nsCCNxTransportService)
  NS_INTERFACE_MAP_ENTRY(nsIEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports)
NS_INTERFACE_MAP_END_THREADSAFE;
*/

//-----------------------------------------------------------------------------
// nsIEVentTarget Methods

nsCCNxTransportService::nsCCNxTransportService()
    : mLock("nsCCNxTransportService.mLock")
    , mInitialized(false)
    , mShuttingDown(false) {
  LOG(("nsCCNxTransportService created @%p\n", this));
}

nsCCNxTransportService::~nsCCNxTransportService() {
  LOG(("nsCCNxTransportService destroyed @%p\n", this));
}

NS_IMETHODIMP
nsCCNxTransportService::Dispatch(nsIRunnable *event, PRUint32 flags) {
  LOG(("nsCCNxTransportService dispatch [%p]\n", event));

  nsCOMPtr<nsIThread> thread = GetThreadSafely();
  NS_ENSURE_TRUE(thread, NS_ERROR_NOT_INITIALIZED);
  nsresult rv = thread->Dispatch(event, flags);
  if (rv == NS_ERROR_UNEXPECTED) {
    // Thread is no longer accepting events. We must have just shut it
    // down on the main thread. Pretend we never saw it.
    rv = NS_ERROR_NOT_INITIALIZED;
  }
  return rv;
}

NS_IMETHODIMP
nsCCNxTransportService::IsOnCurrentThread(bool *result) {
  nsCOMPtr<nsIThread> thread = GetThreadSafely();
  NS_ENSURE_TRUE(thread, NS_ERROR_NOT_INITIALIZED);
  return thread->IsOnCurrentThread(result);
}

NS_IMETHODIMP
nsCCNxTransportService::Run() {
  LOG(("nsCCNxTransportService @%p, start running\n", this));
  // Add self reference
  nsIThread *thread = NS_GetCurrentThread();

  for (;;) {
    bool pendingEvents = false;
    bool shuttingSignal = false;
    thread->HasPendingEvents(&pendingEvents);

    do {
      // If there are pending events for this thread then
      // DoPollIteration() should service the network without blocking.
      //DoPollIteration(!pendingEvents);
            
      // If nothing was pending before the poll, it might be now
      if (!pendingEvents)
        thread->HasPendingEvents(&pendingEvents);

      if (pendingEvents) {
        NS_ProcessNextEvent(thread);
        pendingEvents = false;
        thread->HasPendingEvents(&pendingEvents);
      }
    } while (pendingEvents);

    // now that our event queue is empty, check to see if we should exit
    mLock.Lock();
    shuttingSignal = mShuttingDown;
    mLock.Unlock();
    if (shuttingSignal) {
      LOG(("nsCCNxTransportService @%p, is shutting down\n", this));
      break;
    }
  }
  LOG(("nsCCNxTransportService @%p, stop running\n", this));
  return NS_OK;
}

//-----------------------------------------------------------------------------
// public Methods
NS_IMETHODIMP
nsCCNxTransportService::Init() {
  if (!NS_IsMainThread()) {
    NS_ERROR("wrong thread");
    return NS_ERROR_UNEXPECTED;
  }

  if (mInitialized)
    return NS_OK;

  if (mShuttingDown)
    return NS_ERROR_UNEXPECTED;

  //  nsCOMPtr<nsIThread> thread;
  //  if (NS_FAILED(rv)) return rv;
    
  nsresult rv;
  {
    MutexAutoLock lock(mLock);
    // Install our mThread, protecting against concurrent readers
    rv = NS_NewThread(getter_AddRefs(mThread), this);
    if (NS_FAILED(rv)) return rv;
  }

  mInitialized = true;
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxTransportService::Shutdown() {
  // called from the Main thread
  NS_ENSURE_STATE(NS_IsMainThread());
  if (!mInitialized)
    return NS_OK;

  if (mShuttingDown)
    return NS_ERROR_UNEXPECTED;
  
  {
    MutexAutoLock lock(mLock);

    // signal the socket thread to shutdown
    mShuttingDown = true;
  }

  // join with thread
  //  mThread->Shutdown();
  mThread = nsnull;
  
  LOG(("nsCCNxTransportService @%p, main thread shut me down\n", this));
  mInitialized = false;
  mShuttingDown = false;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// private Methods

already_AddRefed<nsIThread>
nsCCNxTransportService::GetThreadSafely() {
  nsIThread* result;
  {  
    MutexAutoLock lock(mLock);
    result = mThread;
    NS_IF_ADDREF(result);
  }
  return result;
}
