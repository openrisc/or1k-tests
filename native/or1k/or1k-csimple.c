/*
 *
 * Test run first, to check the main loop is reached and the exit mechanism
 * functions OK
 *
 */

#include "support.h"

int main()
{
  report(0x8000000d);
  exit(0);
}
