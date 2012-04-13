#include "nsCCNxCore.h"
#include "nsCCNxChannel.h"
#include "nsCCNxTransport.h"

#include "nsIOService.h"
#include "nsIURL.h"
#include "nsStreamUtils.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gCCNxLog;
#endif
#define LOG(args)         PR_LOG(gCCNxLog, PR_LOG_DEBUG, args)

// nsBaseContentStream::nsISupports

NS_IMPL_THREADSAFE_ADDREF(nsCCNxCore)
NS_IMPL_THREADSAFE_RELEASE(nsCCNxCore)

// We only support nsIAsyncInputStream when we are in non-blocking mode.
NS_INTERFACE_MAP_BEGIN(nsCCNxCore)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStreamCallback)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAsyncInputStream, mNonBlocking)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END_THREADSAFE;

nsCCNxCore::nsCCNxCore()
    : mChannel(nsnull)
    , mDataTransport(nsnull)
    , mDataStream(nsnull)
    , mState(CCNX_INIT)
    , mStatus(NS_OK)
    , mNonBlocking(true)
    , mCallback(nsnull)
    , mCallbackTarget(nsnull) {
  LOG(("nsCCNxCore created @%p", this));
}

nsCCNxCore::~nsCCNxCore() {
  /*
  if (mDataTransport != nsnull)
    NS_RELEASE(mDataTransport);
  mDataTransport = nsnull;
  */
  mDataTransport = nsnull;
  LOG(("nsCCNxCore destroyed @%p", this));
}

nsresult
nsCCNxCore::Init(nsCCNxChannel *channel) {
  mChannel = channel;

  //  nsCOMPtr<nsIURL> url, URL should be parsed here
  //  URI can be access by mChannel->URI()

  nsresult rv;
  nsCOMPtr<nsIURL> url = do_QueryInterface(mChannel->URI());
  rv = url->GetAsciiSpec(mInterest);
  // XXX temopry walkaround when nsCCNxURL is absent
  mInterest.Trim("ccnx:", true, false, false);
  if (NS_FAILED(rv))
    return rv;

  return NS_OK;
}

//-----------------------------------------------------------------------------

void
nsCCNxCore::DispatchCallback(bool async)
{
  if (!mCallback)
    return;

  // It's important to clear mCallback and mCallbackTarget up-front because the
  // OnInputStreamReady implementation may call our AsyncWait method.
  nsCOMPtr<nsIInputStreamCallback> callback;
  if (async) {
    NS_NewInputStreamReadyEvent(getter_AddRefs(callback), mCallback,
                                mCallbackTarget);
    if (!callback)
      return;  // out of memory!
    mCallback = nsnull;
  }
  else {
    callback.swap(mCallback);
  }
  mCallbackTarget = nsnull;

  callback->OnInputStreamReady(this);
}

void
nsCCNxCore::OnCallbackPending() {
    // If this is the first call, then see if we could use the cache.  If we
    // aren't going to read from (or write to) the cache, then just proceed to
    // connect to the server.
    if (mState == CCNX_INIT) {
      // internally call OpenInputStream and refer it as mDataStream
      // mDataStream is actually created by NS_NewPipe2
      mState = Connect();
    }
    if (mState == CCNX_CONNECT && mDataStream) {
      // this is a nsPipeOutputStream
      mDataStream->AsyncWait(this, 0, 0, CallbackTarget());
    }
}


CCNX_STATE
nsCCNxCore::Connect() {
  // create the CCNx transport
  nsCOMPtr<nsITransport> ntrans;
  nsresult rv;
  rv = CreateTransport(getter_AddRefs(ntrans));
  if (NS_FAILED(rv))
    return CCNX_ERROR;
  mDataTransport = ntrans;
  // we are reading from the ndn
  nsCOMPtr<nsIInputStream> input;
  rv = mDataTransport->OpenInputStream(0,
                                       nsIOService::gDefaultSegmentSize,
                                       nsIOService::gDefaultSegmentCount,
                                       getter_AddRefs(input));
  NS_ENSURE_SUCCESS(rv, CCNX_ERROR);
  mDataStream = do_QueryInterface(input);
  return CCNX_CONNECT;
}

NS_IMETHODIMP
nsCCNxCore::CreateTransport(nsITransport **result) {
  nsCCNxTransport *ntrans = new nsCCNxTransport();
  if (!ntrans)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(ntrans);

  nsresult rv = ntrans->Init(mInterest.get());
  if (NS_FAILED(rv)) {
    NS_RELEASE(ntrans);
    return rv;
  }

  *result = ntrans;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIInputStream Methods

NS_IMETHODIMP
nsCCNxCore::Close() {
  return IsClosed() ? NS_OK : CloseWithStatus(NS_BASE_STREAM_CLOSED);
}

NS_IMETHODIMP
nsCCNxCore::Available(PRUint32 *avail) {
  // from nsFtpState
  if (mDataStream)
    return mDataStream->Available(avail);

  // from nsBaseContentStream
  *avail = 0;
  return mStatus;
}

NS_IMETHODIMP
nsCCNxCore::Read(char *buf, PRUint32 count, PRUint32 *countRead) {
  return ReadSegments(NS_CopySegmentToBuffer, buf, count, countRead);
}

NS_IMETHODIMP
nsCCNxCore::ReadSegments(nsWriteSegmentFun writer, void *closure,
                         PRUint32 count, PRUint32 *countRead) {

  // from nsFtpState
  if (mDataStream) {
    nsWriteSegmentThunk thunk = { this, writer, closure };
    return mDataStream->ReadSegments(NS_WriteSegmentThunk, &thunk,
                                     count, countRead);
  }

  // from nsBaseContentStream
  *countRead = 0;

  if (mStatus == NS_BASE_STREAM_CLOSED)
    return NS_OK;

  // No data yet
  if (!IsClosed() && IsNonBlocking())
    return NS_BASE_STREAM_WOULD_BLOCK;

  return mStatus;
}

NS_IMETHODIMP
nsCCNxCore::IsNonBlocking(bool *nonblocking) {
  *nonblocking = true;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIAsyncInputStream Methods

NS_IMETHODIMP
nsCCNxCore::CloseWithStatus(nsresult reason) {
  // from nsFtpState

  if (mDataTransport) {
    // Shutdown the data transport.
    mDataTransport->Close(NS_ERROR_ABORT);
    mDataTransport = nsnull;
  }

  mDataStream = nsnull;

  // from nsBaseContentStream
  if (IsClosed())
    return NS_OK;

  NS_ENSURE_ARG(NS_FAILED(reason));
  mStatus = reason;

  DispatchCallbackAsync();
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxCore::AsyncWait(nsIInputStreamCallback *callback,
                      PRUint32 flags,
                      PRUint32 amount,
                      nsIEventTarget *target) {
  // Our _only_ consumer is nsInputStreamPump, so we simplify things here by
  // making assumptions about how we will be called.
  mCallback = callback;
  mCallbackTarget = target;

  if (!mCallback)
    return NS_OK;

  if (IsClosed()) {
    DispatchCallbackAsync();
    return NS_OK;
  }

  OnCallbackPending();
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIInputStreamCallback Methods

NS_IMETHODIMP
nsCCNxCore::OnInputStreamReady(nsIAsyncInputStream *aInStream) {
    // We are receiving a notification from our data stream, so just forward it
    // on to our stream callback.
  if (HasPendingCallback())
    DispatchCallbackSync();

  return NS_OK;
}

