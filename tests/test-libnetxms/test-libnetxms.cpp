#include <nms_common.h>
#include <nms_util.h>
#include <nxqueue.h>
#include <testtools.h>

void TestMsgWaitQueue();
void TestMessageClass();

static char mbText[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
static WCHAR wcText[] = L"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
static char mbTextShort[] = "Lorem ipsum";
static UCS2CHAR ucs2TextShort[] = { 'L', 'o', 'r', 'e', 'm', ' ', 'i', 'p', 's', 'u', 'm', 0 };

/**
 * Test string conversion
 */
static void TestStringConversion()
{
   StartTest(_T("ANSI to UCS-2 conversion"));
   UCS2CHAR ucs2buffer[1024];
   mb_to_ucs2(mbTextShort, -1, ucs2buffer, 1024);
   AssertTrue(!memcmp(ucs2buffer, ucs2TextShort, sizeof(UCS2CHAR) * 12));
   EndTest();

   StartTest(_T("UCS-2 to ANSI conversion"));
   char mbBuffer[1024];
   ucs2_to_mb(ucs2TextShort, -1, mbBuffer, 1024);
   AssertTrue(!strcmp(mbBuffer, mbTextShort));
   EndTest();

   StartTest(_T("ANSI to UCS-2 conversion performance"));
   INT64 start = GetCurrentTimeMs();
   for(int i = 0; i < 10000; i++)
   {
      UCS2CHAR buffer[1024];
      mb_to_ucs2(mbText, -1, buffer, 1024);
   }
   EndTest(GetCurrentTimeMs() - start);

   StartTest(_T("UCS-2 to ANSI conversion performance"));
   mb_to_ucs2(mbText, -1, ucs2buffer, 1024);
   start = GetCurrentTimeMs();
   for(int i = 0; i < 10000; i++)
   {
      char buffer[1024];
      ucs2_to_mb(ucs2buffer, -1, buffer, 1024);
   }
   EndTest(GetCurrentTimeMs() - start);

#ifdef UNICODE_UCS4
   StartTest(_T("ANSI to UCS-4 conversion performance"));
   start = GetCurrentTimeMs();
   for(int i = 0; i < 10000; i++)
   {
      WCHAR buffer[1024];
      MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, mbText, -1, buffer, 1024);
   }
   EndTest(GetCurrentTimeMs() - start);

   StartTest(_T("UCS-4 to ANSI conversion performance"));
   start = GetCurrentTimeMs();
   for(int i = 0; i < 10000; i++)
   {
      char buffer[1024];
      WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, wcText, -1, buffer, 1024, NULL, NULL);
   }
   EndTest(GetCurrentTimeMs() - start);

   StartTest(_T("UCS-2 to UCS-4 conversion performance"));
   mb_to_ucs2(mbText, -1, ucs2buffer, 1024);
   start = GetCurrentTimeMs();
   for(int i = 0; i < 10000; i++)
   {
      WCHAR buffer[1024];
      ucs2_to_ucs4(ucs2buffer, -1, buffer, 1024);
   }
   EndTest(GetCurrentTimeMs() - start);

   StartTest(_T("UCS-4 to UCS-2 conversion performance"));
   start = GetCurrentTimeMs();
   for(int i = 0; i < 10000; i++)
   {
      UCS2CHAR buffer[1024];
      ucs4_to_ucs2(wcText, -1, buffer, 1024);
   }
   EndTest(GetCurrentTimeMs() - start);
#endif
}

/**
 * Test string map
 */
static void TestStringMap()
{
   StringMap *m = new StringMap();

   StartTest(_T("String map - insert"));
   INT64 start = GetCurrentTimeMs();
   for(int i = 0; i < 10000; i++)
   {
      TCHAR key[64];
      _sntprintf(key, 64, _T("key-%d"), i);
      m->set(key, _T("Lorem ipsum dolor sit amet"));
   }
   AssertEquals(m->size(), 10000);
   const TCHAR *v = m->get(_T("key-42"));
   AssertNotNull(v);
   AssertTrue(!_tcscmp(v, _T("Lorem ipsum dolor sit amet")));
   EndTest(GetCurrentTimeMs() - start);

   StartTest(_T("String map - replace"));
   start = GetCurrentTimeMs();
   for(int i = 0; i < 10000; i++)
   {
      TCHAR key[64];
      _sntprintf(key, 64, _T("key-%d"), i);
      m->set(key, _T("consectetur adipiscing elit"));
   }
   AssertEquals(m->size(), 10000);
   v = m->get(_T("key-42"));
   AssertNotNull(v);
   AssertTrue(!_tcscmp(v, _T("consectetur adipiscing elit")));
   EndTest(GetCurrentTimeMs() - start);

   StartTest(_T("String map - get"));
   start = GetCurrentTimeMs();
   v = m->get(_T("key-888"));
   AssertNotNull(v);
   AssertTrue(!_tcscmp(v, _T("consectetur adipiscing elit")));
   EndTest(GetCurrentTimeMs() - start);

   StartTest(_T("String map - clear"));
   start = GetCurrentTimeMs();
   m->clear();
   AssertEquals(m->size(), 0);
   EndTest(GetCurrentTimeMs() - start);

   delete m;
}

/**
 * Test string set
 */
static void TestStringSet()
{
   StringSet *s = new StringSet();

   StartTest(_T("String set - insert"));
   INT64 start = GetCurrentTimeMs();
   for(int i = 0; i < 10000; i++)
   {
      TCHAR key[64];
      _sntprintf(key, 64, _T("key-%d lorem ipsum"), i);
      s->add(key);
   }
   AssertEquals(s->size(), 10000);
   AssertTrue(s->contains(_T("key-42 lorem ipsum")));
   EndTest(GetCurrentTimeMs() - start);

   StartTest(_T("String set - replace"));
   start = GetCurrentTimeMs();
   for(int i = 0; i < 10000; i++)
   {
      TCHAR key[64];
      _sntprintf(key, 64, _T("key-%d lorem ipsum"), i);
      s->add(key);
   }
   AssertEquals(s->size(), 10000);
   AssertTrue(s->contains(_T("key-42 lorem ipsum")));
   EndTest(GetCurrentTimeMs() - start);

   StartTest(_T("String set - contains"));
   start = GetCurrentTimeMs();
   AssertTrue(s->contains(_T("key-888 lorem ipsum")));
   EndTest(GetCurrentTimeMs() - start);

   StartTest(_T("String set - clear"));
   start = GetCurrentTimeMs();
   s->clear();
   AssertEquals(s->size(), 0);
   EndTest(GetCurrentTimeMs() - start);

   delete s;
}

/**
 * Test string class
 */
static void TestString()
{
   String s;

   StartTest(_T("String - append"));
   for(int i = 0; i < 256; i++)
      s.append(_T("ABC "));
   AssertEquals(s.length(), 1024);
   AssertTrue(!_tcsncmp(s.getBuffer(), _T("ABC ABC ABC ABC "), 16));
   EndTest();

   StartTest(_T("String - assign #1"));
   s = _T("alpha");
   AssertEquals(s.length(), 5);
   AssertTrue(!_tcscmp(s.getBuffer(), _T("alpha")));
   EndTest();

   StartTest(_T("String - assign #2"));
   String t(_T("init string"));
   s = t;
   AssertEquals(s.length(), 11);
   AssertTrue(!_tcscmp(s.getBuffer(), _T("init string")));
   EndTest();

   StartTest(_T("String - shrink"));
   s.shrink();
   AssertEquals(s.length(), 10);
   AssertTrue(!_tcscmp(s.getBuffer(), _T("init strin")));
   EndTest();

   StartTest(_T("String - escape"));
   s.escapeCharacter('i', '+');
   AssertEquals(s.length(), 13);
   AssertTrue(!_tcscmp(s.getBuffer(), _T("+in+it str+in")));
   EndTest();

   StartTest(_T("String - replace #1"));
   s = _T("alpha beta gamma");
   s.replace(_T("beta"), _T("epsilon"));
   AssertEquals(s.length(), 19);
   AssertTrue(!_tcscmp(s.getBuffer(), _T("alpha epsilon gamma")));
   EndTest();

   StartTest(_T("String - replace #2"));
   s = _T("alpha beta gamma");
   s.replace(_T("beta"), _T("xxxx"));
   AssertEquals(s.length(), 16);
   AssertTrue(!_tcscmp(s.getBuffer(), _T("alpha xxxx gamma")));
   EndTest();

   StartTest(_T("String - replace #3"));
   s = _T("alpha beta gamma alpha omega");
   s.replace(_T("alpha"), _T("Z"));
   AssertEquals(s.length(), 20);
   AssertTrue(!_tcscmp(s.getBuffer(), _T("Z beta gamma Z omega")));
   EndTest();

   StartTest(_T("String - substring #1"));
   s = _T("alpha beta gamma");
   TCHAR *str = s.substring(0, 5);
   AssertTrue(!_tcscmp(str, _T("alpha")));
   free(str);
   EndTest();

   StartTest(_T("String - substring #2"));
   s = _T("alpha beta gamma");
   str = s.substring(5, -1);
   AssertTrue(!_tcscmp(str, _T(" beta gamma")));
   free(str);
   EndTest();

   StartTest(_T("String - substring #3"));
   s = _T("alpha beta gamma");
   str = s.substring(14, 4);
   AssertTrue(!_tcscmp(str, _T("ma")));
   free(str);
   EndTest();
}

/**
 * Test InetAddress class
 */
static void TestInetAddress()
{
   InetAddress a, b, c;

   StartTest(_T("InetAddress - isSubnetBroadcast() - IPv4"));
   a = InetAddress::parse("192.168.0.255");
   AssertTrue(a.isSubnetBroadcast(24));
   AssertFalse(a.isSubnetBroadcast(23));
   EndTest();

   StartTest(_T("InetAddress - isSubnetBroadcast() - IPv6"));
   a = InetAddress::parse("fe80::ffff:ffff:ffff:ffff");
   AssertFalse(a.isSubnetBroadcast(64));
   AssertFalse(a.isSubnetBroadcast(63));
   EndTest();

   StartTest(_T("InetAddress - isLinkLocal() - IPv4"));
   a = InetAddress::parse("169.254.17.198");
   AssertTrue(a.isLinkLocal());
   a = InetAddress::parse("192.168.1.1");
   AssertFalse(a.isLinkLocal());
   EndTest();

   StartTest(_T("InetAddress - isLinkLocal() - IPv6"));
   a = InetAddress::parse("fe80::1");
   AssertTrue(a.isLinkLocal());
   a = InetAddress::parse("2000:1234::1");
   AssertFalse(a.isLinkLocal());
   EndTest();

   StartTest(_T("InetAddress - sameSubnet() - IPv4"));
   a = InetAddress::parse("192.168.1.43");
   a.setMaskBits(23);
   b = InetAddress::parse("192.168.0.180");
   b.setMaskBits(23);
   c = InetAddress::parse("192.168.2.22");
   c.setMaskBits(23);
   AssertTrue(a.sameSubnet(b));
   AssertFalse(a.sameSubnet(c));
   EndTest();

   StartTest(_T("InetAddress - sameSubnet() - IPv6"));
   a = InetAddress::parse("2000:1234:1000:1000::1");
   a.setMaskBits(62);
   b = InetAddress::parse("2000:1234:1000:1001::cdef:1");
   b.setMaskBits(62);
   c = InetAddress::parse("2000:1234:1000:1007::1");
   c.setMaskBits(62);
   AssertTrue(a.sameSubnet(b));
   AssertFalse(a.sameSubnet(c));
   EndTest();
}

/**
 * Test itoa/itow
 */
static void TestItoa()
{
   char buffer[64];
   WCHAR wbuffer[64];

   StartTest(_T("itoa"));
   AssertTrue(!strcmp(_itoa(127, buffer, 10), "127"));
   AssertTrue(!strcmp(_itoa(0, buffer, 10), "0"));
   AssertTrue(!strcmp(_itoa(-3, buffer, 10), "-3"));
   AssertTrue(!strcmp(_itoa(0555, buffer, 8), "555"));
   AssertTrue(!strcmp(_itoa(0xFA48, buffer, 16), "fa48"));
   EndTest();

   StartTest(_T("itow"));
   AssertTrue(!wcscmp(_itow(127, wbuffer, 10), L"127"));
   AssertTrue(!wcscmp(_itow(0, wbuffer, 10), L"0"));
   AssertTrue(!wcscmp(_itow(-3, wbuffer, 10), L"-3"));
   AssertTrue(!wcscmp(_itow(0555, wbuffer, 8), L"555"));
   AssertTrue(!wcscmp(_itow(0xFA48, wbuffer, 16), L"fa48"));
   EndTest();
}

/**
 * Test queue
 */
static void TestQueue()
{
   Queue *q = new Queue(16, 16);

   StartTest(_T("Queue: put/get"));
   for(int i = 0; i < 40; i++)
      q->put(CAST_TO_POINTER(i + 1, void *));
   AssertEquals(q->size(), 40);
   AssertEquals(q->allocated(), 48);
   for(int i = 0; i < 40; i++)
   {
      void *p = q->get();
      AssertNotNull(p);
      AssertEquals(CAST_FROM_POINTER(p, int), i + 1);
   }
   EndTest();

   StartTest(_T("Queue: shrink"));
   for(int i = 0; i < 60; i++)
      q->put(CAST_TO_POINTER(i + 1, void *));
   AssertEquals(q->size(), 60);
   AssertEquals(q->allocated(), 64);
   for(int i = 0; i < 55; i++)
   {
      void *p = q->get();
      AssertNotNull(p);
      AssertEquals(CAST_FROM_POINTER(p, int), i + 1);
   }
   AssertEquals(q->size(), 5);
   AssertEquals(q->allocated(), 16);
   EndTest();
}

/**
 * main()
 */
int main(int argc, char *argv[])
{
#ifdef _WIN32
   WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

   TestString();
   TestStringConversion();
   TestStringMap();
   TestStringSet();
   TestMessageClass();
   TestMsgWaitQueue();
   TestInetAddress();
   TestItoa();
   TestQueue();
   return 0;
}
