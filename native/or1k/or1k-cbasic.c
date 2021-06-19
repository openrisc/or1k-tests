/* Test basic c functionality, and BSS section clearing.  */
#include <stdlib.h>
#include <string.h>

#include "support.h"

signed long test_cond(int i)
{
	switch(i) {
		case 1:
			i += 1;
			break;
		case -1:
			i -= 10;
			break;
		default:
			return i;
	}

	if (i == 2)		/* normaly i == 2 */
		i += 1;
	else
		i -= 10;

	if (i > 2)		/* normaly i == 3 */
		i += 1;
	else
		i -=10;

	if (i >= 4)		/* normaly i == 4 */
		i += 1;
	else
		i -= 10;

	if (i <= 5)		/* normaly i == 5 */
		i += 1;
	else
		i -= 10;

	if (i < 7)		/* normaly i == 6 */
		i += 1;
	else
		i -= 10;

	if (i != 666)		/* normaly i == 7 */
		i += 1;
	else
		i -= 10;

	return i;		/* with initial i == 1 return 8 */
}

signed long test_loops(int i)
{
	int j = 0;

	for(; i < 10; i++)
		j += 2;

	do {
		i -= 3;
	} while (j--);

	return i;
}

signed long test_arith(int i)
{
	int mul = 0, div = 0;
	int j;

	for(j = i; j < 40; j++) {

		mul += j*j*i;
#if 0
		report(mul);
#endif
		div += mul + (j+5);
#if 0
		report(div);
#endif
	}

	report (mul+div);
	return (mul + div);
}

signed long test_bitop(int i)
{
	int shl = 0, shr = 0, bit = 0;
	int j;

	for(j = i; j < 35; j++) {
		shl += 1 << j;
#if 0
		printf("%u. shl:%.8lx", j, shl);
		report(shl);
#endif
		shr += 0x80000000 >> j;
#if 0
		printf("  shr:%.8lx", shr);
		report(shr);
#endif
		bit += ((~j ^ 0x11223344) & (0x33557788 + j)) | 0x11223344;
#if 0
		printf("  bit:%.8lx\n", bit);
		report(bit);
#endif
	}

	return (shl + shr + bit);
}

signed long test_types(int i)
{
	unsigned char uc;
	signed char sc;
	unsigned short us;
	signed short ss;
	unsigned long ul;
	signed long sl;

	int j;

	i ^= 0x10203040;

	for(j = 0; j < 10; j++) {
		uc = i;
		sc = i;
		us = i;
		ss = i;
		ul = i;
		sl = i;
		i = uc + sc + us + ss + ul + sl;
	}

	return i;
}

signed long test_array(int i)
{
  char a1[] = "This test string MUST NOT be modified...";
  char a2[100];

  report(a1[5]);
  memcpy(a2, a1, 40);
  report(a1[5]);
  report(a2[5]);
  report(i);
  /* register reload test */
  i += a2[0] + a2[1] + a2[2] + a2[3] + a2[4] + a2[5] + a2[6] + a2[7]
    + a2[8] + a2[9] + a2[10] + a2[11] + a2[12] + a2[13] + a2[14] + a2[15]
    + a2[16] + a2[17] + a2[18] + a2[19] + a2[20] + a2[21] + a2[22] + a2[23]
    + a2[24] + a2[25] + a2[26] + a2[27] + a2[28] + a2[29] + a2[30] + a2[31]
    + a2[32] + a2[33] + a2[34] + a2[35] + a2[36] + a2[37] + a2[38] + a2[39];

  report(i);

  return i;
}

int main()
{
	signed long result1 = 0;
	signed long result2 = 0;
	signed long result3 = 0;
	unsigned long final_result;

	result1 = test_cond(1);
	result2 = test_cond(-1);
	result3 -= result1 + result2;
	report(result2);

	result1 = test_loops(1);
	result2 = test_loops(-1);
	result3 -= result1 + result2;
	report(result2);

	result1 = test_arith(1);
	result2 = test_arith(-1);
	result3 -= result1 + result2;
	report(result2);

	result1 = test_bitop(1);
	result2 = test_bitop(-1);
	result3 -= result1 + result2;
	report(result2);

	result1 = test_types(1);
	result2 = test_types(-1);
	result3 -= result1 + result2;
	report(result2);

	result1 = test_array(1);
	result2 = test_array(-1);
	result3 -= result1 + result2;
	report(result2);

	// Check final result
	final_result = (unsigned long)result3;

	// Should give us 0x8000000d (Gooooood)
	final_result += 0x3474E142;

	report(final_result);

	if (final_result == 0x8000000d)
	  return 0;
	else
	  return 1;
}
