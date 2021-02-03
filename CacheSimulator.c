#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#define MAX_SIZE 999
typedef long int ADDRESS;
typedef struct
{
    int valid;
    long sign;
    int usage;
} LINE;
typedef LINE **CACHE;
typedef LINE *SET;
static int usageline = 0;
static CACHE cache;
static int s_num = 0;
static int e_num = 0;
static int b_num = 0;
void printHelp()
{
    printf("./csim: Missing required command line argument\n");
    printf("Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("-h         Print this help message.\n");
    printf("-v         Optional verbose flag.\n");
    printf("-s <num>   Number of set index bits.\n");
    printf("-E <num>   Number of lines per set.\n");
    printf("-b <num>   Number of block offset bits.\n");
    printf("-t <file>  Trace file.\n\n");
    printf("Examples:\n");
    printf("linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

CACHE cache_init()
{
    int i, k;
    CACHE c_cache = (CACHE)malloc(sizeof(SET) * (1 << s_num));
    for (i = 0; i < (1 << s_num); i++)
    {
        c_cache[i] = (SET)malloc(sizeof(LINE) * e_num);
        for (k = 0; k < e_num; k++)
        {
            c_cache[i][k].valid = 0;
        }
    }
    return c_cache;
}

int match_cache(ADDRESS addr, int* set_catch, int* i_catch)
{
    long key = addr >> (s_num + b_num);
    long mask = ~((1 << 31) >> (31 - s_num - b_num));
    ADDRESS masked = addr & mask;
    long set = masked >> b_num;
    int i;
    for (i = 0; i < e_num; i++)
    {
        if (cache[set][i].sign == key && cache[set][i].valid)
        {
            *set_catch = set;
            *i_catch = i;
            return 1;
        }
    }
    return 0;
}

int cache_add(ADDRESS addr) //add&evict
{
    long i, j;
    int min_usage_line = 99999999;
    int minj = 0;
    i = ((addr & ~((1 << 31) >> (31 - s_num - b_num))) >> b_num); //set index
    for (j = 0; j < e_num; j++)
    {
        if (!cache[i][j].valid)
        {
            cache[i][j].sign = addr >> (s_num + b_num);
            cache[i][j].valid = 1;
            cache[i][j].usage = usageline++;
            return 1; //cold miss, loaded.
        }
        if (cache[i][j].usage < min_usage_line)
        {
            min_usage_line = cache[i][j].usage;
            minj = j;
        }
    }
    cache[i][minj].sign = addr >> (s_num + b_num);
    cache[i][minj].valid = 1;
    cache[i][minj].usage = usageline++;
    return 0;
}

int call_cache(ADDRESS addr, int *hit_time, int *miss_time, int *evic_time)
{
    int *set_catcher = (int*)malloc(sizeof(int));
    int *i_catcher = (int*)malloc(sizeof(int));
    if (match_cache(addr, set_catcher, i_catcher))
    {
        (*hit_time)++;
        cache[*set_catcher][*i_catcher].usage = usageline++;
        return 1; //call hit.
    }
    else
    {
        (*miss_time)++; //call miss.
        if (!cache_add(addr))
            (*evic_time)++; //cold miss, no eviction.
        return 0;
    }
}

int modify_cache(ADDRESS addr, int *hit_time, int *miss_time, int *evic_time)
{
    int a = call_cache(addr, hit_time, miss_time, evic_time);
    call_cache(addr, hit_time, miss_time, evic_time);
    return a;
}
int main(int argc, char *argv[])
{
    char optch;
    int help = 0, verbose = 0, result = 0;
    FILE *input_file;
    char *addr_buf;
    char *cmd_buf = (char *)malloc(sizeof(char) * MAX_SIZE);
    char *cmd_temp_buf = (char *)malloc(sizeof(char) * MAX_SIZE);
    int *hit_time = (int *)malloc(sizeof(int));
    int *miss_time = (int *)malloc(sizeof(int));
    int *evic_time = (int *)malloc(sizeof(int));
    //process input args.
    while ((optch = getopt(argc, argv, "hvs:E:b:t:")) != -1)
    {
        switch (optch)
        {
        case 'h':
            help = 1;
            break;
        case 'v':
            verbose = 1;
            break;
        case 's':
            s_num = atoi(optarg);
            break;
        case 'E':
            e_num = atoi(optarg);
            break;
        case 'b':
            b_num = atoi(optarg);
        case 't':
            input_file = fopen(optarg, "r");
        }
    }
    cache = cache_init();
    if (s_num < 1 || e_num < 1 || b_num < 1 || help || input_file == NULL)
    {
        printHelp();
        return 0;
    }
    //main loop.
    int test;
    while (fgets(cmd_temp_buf, MAX_SIZE, input_file) != NULL)
    {
        strncpy(cmd_buf, cmd_temp_buf, strlen(cmd_temp_buf) - 1);
        if (cmd_buf[0] == ' ')
        {
            addr_buf = strtok(&cmd_buf[3], ",");
            switch (cmd_buf[1])
            {
            case 'S':
                test = 1 + 1;
            case 'L':
                result = call_cache(strtol(addr_buf, &cmd_temp_buf, 16), hit_time, miss_time, evic_time); //?
                if (verbose)
                {
                    if (result)
                    {
                        printf("%s %s", &cmd_buf[1], "hit");
                    }
                    else
                        printf("%s %s", &cmd_buf[1], "miss");
                }
                break;
            case 'M':
                result = modify_cache(strtol(addr_buf, &cmd_temp_buf, 16), hit_time, miss_time, evic_time);
                if (verbose)
                {
                    if (result)
                    {
                        printf("%s %s %s", &cmd_buf[1], "hit", "hit");
                    }
                    else
                        printf("%s %s %s", &cmd_buf[1], "miss", "hit");
                }
            }
            printf("\n");
        }
    }

    printSummary(*hit_time, *miss_time, *evic_time);
    return 0;
}
