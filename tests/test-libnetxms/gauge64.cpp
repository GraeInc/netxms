#include <nms_common.h>
#include <math.h>
#include <gauge_helpers.h>
#include <testtools.h>

#define SAMPLE1_LEN 6
static int64_t sample1[SAMPLE1_LEN] = { 100, 105, 20, 1700, 190, 300 };

#define SAMPLE2_LEN 6
static int64_t sample2[SAMPLE2_LEN] = { 1, 15, 2, 35, 20, 1 };

#define SAMPLE3_LEN 7
static int64_t sample3[SAMPLE3_LEN] = { 1, 1, 2, 2, 2, 1,2 };

void TestGauge64()
{
   StartTest(_T("Gauge64"));
   ManualGauge64 gauge(_T("Test"), 5, 10);
   for (int i = 0; i < SAMPLE1_LEN; i++)
   {
      gauge.update(sample1[i]);
   }
   AssertEquals(1700, gauge.getMax());
   AssertEquals(20, gauge.getMin());
   AssertEquals(sample1[SAMPLE1_LEN - 1], gauge.getCurrent());
   AssertEquals(42501855, (uint64_t)(gauge.getAverage()*100000));

   ManualGauge64 gauge2(_T("Test"), 5, 20);
   for (int i = 0; i < SAMPLE2_LEN; i++)
   {
      gauge2.update(sample2[i]);
   }
   AssertEquals(35, gauge2.getMax());
   AssertEquals(1, gauge2.getMin());
   AssertEquals(sample2[SAMPLE2_LEN - 1], gauge2.getCurrent());
   AssertEquals(1008691, (uint64_t)(gauge2.getAverage()*100000));

   ManualGauge64 gauge3(_T("Test"), 5, 20);
   for (int i = 0; i < SAMPLE3_LEN; i++)
   {
      gauge3.update(sample3[i]);
   }
   AssertEquals(2, gauge3.getMax());
   AssertEquals(1, gauge3.getMin());
   AssertEquals(sample3[SAMPLE3_LEN - 1], gauge3.getCurrent());
   AssertEquals(154150, (uint64_t)(gauge3.getAverage()*100000));

   EndTest();
}
