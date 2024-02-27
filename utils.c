#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

/* Check to see if the file has grown in size. Save new size to passed in value. */
int has_file_grown(const char* filename, off_t *last_size) {
   struct stat sb;

   if (stat(filename, &sb) == -1) {
      perror("stat");
      return -1;
   }

   if (sb.st_size > *last_size) {
      *last_size = sb.st_size;
      return 0;
   }

   return 1;
}
