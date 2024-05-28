#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <strings.h>
#include <time.h> // Included only to track duration of test
#include <libgen.h> // filename manipulation
#include "TreeSet.h"

/* DataFilter - reads in a file of ISO 8601 dates in Zulu time or with a TZ adjustment, applies the adjustment to the time, then prints the original input to an output file
(called <input filename>_output.txt ) as long as it is unique in the original file. */

static unsigned int verbose_enabled = 0;

/* Utilities*/

static void verbose_printf (unsigned int level, const char *format, ...)
{
    if (verbose_enabled >= level)
    {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
    }
}

// These types form the 2D array of centuries X years. The centuries are represented by an array indexed by the first
// two numbers in the year. If a node insert occurs within a century, the years array is dynamically allocated using the century struct.

// we add 1 to century range to allow for adjustments encountering year -1 or year 10000
#define CENTURY_INDEX 101
#define CENTURY_RANGE 100

typedef struct century {
    struct Tree *year[CENTURY_RANGE];
} century;

century *centuries[CENTURY_INDEX] = {NULL};

#ifdef TESTSET_PROFILE
static size_t memory_usage = 0;
#endif

/*(Create key for bit for the TreeSet (year covered by arrays containing the TreeSet struct) */
static unsigned int MakeKey(int month, int day, int hour, int minute, int second)
{
   /*(Months fit within 4 bits,  day of the month and hour of the day fit within 5 bits, while
      minute of hour and second within minute fit with 6 bits. In total, they fit within 26 bits.*/

   return (month<<22) + (day<<17) + (hour<<12) + (minute<<6) + second;
}

/* Check if a TS is already set in our TreeSet, then set it as present if it was not.
   Return true if already present, false if not. */
unsigned int CheckInsertTSPresent (int year, int month, int day, int hour, int minute, int second)
{
  struct Tree *year_tree = NULL;
  unsigned int key = MakeKey (month, day, hour, minute, second);

  unsigned int already_present = 0;
  // Increment year by 1 to allow for year -1 and year 10000 due to time offset
  int century_idx = (year+1) / 100;
  int year_idx = (year+1) % 100;

   /* Find or create the decade struct and tree root for the given year */
   verbose_printf (2,"index year=%d so Century idx=%d[%p], year_idx=%d\n", year+1, century_idx, centuries[century_idx], year_idx);
   if (centuries[century_idx] == NULL)
   {
      centuries[century_idx] = malloc(sizeof(century));
      memset (centuries[century_idx], 0, sizeof(century));
      #ifdef TESTSET_PROFILE
      memory_usage += sizeof(century);
      #endif // TESTSET_PROFILE

      verbose_printf (2,"allocated a year range at %d, %p\n", century_idx, centuries[century_idx]);
   }

   if (centuries[century_idx])
   {
       year_tree = centuries[century_idx]->year[year_idx];
       if (!year_tree)
       {
           centuries[century_idx]->year[year_idx] = year_tree = CreateTree(60);
       }
   }

   SetBit (year_tree, key, 1, &already_present);

   verbose_printf(1, "SetBit complete, bits prior setting=%d\n", already_present);

   return already_present;
}

/* Verify the buffer passed in is a ISO 8501 Zulu or TimeZone format before setting the year,month,day,hour,minute, and second fields to the
 * adjusted Zulu time. If the time is adjusted,  tz_adjusted will be set to 1. Otherwise, a 0 denoting no adjustment. Return true if the line parsed, false otherwise. */
int parse_timestamp (char * buffer, unsigned int length, int *year, int *month, int *day, int *hour, int *minute, int *second, int *tz_adjusted)
{
   char abstract_format[30] = {0};
   char tzd[30] = {0};
   int adj_hr=0, adj_min=0;
   unsigned int parse_failed = 0;
   unsigned int count = 0;
   unsigned int output_idx = 0;

   if (buffer)
   {
     // buffer must fall between these lengths to be valid.
     if ((length < 20) || (length > 25))
     {
         parse_failed = 1;
         verbose_printf (2,"Length of %d shorter than 20 or longer than 25, fail!\n",  length);
         return parse_failed;
     }

     // Build an abstract format of the input buffer to match against vaild patterns below.
     for (int idx=0;(idx<length) && !parse_failed;idx++)
     {
        if ((buffer[idx] >= '0') && (buffer[idx] <= '9'))
        {
            count++;
        }
        else
        {
            if (count > 0)
               abstract_format[output_idx++] = '0' + count;
            abstract_format[output_idx++] = buffer[idx];
            count = 0;
        }
     }

     // add any trailing digits into abstract format.
     if (count > 0)
     {
         abstract_format[output_idx++] = '0' + count;
     }
     verbose_printf (2,"Format str:%s\n", abstract_format);

     if (!parse_failed)
     {
        // Check against the valid ISO 8601 patterns per problem definition.
        parse_failed = !((strcmp(abstract_format, "4-2-2T2:2:2+2:2") == 0) || (strcmp(abstract_format, "4-2-2T2:2:2-2:2") == 0) || (strcmp(abstract_format, "4-2-2T2:2:2Z") == 0));
     }

     if (!parse_failed && sscanf (buffer, "%4d-%2d-%2dT%2d:%2d:%2d%s", year, month, day, hour, minute, second, tzd))
     {
         char tz_sign;
         verbose_printf (2,"year:%d month:%d day:%d, hour:%d minute:%d, second:%d (%s)\n", *year, *month, *day, *hour, *minute, *second, tzd);

         // Check for a timezone modification field and ripple the adjustments upward through day, month, year as needed.
         if (tzd[0] != 'Z')
         {
            if (sscanf (tzd, "%c%2d:%2d", &tz_sign, &adj_hr, &adj_min))
            {
               *tz_adjusted = ((adj_hr != 0) || (adj_min != 0))?1:0;

               verbose_printf(2, " tz adjusted=%d (adj_hr:%d adj_min:%d)\n", *tz_adjusted, adj_hr, adj_min);
               if (tz_sign == '-')
               {
                   adj_hr = -adj_hr;
                   adj_min = -adj_min;
               }
               *minute += adj_min;

               if (*minute > 59)
               {
                  adj_hr++;
                  *minute %= 60;
               }
               else if (*minute < 0)
               {
                   adj_hr--;
                   *minute += 60;
               }
               *hour += adj_hr;

               if (*hour > 23)
               {
                  (*day)++;
                  *hour %= 24;
               }
               else if (*hour <= 0)
               {
                  (*day)--;
                  *hour += 23;
               }

               if (*day > 31)
               {
                  (*month)++;
                  *day %= 31;
               }
               else if (*day <= 0)
               {
                  (*month)--;
                  *day += 31;
               }

               if (*month > 12)
               {
                  (*year)++;
                  *month %= 12;
               }
               else if (*month <= 0)
               {
                  (*year)--;
                  *month += 12;
               }
               verbose_printf (2,"Adjusted year:%d month:%d day:%d, hour:%d minute:%d, second:%d\n\n", *year, *month, *day, *hour, *minute, *second);
            }
            else
            {
                parse_failed = 1;
            }
         }
         else
         {
             *tz_adjusted = 0;
         }
      }
   }
   return parse_failed;
}

int main (int argc , char **argv)
{
    FILE *fp_in;
    FILE *fp_out;
    char buffer[255];
    char input_filename[300]="test.txt";
    char output_filename[300] = "output.txt";
    int year, month, day, hour, minute, second, tz_adjusted;

    unsigned int ts_handled = 0;
    unsigned int duplicates_found = 0;
    unsigned int written_to_file = 0;
    unsigned int parse_failures = 0;
    unsigned int lines_in_file = 0;

    for (int i = 1;i < argc; i++) {
        if (argv[i][0] != '-')
        {
           strcpy (input_filename, argv[i]);
        }
        else
        {
            if (argv[i][1] == 'v')
            {
                verbose_enabled = (unsigned int) (argv[i][2] - '0');

            }
        }
    }

    if (verbose_enabled > 0)
    {
       printf ("Verbose level set to %d\n", verbose_enabled);
       SetTSVerbose(verbose_enabled);
    }

    fp_in=fopen(input_filename, "r");

    if (!fp_in)
    {
        fprintf(stderr, "Input file %s cannot be opened:%s\n", input_filename, strerror(errno));
        exit(errno);
    }

    strcpy(output_filename, basename(input_filename));
    char *output_ext = strrchr(output_filename, '.');
    if (output_ext)
        *output_ext = 0;
    strcat(output_filename, "_output.txt");
     printf ("Filtering file '%s' into '%s'.\n", input_filename, output_filename);

    fp_out = fopen (output_filename, "w");

    if (!fp_out)
    {
       fprintf(stderr, "Output file test_output.txt failed to open!\n");
       exit(errno);

    }
    printf ("\n");

    // Used to measure rough duration of test only.
    clock_t start_time = clock();

    while (fgets(buffer, 255, (FILE*)fp_in))
    {
        int parse_failed = 0;
        unsigned int len = strlen(buffer);

        lines_in_file++;

        // remove any extra return lines
        if ((len>0) && (buffer[len-1]=='\n'))
        {
           buffer[len-1] = 0;
           len--;
        }

        verbose_printf (1, "Buffer read:%s\n", buffer);

        parse_failed = parse_timestamp(buffer, len, &year, &month, &day, &hour, &minute, &second, &tz_adjusted);

        if (!parse_failed)
        {

            verbose_printf (2,"year:%d month:%d day:%d, hour:%d minute:%d, second:%d (adjusted=%s)\n", year, month, day, hour, minute, second, (tz_adjusted !=0)?"true":"false");

            ts_handled++;

            // Check if the absolute timestamp has been seen before printing to output file
            if (CheckInsertTSPresent(year, month, day, hour, minute, second) == 1)
            {
               if (!tz_adjusted)
                  verbose_printf (1,"Duplicate '%s' found, discarding (key=%d).\n", buffer, MakeKey (month, day,hour, minute, second));
               else
                  verbose_printf (1, "Duplicate '%s' found, discarding (%04d-%02d-%02dT%02d:%02d:%02dZ normalized, key=%d).\n", buffer, year, month, day, hour, minute, second, MakeKey (month, day,hour, minute, second));

               duplicates_found++;
            }
            else
            {
               fprintf (fp_out, "%s\n", buffer);
               if (!tz_adjusted)
                  verbose_printf (1, "NewEntry: '%s' added to output file (key=%d).\n", buffer, MakeKey (month, day,hour, minute, second));
               else
                  verbose_printf (1, "NewEntry: '%s' added to output file (%04d-%02d-%02dT%02d:%02d:%02dZ normalized, key=%d).\n", buffer, year, month, day, hour, minute, second, MakeKey (month, day,hour, minute, second));

               written_to_file++;
            }

        }
        else
        {
            verbose_printf (1, "Item '%s' failed parse, discarded.\n", buffer);
            parse_failures++;
        }
        verbose_printf(1, "Processing done.\n====================\n");
    }
    clock_t stop_time = clock();
    double run_time = (double) (stop_time - start_time) / CLOCKS_PER_SEC;
    verbose_printf (2,"\n\nEOF\n");

    fclose(fp_in);
    fclose(fp_out);

#ifdef TESTSET_PROFILE
    printf ("DataFilter: RunTime:%f Mem Usage: %Iu TS Mem Usage:%Iu for %d nodes in system in %d trees.\n (%d lines of input => %d failed parse, %d ts parsed => %d written to file, %d discarded).\n",
            run_time, memory_usage, GetTSMemory(), GetTSNodes(), GetTSTrees(),
            lines_in_file, parse_failures, ts_handled, written_to_file, duplicates_found);
#else
    printf ("DataFilter: RunTime: %f \n %d lines of input => %d failed parse, %d ts parsed => %d written to file, %d discarded).\n", run_time, lines_in_file, parse_failures, ts_handled, written_to_file, duplicates_found);

#endif // TESTSET_PROFILE

    int print_once = 0;
    for (int i=0;i<CENTURY_INDEX;i++)
    {

       if (centuries[i] != NULL)
       {
          for (int j=0;j<CENTURY_RANGE;j++)
          {
             if (centuries[i]->year[j])
             {
                if ((verbose_enabled > 0) || (!print_once))
                {
                   printf ("\nTree %d=>", ((i*100)+j)-1);
                   //PrintTree (centuries[i]->year[j]);
                   TreeInfo(centuries[i]->year[j]);
                   print_once = 1;

                }

                DestroyTree(centuries[i]->year[j]);

             }
          }
          free(centuries[i]);
          #ifdef TESTSET_PROFILE
          memory_usage -= sizeof(century);
          #endif
       }
    }

#ifdef TESTSET_PROFILE
    printf ("After destroying memory - DataFilter Mem Usage: %Iu TS Mem Usage: %Iu for %d nodes in system. \n", memory_usage, GetTSMemory(), GetTSNodes());
#endif

    return 0;
}

