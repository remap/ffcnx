/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
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

#include "nsCCNxProtocolHandler.h"
#include "nsCCNxChannel.h"

#include "nsNetUtil.h"
#include "nsIURL.h"
#include "nsNetCID.h"
#include "nsIClassInfoImpl.h"
#include "nsStandardURL.h"
#include "prlog.h"

#include "mozilla/ModuleUtils.h"
#include "nsAutoPtr.h"

#if defined(PR_LOGGING)
//
// Log module for CCNx Protocol logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsCCNx:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
//
PRLogModuleInfo* gCCNxLog = nsnull;
#endif
#undef LOG
#define LOG(args) PR_LOG(gCCNxLog, PR_LOG_DEBUG, args)

//-----------------------------------------------------------------------------

nsCCNxProtocolHandler* gCCNxHandler = nsnull;

NS_IMPL_CLASSINFO(nsCCNxProtocolHandler, NULL, 0, NS_CCNX_HANDLER_CID)
NS_IMPL_ISUPPORTS2(nsCCNxProtocolHandler,
                   nsIProtocolHandler,
                   nsICCNxProtocolHandler);

nsCCNxProtocolHandler::nsCCNxProtocolHandler() {
#if defined(PR_LOGGING)
    if (!gCCNxLog)
        gCCNxLog = PR_NewLogModule("nsCCNx");
#endif
    LOG(("FTP:creating handler @%x\n", this));

  gCCNxHandler = this;
}

nsCCNxProtocolHandler::~nsCCNxProtocolHandler() {
  gCCNxHandler = nsnull;
}

nsresult nsCCNxProtocolHandler::Init() {
  nsresult rv;
  mIOService = do_GetIOService(&rv);
  if (NS_FAILED(rv))
    return rv;
  return NS_OK;
}

NS_IMETHODIMP nsCCNxProtocolHandler::GetScheme(nsACString & result) {
  result.AssignLiteral("ccnx");
  return NS_OK;
}

NS_IMETHODIMP
nsCCNxProtocolHandler::GetDefaultPort(PRInt32 *result) {
  *result = 80;        // no port for res: URLs
  return NS_OK;
}

NS_IMETHODIMP nsCCNxProtocolHandler::GetProtocolFlags(PRUint32 *result) {
  *result = URI_NORELATIVE | URI_NOAUTH | URI_LOADABLE_BY_ANYONE;
  return NS_OK;
}

NS_IMETHODIMP nsCCNxProtocolHandler::AllowPort(PRInt32 port, 
                                               const char * scheme,
                                               bool *_retval NS_OUTPARAM) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCCNxProtocolHandler::NewURI(const nsACString & aSpec,
                                            const char * aCharset,
                                            nsIURI *aBaseURI,
                                            nsIURI * *result) {
  nsresult rv;
  NS_ASSERTION(!aBaseURI, "base url passed into finger protocol handler");
  nsStandardURL* url = new nsStandardURL();
  if (!url)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(url);

  rv = url->Init(nsIStandardURL::URLTYPE_STANDARD, -1, aSpec, 
                 aCharset, aBaseURI);
  if (NS_SUCCEEDED(rv))
    rv = CallQueryInterface(url, result);
  NS_RELEASE(url);

  return rv;
}

NS_IMETHODIMP nsCCNxProtocolHandler::NewChannel(nsIURI *aURI,
                                                nsIChannel * *result) {
  nsresult rv;
  //  nsCAString tag = aURI->spec.split(":")[1];
  /*
  nsCAutoString tag("http://twitter.com/goodcjw");
  nsCOMPtr<nsIURI> uri;
  rv = mIOService->NewURI(tag, nsnull, nsnull,
                          getter_AddRefs(uri));
  rv = mIOService->NewChannel(tag, nsnull, nsnull, result);
  return NS_OK;
  */
  nsRefPtr<nsCCNxChannel> channel;
  channel = new nsCCNxChannel(aURI);

  rv = channel->Init();
  if (NS_FAILED(rv)) {
    return rv;
  }
  channel.forget(result);
  return rv;
}
