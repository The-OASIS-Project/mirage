#ifdef __cplusplus
extern "C" {
#endif

typedef struct _detect {
   int active;
   char description[256];
   double confidence;
   double left;
   double top;
   double width;
   double height;
} detect;

typedef struct _detect_net {
   void *detectNet_net;
   void* detections;

   void *d_image;
   int l_width;
   int l_height;

   sem_t *v_mutex;
} detect_net;

int init_detect(detect_net *new_detect, int argc, char **argv, int width, int height);
int detect_image(detect_net *new_detect, void *image, detect *my_detects, int max_detections);
void free_detect(detect_net *new_detect);

#ifdef __cplusplus
}
#endif
