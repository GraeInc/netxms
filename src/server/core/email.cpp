/* 
** NetXMS - Network Management System
** Copyright (C) 2003-2021 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** File: email.cpp
**
**/

#include "nxcore.h"

#define DEBUG_TAG _T("smtp")

/**
 * Receive buffer size
 */
#define SMTP_BUFFER_SIZE            1024

/**
 * Sender errors
 */
#define SMTP_ERR_SUCCESS            0
#define SMTP_ERR_BAD_SERVER_NAME    1
#define SMTP_ERR_COMM_FAILURE       2
#define SMTP_ERR_PROTOCOL_FAILURE   3

/**
 * Mail sender states
 */
#define STATE_INITIAL      0
#define STATE_HELLO        1
#define STATE_FROM         2
#define STATE_RCPT         3
#define STATE_DATA         4
#define STATE_MAIL_BODY    5
#define STATE_QUIT         6
#define STATE_FINISHED     7
#define STATE_ERROR        8

/**
 * Mail envelope structure
 */
struct MAIL_ENVELOPE
{
   char rcptAddr[MAX_RCPT_ADDR_LEN];
   char subject[MAX_EMAIL_SUBJECT_LEN];
   char *text;
   char encoding[64];
   bool isHtml;
   bool isUtf8;
   int retryCount;
};

/**
 * Static data
 */
static ObjectQueue<MAIL_ENVELOPE> s_mailerQueue(64, Ownership::False);
static THREAD s_mailerThread = INVALID_THREAD_HANDLE;

/**
 * Find end-of-line character
 */
static char *FindEOL(char *pszBuffer, size_t len)
{
   for(size_t i = 0; i < len; i++)
      if (pszBuffer[i] == '\n')
         return &pszBuffer[i];
   return nullptr;
}

/**
 * Read line from socket
 */
static bool ReadLineFromSocket(SOCKET hSocket, char *pszBuffer, size_t *pnBufPos, char *pszLine)
{
   char *ptr;
   do
   {
      ptr = FindEOL(pszBuffer, *pnBufPos);
      if (ptr == nullptr)
      {
         ssize_t bytes = RecvEx(hSocket, &pszBuffer[*pnBufPos], SMTP_BUFFER_SIZE - *pnBufPos, 0, 30000);
         if (bytes <= 0)
            return false;
         *pnBufPos += bytes;
      }
   } while(ptr == nullptr);
   *ptr = 0;
   strcpy(pszLine, pszBuffer);
   *pnBufPos -= (int)(ptr - pszBuffer + 1);
   memmove(pszBuffer, ptr + 1, *pnBufPos);
   return true;
}

/**
 * Read SMTP response code from socket
 */
static int GetSMTPResponse(SOCKET hSocket, char *pszBuffer, size_t *pnBufPos)
{
   char szLine[SMTP_BUFFER_SIZE];

   while(1)
   {
      if (!ReadLineFromSocket(hSocket, pszBuffer, pnBufPos, szLine))
         return -1;
      if (strlen(szLine) < 4)
         return -1;
      if (szLine[3] == ' ')
      {
         szLine[3] = 0;
         break;
      }
   }
   return atoi(szLine);
}

/**
 * Encode SMTP header
 */
static char *EncodeHeader(const char *header, const char *encoding, const char *data, char *buffer, size_t bufferSize)
{
   bool encode = false;
   for(const char *p = data; *p != 0; p++)
      if (*p & 0x80)
      {
         encode = true;
         break;
      }
   if (encode)
   {
      char *encodedData = NULL;
      base64_encode_alloc(data, strlen(data), &encodedData);
      if (encodedData != NULL)
      {
         if (header != NULL)
            snprintf(buffer, bufferSize, "%s: =?%s?B?%s?=\r\n", header, encoding, encodedData);
         else
            snprintf(buffer, bufferSize, "=?%s?B?%s?=", encoding, encodedData);
         free(encodedData);
      }
      else
      {
         // fallback
         if (header != NULL)
            snprintf(buffer, bufferSize, "%s: %s\r\n", header, data);
         else
            strlcpy(buffer, data, bufferSize);
      }
   }
   else
   {
      if (header != NULL)
         snprintf(buffer, bufferSize, "%s: %s\r\n", header, data);
      else
         strlcpy(buffer, data, bufferSize);
   }
   return buffer;
}

/**
 * Send command to server
 */
static inline void SmtpSend(SOCKET hSocket, char *command)
{
   SendEx(hSocket, command, strlen(command), 0, nullptr);
   char *cr = strchr(command, '\r');
   if (cr != nullptr)
      *cr = 0;
   nxlog_debug_tag(DEBUG_TAG, 8, _T("SMTP SEND: %hs"), command);
}

/**
 * Send command to server
 */
static inline void SmtpSend2(SOCKET hSocket, const char *command, const TCHAR *displayText)
{
   SendEx(hSocket, command, strlen(command), 0, nullptr);
   nxlog_debug_tag(DEBUG_TAG, 8, _T("SMTP SEND: %s"), displayText);
}

/**
 * Send e-mail
 */
static UINT32 SendMail(const char *pszRcpt, const char *pszSubject, const char *pszText, const char *encoding, bool isHtml, bool isUtf8)
{
   TCHAR smtpServer[256];
   char fromName[256], fromAddr[256], localHostName[256];
   ConfigReadStr(_T("SMTP.Server"), smtpServer, 256, _T("localhost"));
   ConfigReadStrA(_T("SMTP.FromAddr"), fromAddr, 256, "netxms@localhost");
   if (isUtf8)
   {
      ConfigReadStrUTF8(_T("SMTP.FromName"), fromName, 256, "NetXMS Server");
   }
   else
   {
      ConfigReadStrA(_T("SMTP.FromName"), fromName, 256, "NetXMS Server");
   }
   uint16_t smtpPort = static_cast<uint16_t>(ConfigReadInt(_T("SMTP.Port"), 25));
   ConfigReadStrA(_T("SMTP.LocalHostName"), localHostName, 256, "");
   if (localHostName[0] == 0)
   {
#ifdef UNICODE
      WCHAR localHostNameW[256] = L"";
      GetLocalHostName(localHostNameW, 256, true);
      wchar_to_utf8(localHostNameW, -1, localHostName, 256);
#else
      GetLocalHostName(localHostName, 256, true);
#endif
      if (localHostName[0] == 0)
         strcpy(localHostName, "localhost");
   }

   // Resolve hostname
	InetAddress addr = InetAddress::resolveHostName(smtpServer);
   if (!addr.isValid() || addr.isBroadcast() || addr.isMulticast())
      return SMTP_ERR_BAD_SERVER_NAME;

   // Create socket and connect to server
   SOCKET hSocket = ConnectToHost(addr, smtpPort, 3000);
   if (hSocket == INVALID_SOCKET)
      return SMTP_ERR_COMM_FAILURE;

   char szBuffer[SMTP_BUFFER_SIZE];
   size_t nBufPos = 0;
   int iState = STATE_INITIAL;
   while((iState != STATE_FINISHED) && (iState != STATE_ERROR))
   {
      int iResp = GetSMTPResponse(hSocket, szBuffer, &nBufPos);
      nxlog_debug_tag(DEBUG_TAG, 8, _T("SMTP RESPONSE: %03d (state=%d)"), iResp, iState);
      if (iResp > 0)
      {
         switch(iState)
         {
            case STATE_INITIAL:
               // Server should send 220 text after connect
               if (iResp == 220)
               {
                  iState = STATE_HELLO;
                  char command[280];
                  snprintf(command, 280, "HELO %s\r\n", localHostName);
                  SmtpSend(hSocket, command);
               }
               else
               {
                  iState = STATE_ERROR;
               }
               break;
            case STATE_HELLO:
               // Server should respond with 250 text to our HELO command
               if (iResp == 250)
               {
                  iState = STATE_FROM;
                  snprintf(szBuffer, SMTP_BUFFER_SIZE, "MAIL FROM: <%s>\r\n", fromAddr);
                  SmtpSend(hSocket, szBuffer);
               }
               else
               {
                  iState = STATE_ERROR;
               }
               break;
            case STATE_FROM:
               // Server should respond with 250 text to our MAIL FROM command
               if (iResp == 250)
               {
                  iState = STATE_RCPT;
                  snprintf(szBuffer, SMTP_BUFFER_SIZE, "RCPT TO: <%s>\r\n", pszRcpt);
                  SmtpSend(hSocket, szBuffer);
               }
               else
               {
                  iState = STATE_ERROR;
               }
               break;
            case STATE_RCPT:
               // Server should respond with 250 text to our RCPT TO command
               if (iResp == 250)
               {
                  iState = STATE_DATA;
                  SmtpSend2(hSocket, "DATA\r\n", _T("DATA"));
               }
               else
               {
                  iState = STATE_ERROR;
               }
               break;
            case STATE_DATA:
               // Server should respond with 354 text to our DATA command
               if (iResp == 354)
               {
                  iState = STATE_MAIL_BODY;

                  // Mail headers
                  // from
                  char from[512];
                  snprintf(szBuffer, SMTP_BUFFER_SIZE, "From: \"%s\" <%s>\r\n", EncodeHeader(NULL, encoding, fromName, from, 512), fromAddr);
                  SmtpSend(hSocket, szBuffer);
                  // to
                  snprintf(szBuffer, SMTP_BUFFER_SIZE, "To: <%s>\r\n", pszRcpt);
                  SmtpSend(hSocket, szBuffer);
                  // subject
                  EncodeHeader("Subject", encoding, pszSubject, szBuffer, SMTP_BUFFER_SIZE);
                  SmtpSend(hSocket, szBuffer);

                  // date
                  time_t currentTime;
                  struct tm *pCurrentTM;
                  time(&currentTime);
#ifdef HAVE_LOCALTIME_R
                  struct tm currentTM;
                  localtime_r(&currentTime, &currentTM);
                  pCurrentTM = &currentTM;
#else
                  pCurrentTM = localtime(&currentTime);
#endif
#ifdef _WIN32
                  strftime(szBuffer, sizeof(szBuffer), "Date: %a, %d %b %Y %H:%M:%S ", pCurrentTM);

                  TIME_ZONE_INFORMATION tzi;
                  UINT32 tzType = GetTimeZoneInformation(&tzi);
                  LONG effectiveBias;
                  switch(tzType)
                  {
                     case TIME_ZONE_ID_STANDARD:
                        effectiveBias = tzi.Bias + tzi.StandardBias;
                        break;
                     case TIME_ZONE_ID_DAYLIGHT:
                        effectiveBias = tzi.Bias + tzi.DaylightBias;
                        break;
                     case TIME_ZONE_ID_UNKNOWN:
                        effectiveBias = tzi.Bias;
                        break;
                     default:		// error
                        effectiveBias = 0;
                        nxlog_debug_tag(DEBUG_TAG, 4, _T("GetTimeZoneInformation() call failed"));
                        break;
                  }
                  int offset = abs(effectiveBias);
                  sprintf(&szBuffer[strlen(szBuffer)], "%c%02d%02d\r\n", effectiveBias <= 0 ? '+' : '-', offset / 60, offset % 60);
#else
                  strftime(szBuffer, sizeof(szBuffer), "Date: %a, %d %b %Y %H:%M:%S %z\r\n", pCurrentTM);
#endif

                  SmtpSend(hSocket, szBuffer);
                  // content-type
                  snprintf(szBuffer, SMTP_BUFFER_SIZE,
                                    "Content-Type: text/%s; charset=%s\r\n"
                                    "Content-Transfer-Encoding: 8bit\r\n\r\n", isHtml ? "html" : "plain", encoding);
                  SendEx(hSocket, szBuffer, strlen(szBuffer), 0, NULL);

                  // Mail body
                  SendEx(hSocket, pszText, strlen(pszText), 0, NULL);
                  SendEx(hSocket, "\r\n.\r\n", 5, 0, NULL);
               }
               else
               {
                  iState = STATE_ERROR;
               }
               break;
            case STATE_MAIL_BODY:
               // Server should respond with 250 to our mail body
               if (iResp == 250)
               {
                  iState = STATE_QUIT;
                  SmtpSend2(hSocket, "QUIT\r\n", _T("QUIT"));
               }
               else
               {
                  iState = STATE_ERROR;
               }
               break;
            case STATE_QUIT:
               // Server should respond with 221 text to our QUIT command
               if (iResp == 221)
               {
                  iState = STATE_FINISHED;
               }
               else
               {
                  iState = STATE_ERROR;
               }
               break;
            default:
               iState = STATE_ERROR;
               break;
         }
      }
      else
      {
         iState = STATE_ERROR;
      }
   }

   // Shutdown communication channel
   shutdown(hSocket, SHUT_RDWR);
   closesocket(hSocket);

   return (iState == STATE_FINISHED) ? SMTP_ERR_SUCCESS : SMTP_ERR_PROTOCOL_FAILURE;
}

/**
 * Mailer thread
 */
static THREAD_RESULT THREAD_CALL MailerThread(void *pArg)
{
   static const TCHAR *m_szErrorText[] =
   {
      _T("Sent successfully"),
      _T("Unable to resolve SMTP server name"),
      _T("Communication failure"),
      _T("SMTP conversation failure")
   };

   ThreadSetName("Mailer");
	nxlog_debug_tag(DEBUG_TAG, 1, _T("SMTP mailer thread started"));
   while(1)
   {
      MAIL_ENVELOPE *pEnvelope = s_mailerQueue.getOrBlock();
      if (pEnvelope == INVALID_POINTER_VALUE)
         break;

		nxlog_debug_tag(DEBUG_TAG, 6, _T("SMTP(%p): new envelope, rcpt=%hs"), pEnvelope, pEnvelope->rcptAddr);

      UINT32 dwResult = SendMail(pEnvelope->rcptAddr, pEnvelope->subject, pEnvelope->text, pEnvelope->encoding, pEnvelope->isHtml, pEnvelope->isUtf8);
      if (dwResult != SMTP_ERR_SUCCESS)
		{
			pEnvelope->retryCount--;
			nxlog_debug_tag(DEBUG_TAG, 6, _T("SMTP(%p): Failed to send e-mail, remaining retries: %d"), pEnvelope, pEnvelope->retryCount);
			if (pEnvelope->retryCount > 0)
			{
				// Try posting again
			   s_mailerQueue.put(pEnvelope);
			}
			else
			{
				PostSystemEvent(EVENT_SMTP_FAILURE, g_dwMgmtNode, "dsmm", dwResult,
							 m_szErrorText[dwResult], pEnvelope->rcptAddr, pEnvelope->subject);
				MemFree(pEnvelope->text);
				MemFree(pEnvelope);
			}
		}
		else
		{
			nxlog_debug_tag(DEBUG_TAG, 6, _T("SMTP(%p): mail sent successfully"), pEnvelope);
			MemFree(pEnvelope->text);
			MemFree(pEnvelope);
		}
   }
   return THREAD_OK;
}

/**
 * Initialize mailer subsystem
 */
void InitMailer()
{
   s_mailerThread = ThreadCreateEx(MailerThread, 0, NULL);
}

/**
 * Shutdown mailer
 */
void ShutdownMailer()
{
   s_mailerQueue.clear();
   s_mailerQueue.put(INVALID_POINTER_VALUE);
   ThreadJoin(s_mailerThread);
}

/**
 * Post e-mail to queue
 */
void NXCORE_EXPORTABLE PostMail(const TCHAR *pszRcpt, const TCHAR *pszSubject, const TCHAR *pszText, bool isHtml)
{
   auto envelope = MemAllocStruct<MAIL_ENVELOPE>();
   ConfigReadStrA(_T("MailEncoding"), envelope->encoding, 64, "utf8");
   envelope->isUtf8 = isHtml || !stricmp(envelope->encoding, "utf-8") || !stricmp(envelope->encoding, "utf8");

#ifdef UNICODE
	WideCharToMultiByte(envelope->isUtf8 ? CP_UTF8 : CP_ACP, envelope->isUtf8 ? 0 : WC_DEFAULTCHAR | WC_COMPOSITECHECK, pszRcpt, -1, envelope->rcptAddr, MAX_RCPT_ADDR_LEN, NULL, NULL);
	envelope->rcptAddr[MAX_RCPT_ADDR_LEN - 1] = 0;
	WideCharToMultiByte(envelope->isUtf8 ? CP_UTF8 : CP_ACP, envelope->isUtf8 ? 0 : WC_DEFAULTCHAR | WC_COMPOSITECHECK, pszSubject, -1, envelope->subject, MAX_EMAIL_SUBJECT_LEN, NULL, NULL);
	envelope->subject[MAX_EMAIL_SUBJECT_LEN - 1] = 0;
	envelope->text = envelope->isUtf8 ? UTF8StringFromWideString(pszText) : MBStringFromWideString(pszText);
#else
	if (envelope->isUtf8)
	{
	   mb_to_utf8(pszRcpt, -1, envelope->rcptAddr, MAX_RCPT_ADDR_LEN);
	   envelope->rcptAddr[MAX_RCPT_ADDR_LEN - 1] = 0;
      mb_to_utf8(pszSubject, -1, envelope->subject, MAX_EMAIL_SUBJECT_LEN);
      envelope->subject[MAX_EMAIL_SUBJECT_LEN - 1] = 0;
	   envelope->text = UTF8StringFromMBString(pszText);
	}
	else
	{
      nx_strncpy(envelope->rcptAddr, pszRcpt, MAX_RCPT_ADDR_LEN);
      nx_strncpy(envelope->subject, pszSubject, MAX_EMAIL_SUBJECT_LEN);
      envelope->text = strdup(pszText);
	}
#endif
	envelope->retryCount = ConfigReadInt(_T("SMTP.RetryCount"), 1);
	envelope->isHtml = isHtml;
	s_mailerQueue.put(envelope);
}
