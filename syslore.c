/*
Debug: -f-
Release: -f- -O

"CPU ID" can mean 3 things:
- 8080 to i486: Quirks in the processor operation
- 80386 and i486: DX at reset
- Later i486 models, and Pentium and later: CPUID instruction

CPU	Model	Desc
8086	N/A	FLAGS[15:12] always set and cannot be cleared
80286	N/A	FLAGS[15:12] always cleared and cannot be set
80386	N/A	EFLAGS[AC] cannot be toggled
i486SX	N/A	No FPU, No CPUID
i486DX	N/A	Has FPU, No CPUID, later models do CPUID
i486SL	N/A	Has FPU, has CPUID
*/

#include <errno.h>
#include <string.h>
#include <dos.h>
#include <stdio.h>

enum	/* CPU Type */
{
	CPU_UNKNOWN,
	CPU_8086,
	CPU_80286,
	CPU_80386,
	CPU_i486SX,
	CPU_i486DX,
	CPU_i486SL,
	CPU_PENTIUM,		/* 586 */
	CPU_PENTIUM_PRO,	/* 686 */
	CPU_PENTIUM_II,
	CPU_PENTIUM_III,
};

enum	/* FPU Type */
{
	FPU_NONE,
	FPU_8087,
	FPU_80287,
	FPU_80387,
	FPU_x87		/* 80486 and later */
};

struct	dosdate_t
{
	unsigned short year;
	unsigned char  month, day;
};

struct	dostime_t
{
	unsigned char hour, minute, second, frac;
};

struct	regs_t
{
	unsigned short highEAX;
	unsigned short AX;
	unsigned short highEBX;
	unsigned short BX;
	unsigned short highECX;
	unsigned short CX;
	unsigned short highEDX;
	unsigned short DX;
};

/*struct	leaf0_t
{
	unsigned short highEAX;
	unsigned short AX;
	char vendor[12];
};*/

void cpuid(struct regs_t *regs, int level)
{
	asm mov di,regs	;
	asm DB 0x31,0xc0	; /* XOR EAX,EAX */
	asm DB 0x31,0xc9	; /* XOR ECX,ECX */
	asm mov ax,level	; 
	asm DB 0x0f,0xa2	; /* CPUID */
	/* Move EAX */
	asm mov word ptr [di+2],ax	; /* To lower EAX */
	asm DB 0x66,0xc1,0xe8,0x10	; /* SHR EAX,16 */
	asm mov word ptr [di],ax	; /* To upper EAX */
	/* Move EBX */
	asm mov word ptr [di+6],bx	; /* To lower EBX */
	asm DB 0x66,0xc1,0xeb,0x10	; /* SHR EBX,16 */
	asm mov word ptr [di+4],bx	; /* To upper EBX */
	/* Move ECX */
	asm mov word ptr [di+10],cx	; /* To lower ECX */
	asm DB 0x66,0xc1,0xe9,0x10	; /* SHR ECX,16 */
	asm mov word ptr [di+8],cx	; /* To upper ECX */
	/* Move EDX */
	asm mov word ptr [di+14],dx	; /* To lower EDX */
	asm DB 0x66,0xc1,0xea,0x10	; /* SHR EDX,16 */
	asm mov word ptr [di+12],dx	; /* To upper EDX */
}

int hasfpu()
{
	asm fninit;
	asm DB 0xdf,0xe0;	/* FNSTSW AX */
	return _AL == 0;
}

int hascpuid()
{
	asm DB 0x66,0x68,0,0,0x20,0	; /* PUSH 0x20_0000 */
	asm DB 0x66,0x9d		; /* POPFD */
	asm DB 0x66,0x9c		; /* PUSHFD */
	asm DB 0x66,0x58		; /* POP EAX */
	asm DB 0x66,0xc1,0xe8,0x15	; /* SHR EAX,21 */
	return _AL;
}

/**
 * Detect pre-CPUID processor, snippet taken out of AP-485.
 */
int cputype()
{
	/* 8086 has FLAGS[15:12] always set and cannot be cleared */
	/* 8086 also changes value of SP after pushing */
	asm pushf	; /* Push original FLAGS */
	asm pop ax	; /* Get FLAGS into AX */
	asm mov cx,ax	; /* Save original FLAGS */
	asm and ax,0xfff	; /* Clear bits 15:12 */
	asm push ax	; /* Push new value into stack*/
	asm popf	; /* Set FLAGS from value from stack */
	asm pushf	; /* Get new FLAGS */
	asm pop ax	; /* Save FLAGS value */
	asm and ax,0xf000	; /* Keep bits 15:12 */
	asm cmp ax,0xf000	; /* Anything in FLAGS[15:12]? */
	asm jne check_286	; /* If cleared, it's probably an i286 */
	asm push sp	; /* Not an i286? Check for PUSH/SP */
	asm pop dx	; /* Save SP value */
	asm cmp dx,sp	; /* If current SP value */
	asm jne case_8086	; 
	/*asm jmp case_unknown	;*/
check_286: /* 80286 has FLAGS[15:12] always cleared and cannot be set */
	asm or cx,0xf000	; /* Try to set bits 15:12 to saved FLAGS */
	asm push cx	;
	asm popf	; /* Sets FLAGS from CX */
	asm pushf	;
	asm pop dx	; /* Get new FLAGS into DX */
	asm and dx,0xf000	; /* Clear FLAGS[11:0] */
	asm jz case_286	; /* Jump if FLAGS[15:12] is cleared */
check_386: /* 80386 doesn't have toggable EFLAGS[AC], 80486 does */
	asm DB 0x66,0x9c	; /* PUSHFD: Push original EFLAGS */
	asm DB 0x66,0x58	; /* POP EAX: Get ELFAGS */
	asm DB 0x66,0x89,0xc1	; /* MOV ECX,EAX: Save EFLAGS */
	asm DB 0x66,0x35,0,0,4,0; /* XOR EAX,0x40000: Flip EFLAGS[AC] */
	asm DB 0x66,0x50	; /* PUSH EAX: Save new value on stack */
	asm DB 0x66,0x9d	; /* POPFD: Replace EFLAGS */
	asm DB 0x66,0x9c	; /* PUSHFD: Get new EFLAGS */
	asm DB 0x66,0x58	; /* POP EAX: Store EFLAGS */
	asm DB 0x66,0x31,0xc8	; /* XOR EAX,ECX: If can't toggle AC, it is 80386 */
	asm jz case_386	;
	asm DB 0x66,0x51	; /* PUSH ECX */
	asm DB 0x66,0x9d	; /* POPFD: Restore EFLAGS */

case_486:
	return CPU_i486SX;
case_386:
	return CPU_80386;
case_286:
	return CPU_80286;
case_8086:
	return CPU_8086;
case_unknown:
	return CPU_UNKNOWN;
}

const char* cpustr(int type)
{
	switch (type) {
	case CPU_8086:	return "8086";
	case CPU_80286:	return "80286";
	case CPU_80386:	return "80386";
	case CPU_i486SX:	return "i486SX";
	case CPU_i486DX:	return "i486DX";
	case CPU_i486SL:	return "i486SL";
	case CPU_PENTIUM:	return "Pentium";
	case CPU_PENTIUM_II:	return "Pentium II";
	default:	return "Unknown";
	}
}

int fputype()
{
	static unsigned short _fpu_cw;
	
	asm fninit	; /* Resets FPU if present */
	asm DB 0xdf,0xe0	; /* FNSTSW AX: Get FPU status word */
	asm cmp	al,0	; /* Do we have anything? */
	asm jne	case_none	; /* No FPU then */
	/* Check for 8087 */
	asm and	ax,0xff7f	; /* Clear other bits from AX */
	asm mov	word [_fpu_cw],ax	; /* Load AX into mem */
	asm fldcw word [_fpu_cw]	; /* Load control word into FPU */
	asm fdisi	; /* 8087-only instruction */
	asm fstcw word [_fpu_cw]	; /* Get control word */
	asm test word ptr [_fpu_cw],0x80	; /* Did FDISI do anything? */
	asm jne case_8087	; /* FDISI did something, FPU: 8087 */
	/* Check for +INF/-INF between 80287/80387 */
	asm fld1		; /* Push +1.0, this will be st(0) */
	asm fldz		; /* Push +0.0, this will be st(1) */
	asm fdiv		; /* (fdivp st1,st0) 1.0/0.0 = +INF, then pop, TOP=st0 */
	asm fld		st(0)	; /* Push st0 value (+INF) again into stack (now st1) */
	asm fchs		; /* Toggle sign to st0, making st0 -INF */
	asm fcompp		; /* See if st0/st1 are the same, then pop both */
	asm DB 0xdf,0xe0	; /* FSTSW AX: Get status word */
	asm sahf		; /* Save AH into low FLAGS to see if infinites matched */
	asm jz	case_80287	; /* <= 80287: +inf == -inf */
	
	return FPU_80387;
case_80287:
	return FPU_80287;
case_8087:
	return FPU_8087;
case_none:
	return FPU_NONE;
}

const char* fpustr(int type)
{
	switch (type) {
	case FPU_NONE:	return "none";
	case FPU_8087:	return "8087";
	case FPU_80287:	return "80287";
	case FPU_80387:	return "80387";
	case FPU_x87:	return "included";
	default:	return "unknown";
	}
}

void cpuinfo(int *cpu, int *fpu)
{
	struct regs_t regs;
	unsigned char hasfpu;
	unsigned char family;
	unsigned char model;
	unsigned char stepping;
	
	*cpu = cputype();
	*fpu = fputype();
	
	/* 8086-80386 */
	if (*cpu <= CPU_80386)
	{
		return;
	}
	
	hasfpu = *fpu != FPU_NONE;
	
	/* Early i486SX/DX models */
	if (hascpuid() == 0)
	{
		*cpu = hasfpu ? CPU_i486SX : CPU_i486DX;
		if (hasfpu) *fpu = FPU_x87;
		return;
	}
	
	/* Later i486SX/DX/SL/GX models, Pentium, etc. */
	*fpu = FPU_x87;
	cpuid(&regs, 1);
}

void dosver(unsigned char *major, unsigned char *minor)
{
	asm mov ah, 0x30;
	asm int 0x21;
	*major = _AL;
	*minor = _AH;
}

void dosdate(struct dosdate_t *date)
{
	asm mov ah, 0x2a;
	asm int 0x21;
	date->year  = _CX;
	date->month = _DH;
	date->day   = _DL;
}

void dostime(struct dostime_t *time)
{
	asm mov ah, 0x2c;
	asm int 0x21;
	time->hour   = _CH;
	time->minute = _CL;
	time->second = _DH;
	time->frac   = _DL;
}

static char	*optarg;	/* pointer to argument of current option */
static int	opterr	= 1;	/* allow error message	*/

int getopt(int argc, char *argv[], char *optionS)
{
	static char *letP = NULL;	/* remember next option char's location */
	static int optind = 1;	/* index of which argument is next */
	
	unsigned char ch;
	char *optP;
	
	if (argc > optind)
	{
		if (letP == NULL)
		{
			if ((letP = argv[optind]) == NULL || *(letP++) != '-')
				goto gopEOF;
			
			if (*letP == '-')
			{
				optind++;
				goto gopEOF;
			}
		}
		
		if ((ch = *(letP++)) == 0)
		{
			optind++;
			goto gopEOF;
		}
		
		if (':' == ch  ||  (optP = strchr(optionS, ch)) == NULL)  
			goto gopError;
		
		if (':' == *(++optP))
		{
			optind++;
			if (0 == *letP) {
				if (argc <= optind)  goto  gopError;
				letP = argv[optind++];
			}
			optarg = letP;
			letP = NULL;
		}
		else
		{
			if (0 == *letP)
			{
				optind++;
				letP = NULL;
			}
			optarg = NULL;
		}
		return ch;
	}
	
gopEOF:
	optarg = letP = NULL;
	return EOF;
	
gopError:
	optarg = NULL;
	errno  = EINVAL;
	if (opterr)
	{
		perror("getopt");
		exit(EINVAL);
	}
	
	return '?';
}

int main(int argc, char **argv)
{
	struct regs_t regs;
	int leaf;
	int opt;
	int cpu, fpu;
	
	unsigned char dosmajor, dosminor;
	struct dosdate_t date;
	struct dostime_t time;
	
	while ((opt = getopt(argc, argv, "hvc:")) != EOF)
	{
		switch (opt) {
		case 'c':
			/* TODO: Check CPUID */
			if (sscanf(optarg, "%i", &leaf) != 1)
			{
				fprintf(stderr, "NaN: %s", optarg);
				return 1;
			}
			cpuid(&regs, leaf);
			printf("EAX=%04x%04x EBX=%04x%04x ECX=%04x%04x EDX=%04x%04x\n",
				regs.highEAX, regs.AX,
				regs.highEBX, regs.BX,
				regs.highECX, regs.CX,
				regs.highEDX, regs.DX);
			return 0;
		case 'v': puts("Version 1.0"); return 0;
		case 'h':
		case '?':
			puts(
			"SYSLORE - Real-mode processor information\n"
			"\n"
			"OPTIONS\n"
			"c NUM    Call CPUID with leaf NUM.\n"
			);
			return 0;
		default:
			fprintf(stderr, "what is -%c?\n", opt);
			return 1;
		}
	}
	
	dosver(&dosmajor, &dosminor);
	dosdate(&date);
	dostime(&time);
	cpuinfo(&cpu, &fpu);
	
	printf(
	"DOS VER : %u.%u\n"
	"DOS DATE: %u-%02u-%02u\n"
	"DOS TIME: %2u:%02u:%02u.%02u\n"
	"SYS CPU : %s\n"
	"SYS FPU : %s\n",
	dosmajor, dosminor,
	date.year, date.month, date.day,
	time.hour, time.minute, time.second, time.frac,
	cpustr(cpu),
	fpustr(fpu)
	);
	
	return 0;
}
