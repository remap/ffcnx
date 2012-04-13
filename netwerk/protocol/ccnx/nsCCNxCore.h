#ifndef nsCCNxCore_h__
#define nsCCNxCore_h__

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIAsyncInputStream.h"
#include "nsIEventTarget.h"
#include "nsITransport.h"

class nsCCNxChannel;
class nsCCNxTransport;

typedef enum _CCNX_STATE {
  CCNX_INIT,
  CCNX_CONNECT,
  CCNX_ERROR,
  CCNX_COMPLETE

} CCNX_STATE;

class nsCCNxCore : public nsIAsyncInputStream,
                  public nsIInputStreamCallback {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  nsCCNxCore();

  nsresult Status() { return mStatus; }
  bool IsNonBlocking() { return mNonBlocking; }
  bool IsClosed() { return NS_FAILED(mStatus); }

  // Called to test if the stream has a pending callback.
  bool HasPendingCallback() { return mCallback != nsnull; }

  // The current dispatch target (may be null) for the pending callback if any.
  nsIEventTarget *CallbackTarget() { return mCallbackTarget; }

  nsresult Init(nsCCNxChannel *channel);
  void DispatchCallback(bool async);
  void DispatchCallbackAsync() { DispatchCallback(true); }
  void DispatchCallbackSync() { DispatchCallback(false); }

protected:
  virtual ~nsCCNxCore();

private:
  // Called from the base stream(nsCCNxCore)'s AsyncWait method when a pending
  // callback is installed on the stream.
  void OnCallbackPending();
  CCNX_STATE Connect();
  NS_IMETHODIMP CreateTransport(nsITransport **result);

private:
  nsRefPtr<nsCCNxChannel>             mChannel;
  nsCString                           mInterest;
  nsCOMPtr<nsITransport>              mDataTransport;
  nsCOMPtr<nsIAsyncInputStream>       mDataStream;
  CCNX_STATE                          mState;
  nsresult                            mStatus;
  bool                                mNonBlocking;
  nsCOMPtr<nsIInputStreamCallback>    mCallback;
  nsCOMPtr<nsIEventTarget>            mCallbackTarget;
};

#endif // nsCCNxCore_h__
