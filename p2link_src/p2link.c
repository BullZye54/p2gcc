#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "p2link.h"

#define MAX_NAME_LEN 30
#define MAX_OBJECTS 1000
#define MAX_SYMBOLS 2000

int FindGlobalLimits(char *str, int type, int first, int last);

int addr = 0;
int debugflag = 0;
int start_addr = 0;

char symtype[MAX_SYMBOLS];
int symvalue[MAX_SYMBOLS];
int symoffset[MAX_SYMBOLS];
char *symname[MAX_SYMBOLS];
int numsym = 0;
char bigbuf[MAX_SYMBOLS*32];
char *bufptr = bigbuf;
int objnum = 0;
int varcount = 0;
int verbose = 0;
int objstart[MAX_OBJECTS+1];
char objname[MAX_OBJECTS][MAX_NAME_LEN];

int mem[100000];

void usage(void)
{
    printf("usage: p2link\n");
    printf("         [ -v ]       enable verbose mode\n");
    printf("         [ -d ]       enable debug mode\n");
    printf("         [ -o file ]  output file name (default a.out)\n");
    printf("         [ -s addr ]  set starting address (default 0)\n");
    printf("         files        one or more object and library files\n");
    exit(1);
}

FILE *OpenFile(char *fname, char *mode)
{
    FILE *iofile = fopen(fname, mode);

    if (!iofile)
    {
        printf("Couldn't open %s\n", fname);
        exit(1);
    }

    return iofile;
}

int read_header(FILE *infile, char *objectname)
{
    int addr0;
    char buffer[8];
    unsigned char len;
    int num = fread(buffer, 1, 8, infile);

    if (num == 0) return -1;

    if (num != 8 || strncmp(buffer, "P2OBJECT", 8))
    {
        printf("Invalid P2 object file\n");
        return -1;
    }
    fread(&len, 1, 1, infile);
    if (len > MAX_NAME_LEN)
    {
        fread(objectname, 1, MAX_NAME_LEN, infile);
        objectname[MAX_NAME_LEN-1] = 0;
        printf("ERROR: Object name is too long - %s\n", objectname);
        exit(1);
    }
    else
        fread(objectname, 1, len, infile);
    fread(&addr0, 1, 4, infile);
    if (debugflag) printf("Linking %s\n", objectname);
    return addr0;
}

void ComputeSymbolOffsets(int first, int last, int offset)
{
    int i, index, value1, addr, offset1;

    for (i = first; i < last; i++)
    {
        if (symtype[i] == OTYPE_REF_AUGS || symtype[i] == OTYPE_REF_AUGD || symtype[i] == OTYPE_REF_LONG_REL)
        {
            addr = symvalue[i];
            value1 = mem[addr>>2];
            index = FindGlobalLimits(symname[i], OTYPE_LABEL, first, last);
            if (index < 0)
            {
                printf("Couldn't find %s\n", symname[i]);
                continue;
            }
            if (symtype[i] == OTYPE_REF_AUGS)
            {
                value1 = (value1 & 0x7fffff) << 9;
                value1 |= mem[(addr>>2)+1] & 0x1ff;
            }
            else if (symtype[i] == OTYPE_REF_AUGD)
            {
                value1 = (value1 & 0x7fffff) << 9;
                value1 |= (mem[(addr>>2)+1] >> 9) & 0x1ff;
            }
            offset1 = value1 - (symvalue[index] - offset);
            if (offset1)
            {
                symoffset[i] = offset1;
                if (debugflag || verbose)
                    printf("Found offset of %d for symbol %s of type %c at location %x\n", offset1, symname[i], symtype[i], addr);
            }
        }
    }
}

int ReadObject(FILE *infile)
{
    unsigned char len;
    int num, size;
    int addr0;
    int prev_numsym = numsym;

    if (objnum >= MAX_OBJECTS)
    {
        printf("ERROR: Too many objects\n");
        exit(1);
    }

    addr0 = read_header(infile, objname[objnum]);
    objstart[objnum] = numsym;
    if (addr0 < 0) return 0;

    while (1)
    {
        if (numsym > MAX_SYMBOLS)
        {
            printf("ERROR: Too many symbols\n");
            exit(1);
        }
        if (fread(&symtype[numsym], 1, 1, infile) != 1) return 0;
        if (symtype[numsym] == OTYPE_END_OF_CODE)
        {
            fread(&size, 1, 4, infile);
            break;
        }
        fread(&symvalue[numsym], 1, 4, infile);
        if (addr != start_addr)
            symvalue[numsym] += addr - 0x400;
        else
            symvalue[numsym] += addr;
        fread(&len, 1, 1, infile);
        fread(bufptr, 1, len, infile);
        symname[numsym] = bufptr;
        bufptr += len;
        if (debugflag)
            printf("%d: %c %8.8x %s\n", numsym, symtype[numsym], symvalue[numsym], symname[numsym]);
        numsym++;
    }
    objstart[objnum+1] = numsym;

    if (addr != start_addr)
    {
        if (addr0 == 0)
        {
            fread(&mem[addr>>2], 1, 0x400, infile);
        }
        else if (addr0 != 0x400)
        {
            printf("ERROR: Object must start at address 0 or 0x400\n");
            exit(1);
        }
        size -= 0x400;
    }
    else if (addr0)
    {
        printf("ERROR: First object must start at address 0\n");
        exit(1);
    }
    num = fread(&mem[addr>>2], 1, size, infile);
    if (num != size)
    {
        printf("ERROR: Expected %d bytes, but only read %d bytes\n", size, num);
    }
    if (addr != start_addr) ComputeSymbolOffsets(prev_numsym, numsym, addr - 0x400);
    addr += num;
    objnum++;
    return 1;
}

int FindSymbol(char *str, int type0)
{
    int i;

    for (i = 0; i < numsym; i++)
    {
        if (symtype[i] == type0 && !strcmp(str, symname[i])) return i;
    }

    return -1;
}

int FindGlobal(char *str)
{
    int i;

    for (i = 0; i < numsym; i++)
    {
        if (symtype[i] == OTYPE_GLOBAL_FUNC && !strcmp(str, symname[i])) return i;
    }

    return -1;
}

int FindVariable(char *str)
{
    int i;

    for (i = 0; i < numsym; i++)
    {
        if (symtype[i] == OTYPE_LABEL && !strcmp(str, symname[i])) return i;
    }

    return -1;
}

int FindGlobalLimits(char *str, int type, int first, int last)
{
    int i;

    for (i = first; i < last; i++)
    {
        if (symtype[i] == type && !strcmp(str, symname[i])) return i;
    }

    return -1;
}

int Resolve(int num)
{
    int i, j;
    int resolved = 0;
    int addr_u, addr_g, value_u, value_r;

    for (i = 0; i < num; i++)
    {
        if (symtype[i] == OTYPE_REF_FUNC_UND || symtype[i] == OTYPE_REF_LONG_UND)
        {
            if (debugflag)
                printf("Resolving %s at %8.8x\n", symname[i], symvalue[i]);
            j = FindGlobal(symname[i]);
            if (j >= 0)
            {
                if (debugflag)
                    printf("Global %s located at %8.8x\n", symname[j], symvalue[j]);
                addr_u = symvalue[i];
                addr_g = symvalue[j];
                value_u = mem[addr_u>>2];
                if (symtype[i] == OTYPE_REF_FUNC_UND)
                    value_r = (value_u & ~0xfffff) | ((addr_g - addr_u - 4) & 0xfffff);
                else
                    value_r = (addr_g & 0xfffff);
                mem[addr_u>>2] = value_r;
                symtype[i] = OTYPE_REF_LONG_REL;
                resolved++;
            }
        }
    }
    return resolved;
}

void CountVariables(void)
{
    int i;

    for (i = 0; i < numsym; i++)
    {
        if (symtype[i] == OTYPE_LABEL) varcount++;
    }
}

void ModifyBits(int addr, int value, int mask, int shift);

void FixUpRef(char *ptr, int newval, int first, int last)
{
    int i, addr, offset, newval1;

    for (i = first; i < last; i++)
    {
        if (strcmp(ptr, symname[i])) continue;
        if (symtype[i] != OTYPE_REF_AUGS && symtype[i] != OTYPE_REF_AUGD && symtype[i] != OTYPE_REF_LONG_REL) continue;
        addr = symvalue[i];
        offset = symoffset[i];
        newval1 = newval + offset;
        if (debugflag && offset) printf("FixUpRef: offset = %d\n", offset);
        if (symtype[i] == OTYPE_REF_AUGS)
        {
            if (debugflag) printf("Fixing up reference at entry %d at address %8.8x\n", i, addr);
            ModifyBits(addr, newval1 >> 9, 0x7fffff, 0);
            ModifyBits(addr+4, newval1, 0x1ff, 0);
        }
        else if (symtype[i] == OTYPE_REF_AUGD)
        {
            if (debugflag) printf("Fixing up reference at entry %d at address %8.8x\n", i, addr);
            ModifyBits(addr, newval1 >> 9, 0x7fffff, 0);
            ModifyBits(addr+4, newval1, 0x1ff, 9);
        }
        else
        {
            int oldval = mem[(addr>>2)];
            if (debugflag) printf("Fixing up reference at entry %d at address %8.8x\n", i, addr);
            mem[addr>>2] = newval1;
            if (debugflag) printf("Changing variable ref at %8.8x from %3.3x to %3.3x\n", addr, oldval, newval1);
        }
    }
}

int MergeGlobalVariables(int prev_num)
{
    int resolved = 0;
    int i, j, k, first, last;

    for (i = prev_num; i < numsym; i++)
    {
        if (symtype[i] == OTYPE_UNINIT_DATA)
        {
            j = FindSymbol(symname[i], OTYPE_INIT_DATA);
            if (j < 0) j = FindSymbol(symname[i], OTYPE_UNINIT_DATA);
            if (debugflag)
            {
                printf("Found variable %s at %8.8x\n", symname[i], symvalue[i]);
                printf("i = %d, j = %d\n", i, j);
            }
            // Use address from previous object if found
            if (j >= 0 && j != i)
            {
                if (debugflag)
                {
                    printf("Use address from entry %d at address %8.8x\n", j, symvalue[j]);
                    printf("Changing %8.8x to %8.8x\n", symvalue[i], symvalue[j]);
                }
                symvalue[i] = symvalue[j];
                // Fix address in "V" list also
                j = FindGlobalLimits(symname[i], OTYPE_LABEL, prev_num, numsym);
                if (j >= 0)
                {
                    if (debugflag) printf("Also change entry %d\n", j);
                    if (debugflag) printf("Changing %8.8x to %8.8x\n", symvalue[j], symvalue[i]);
                    symvalue[j] = symvalue[i];
                }
            }
        }
        else if (symtype[i] == OTYPE_INIT_DATA)
        {
            if (debugflag) printf("Found initialized global variable %s at entry %d and address %8.8x\n", symname[i], i, symvalue[i]);
            // Loop over previous objects
            for (k = 0; k < objnum-1; k++)
            {
                first = objstart[k];
                last = objstart[k+1];
                j = FindGlobalLimits(symname[i], OTYPE_UNINIT_DATA, first, last);
                if (j < 0)
                {
                    j = FindGlobalLimits(symname[i], OTYPE_INIT_DATA, first, last);
                    if (j >= 0) printf("WARNING: Global variable %s initialized in another object\n", symname[i]);
                }
                if (j < 0) continue;
                if (debugflag) printf("Found global variable %s at entry %d and address %8.8x\n", symname[j], j, symvalue[j]);
                if (debugflag) printf("Changing %8.8x to %8.8x\n", symvalue[j], symvalue[i]);
                FixUpRef(symname[i], symvalue[i], first, last);
                symvalue[j] = symvalue[i];
                // Fix address in "V" list also
                j = FindGlobalLimits(symname[i], OTYPE_LABEL, first, last);
                if (j >= 0)
                {
                    if (debugflag) printf("Also change entry %d\n", j);
                    if (debugflag) printf("Changing %8.8x to %8.8x\n", symvalue[j], symvalue[i]);
                    symvalue[j] = symvalue[i];
                }
                resolved++;
            }
        }
    }
    return resolved;
}

void ModifyBits(int addr, int value, int mask, int shift)
{
    int oldval = mem[addr>>2];
    int newval = (oldval & ~(mask << shift)) | ((value & mask) << shift);
    mem[addr>>2] = newval;
    if (debugflag) printf("Changing variable ref at %8.8x from %3.3x to %3.3x\n", addr, oldval, newval);
}

void FixVariableRef(int prev_num)
{
    int i, j;

    for (i = prev_num; i < numsym; i++)
    {
        if (symtype[i] == OTYPE_REF_AUGS || symtype[i] == OTYPE_REF_AUGD || symtype[i] == OTYPE_REF_LONG_REL)
        {
            int newval;
            int addr = symvalue[i];
            int oldval = mem[addr>>2];
            int offset = symoffset[i];
            j = FindGlobalLimits(symname[i], OTYPE_LABEL, prev_num, numsym);
            if (debugflag)
            {
                printf("Found variable %s at %8.8x\n", symname[i], symvalue[i]);
                printf("i = %d, j = %d\n", i, j);
            }
            if (j < 0)
            {
                if (debugflag)
                    printf("ERROR: Couldn't find variable %s\n", symname[i]);
                continue;
            }
            newval = symvalue[j] + offset;
            if (symtype[i] == OTYPE_REF_AUGS)
            {
                ModifyBits(addr, newval >> 9, 0x7fffff, 0);
                ModifyBits(addr+4, newval, 0x1ff, 0);
            }
            else if (symtype[i] == OTYPE_REF_AUGD)
            {
                ModifyBits(addr, newval >> 9, 0x7fffff, 0);
                ModifyBits(addr+4, newval, 0x1ff, 9);
            }
            else if (symtype[i] == OTYPE_REF_LONG_REL)
            {
                mem[addr>>2] = newval;
                if (debugflag)
                    printf("Changing variable ref at %8.8x from %3.3x to %3.3x\n", addr, oldval, newval);
            }
        }
    }
}

void ReadFile(FILE *infile, int libflag)
{
    int vars_resolved = 0;
    int prev_addr = addr;
    int prev_numsym = numsym;
    int prev_varcount = varcount;

    while (ReadObject(infile))
    {
        if (objnum > 1)
        {
            vars_resolved = MergeGlobalVariables(prev_numsym);
            FixVariableRef(prev_numsym);
        }
        else
            CountVariables();
        if (objnum > 1 && !Resolve(prev_numsym) && !vars_resolved && libflag)
        {
            addr = prev_addr;
            numsym = prev_numsym;
            varcount = prev_varcount;
            objnum--;
        }
        else
        {
            prev_addr = addr;
            prev_numsym = numsym;
            prev_varcount = varcount;
        }
    }
}

int is_lib(char *str)
{
    int len = strlen(str);
    if (len < 2) return 0;
    if (str[len-1] != 'a') return 0;
    if (str[len-2] != '.') return 0;
    return 1;
}

void InitHeapAddress(void)
{
    int i, hub_addr;
    for (i = 0; i < numsym; i++)
    {
        if (symtype[i] == OTYPE_LABEL)
        {
            if (!strcmp(symname[i], "_heapaddr") || !strcmp(symname[i], "_heapaddrlast"))
            {
                hub_addr = symvalue[i] & 0xfffff;
                if (verbose) printf("Setting value of %s at location %x to %x\n", symname[i], hub_addr, addr);
                mem[hub_addr>>2] = addr;
            }
        }
    }
}

int main(int argc, char **argv)
{
    int i;
    char buffer[256];
    char outfname[100];
    int unresolved = 0;
    FILE *infile, *outfile;

    memset(symoffset, 0, MAX_SYMBOLS*4);
    strcpy(outfname, "a.out");
    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            if (!strcmp(argv[i], "-d")) debugflag = 1;
            else if (!strcmp(argv[i], "-v")) verbose = 1;
            else if (argv[i][1] == 'o')
            {
                if (argv[i][2])
                    strcpy(outfname, &argv[i][2]);
                else if (++i >= argc)
                    usage();
                else
                    strcpy(outfname, argv[i]);
            }
            else if (argv[i][1] == 's')
            {
                if (argv[i][2])
                    sscanf(&argv[i][2], "%x", &start_addr);
                else if (++i >= argc)
                    usage();
                else
                    sscanf(argv[i], "%x", &start_addr);
                addr = start_addr;
            }
            else usage();
        }
        else
        {
            infile = OpenFile(argv[i], "rb");
            ReadFile(infile, is_lib(argv[i]));
            fclose(infile);
        }
    }

    if (addr == start_addr) usage();

    Resolve(numsym);
    // Check if any remaining unresolves
    for (i = 0; i < numsym; i++)
    {
        if (symtype[i] == OTYPE_REF_FUNC_UND)
        {
            unresolved++;
            printf("%s is unresolved\n", symname[i]);
        }
    }

    if (unresolved)
    {
        printf("%d symbol(s) are unresolved\n", unresolved);
        exit(1);
    }

    InitHeapAddress();

    outfile = OpenFile(outfname, "wb");
    fwrite(&mem[start_addr>>2], 1, addr - start_addr, outfile);

    if (debugflag)
    {
        int j;
        printf("Object Dump\n");
        for (i = 1; i < objnum; i++)
        {
            printf("Object %d: %s\n", i, objname[i]);
            for (j = objstart[i]; j < objstart[i+1]; j++)
            {
                if (symoffset[j] == 0)
                    printf("%d: %c %8.8x %s\n", j, symtype[j], symvalue[j], symname[j]);
                else if (symoffset[j] > 0)
                    printf("%d: %c %8.8x %s+%d\n", j, symtype[j], symvalue[j], symname[j], symoffset[j]);
                else
                    printf("%d: %c %8.8x %s-%d\n", j, symtype[j], symvalue[j], symname[j], -symoffset[j]);
            }
        }
    }

    return 0;
}
