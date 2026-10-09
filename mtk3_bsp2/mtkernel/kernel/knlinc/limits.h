/*
 *----------------------------------------------------------------------
 *    micro T-Kernel 3.00.00
 *
 *    Copyright (C) 2006-2019 by Ken Sakamura.
 *    This software is distributed under the T-License 2.1.
 *----------------------------------------------------------------------
 *
 *    Released by TRON Forum(http://www.tron.org) at 2019/12/11.
 *
 *----------------------------------------------------------------------
 */

/*
 *	limits.h
 *
 *	C language: integer size 
 */

#ifndef _LIMITS_
#define _LIMITS_


#define CHAR_BIT	(8)
#define SCHAR_MIN	(-128)
#define SCHAR_MAX	(+127)
#define UCHAR_MAX	(255)
#define CHAR_MIN	SCHAR_MIN
#define CHAR_MAX	SCHAR_MAX
#define MB_LEN_MAX	(2)

#define SHRT_MIN	(-32768)
#define SHRT_MAX	(+32767)
#define USHRT_MAX	(65535)

#define LONG_MIN	(-2147483648L)
#define LONG_MAX	(+2147483647L)
#define ULONG_MAX	(4294967295L)

#ifndef LLONG_MIN
#define LLONG_MIN	(-9223372036854775807LL - 1LL)
#endif
#ifndef LLONG_MAX
#define LLONG_MAX	9223372036854775807LL
#endif
#ifndef ULLONG_MAX
#define ULLONG_MAX	18446744073709551615ULL
#endif


#if INT_BITWIDTH == 16

#define INT_MIN		SHRT_MIN
#define INT_MAX		SHRT_MAX
#define UINT_MAX	USHRT_MAX

#else /* 32bit */

#define INT_MIN		LONG_MIN
#define INT_MAX		LONG_MAX
#define UINT_MAX	ULONG_MAX

#endif /* INT_BITWIDTH == 16 */
#endif /* _LIMITS_ */
