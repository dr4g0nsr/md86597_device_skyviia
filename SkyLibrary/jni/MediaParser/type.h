#ifndef _BASIC_DATA_TYPE_HEADER_
#define _BASIC_DATA_TYPE_HEADER_
//------------------------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

//------------------------------------------------------------------------------------------------

#define TRUE			1
#define FALSE			0
#define true			1
#define false			0

#define LEN_XXL			1024	//for UNICODE file path string
#define LEN_XL			256		//for file path string
#define LEN_L			60		//for screen string width
#define LEN_M			20		//now using for ds name
#define LEN_S			10
#define SET_MAX			120

//------------------------------------------------------------------------------------------------
#define _BIT0			0x00000001
#define _BIT1			0x00000002
#define _BIT2			0x00000004
#define _BIT3			0x00000008
#define _BIT4			0x00000010
#define _BIT5			0x00000020
#define _BIT6			0x00000040
#define _BIT7			0x00000080
#define _BIT8			0x00000100
#define _BIT9			0x00000200
#define _BIT10			0x00000400
#define _BIT11			0x00000800
#define _BIT12			0x00001000
#define _BIT13			0x00002000
#define _BIT14			0x00004000
#define _BIT15			0x00008000
#define _BIT16			0x00010000
#define _BIT17			0x00020000
#define _BIT18			0x00040000
#define _BIT19			0x00080000
#define _BIT20			0x00100000
#define _BIT21			0x00200000
#define _BIT22			0x00400000
#define _BIT23			0x00800000
#define _BIT24			0x01000000
#define _BIT25			0x02000000
#define _BIT26			0x04000000
#define _BIT27			0x08000000
#define _BIT28			0x10000000
#define _BIT29			0x20000000
#define _BIT30			0x40000000
#define _BIT31			0x80000000

//------------------------------------------------------------------------------------------------
typedef signed char		Sint8;
typedef signed short	Sint16;
typedef signed int		Sint32;
typedef signed long		SLint32;
typedef unsigned char	Uint8;
typedef unsigned short	Uint16;
typedef unsigned int	Uint32;
typedef unsigned long	ULint32;

//typedef unsigned char	bool;
typedef unsigned char	BYTE;
typedef unsigned short	WORD;
typedef unsigned long	DWORD;
typedef short           SHORT;
typedef long            LONG;
typedef int             INT;
typedef long long		INT64;
typedef void*			POSITION;
typedef void*			PTR;
typedef long			RESULT;

//------------------------------------------------------------------------------------------------

#endif

