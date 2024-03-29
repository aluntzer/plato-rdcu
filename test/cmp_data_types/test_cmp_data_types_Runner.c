/* AUTOGENERATED FILE. DO NOT EDIT. */

/*=======Automagically Detected Files To Include=====*/
#include "unity.h"
#include <stdint.h>
#include <cmp_data_types.h>

/*=======External Functions This Runner Calls=====*/
extern void setUp(void);
extern void tearDown(void);
extern void test_size_of_a_sample(void);
extern void test_cmp_cal_size_of_data(void);
extern void test_cmp_input_size_to_samples(void);
extern void test_cmp_input_big_to_cpu_endianness(void);
extern void test_cmp_input_big_to_cpu_endianness_error_cases(void);


/*=======Mock Management=====*/
static void CMock_Init(void)
{
}
static void CMock_Verify(void)
{
}
static void CMock_Destroy(void)
{
}

/*=======Setup (stub)=====*/
void setUp(void) {}

/*=======Teardown (stub)=====*/
void tearDown(void) {}

/*=======Test Reset Options=====*/
void resetTest(void);
void resetTest(void)
{
  tearDown();
  CMock_Verify();
  CMock_Destroy();
  CMock_Init();
  setUp();
}
void verifyTest(void);
void verifyTest(void)
{
  CMock_Verify();
}

/*=======Test Runner Used To Run Each Test=====*/
static void run_test(UnityTestFunction func, const char* name, UNITY_LINE_TYPE line_num)
{
    Unity.CurrentTestName = name;
    Unity.CurrentTestLineNumber = line_num;
#ifdef UNITY_USE_COMMAND_LINE_ARGS
    if (!UnityTestMatches())
        return;
#endif
    Unity.NumberOfTests++;
    UNITY_CLR_DETAILS();
    UNITY_EXEC_TIME_START();
    CMock_Init();
    if (TEST_PROTECT())
    {
        setUp();
        func();
    }
    if (TEST_PROTECT())
    {
        tearDown();
        CMock_Verify();
    }
    CMock_Destroy();
    UNITY_EXEC_TIME_STOP();
    UnityConcludeTest();
}

/*=======MAIN=====*/
int main(void)
{
  UnityBegin("../test/cmp_data_types/test_cmp_data_types.c");
  run_test(test_size_of_a_sample, "test_size_of_a_sample", 30);
  run_test(test_cmp_cal_size_of_data, "test_cmp_cal_size_of_data", 70);
  run_test(test_cmp_input_size_to_samples, "test_cmp_input_size_to_samples", 104);
  run_test(test_cmp_input_big_to_cpu_endianness, "test_cmp_input_big_to_cpu_endianness", 170);
  run_test(test_cmp_input_big_to_cpu_endianness_error_cases, "test_cmp_input_big_to_cpu_endianness_error_cases", 497);

  return UnityEnd();
}
