#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>

#include <detectNet.h>
#include "detect.h"

/* Initialize detection struct. */
int init_detect(detect_net * new_detect, int argc, char **argv, int width, int height)
{
   detectNet *net = NULL;

   /* Create detectNet Instance */
   net = detectNet::Create(argc, argv);
   if (!net) {
      LogError("detectnet:  failed to load detectNet model\n");

      return 1;
   }
   new_detect->detectNet_net = (void *)net;

   /* Allocate CUDA memory for object detection. */
   if (new_detect->d_image == NULL) {
      if (CUDA_FAILED(cudaMalloc(&new_detect->d_image, width * height * sizeof(uchar4)))) {
         printf("Failed cudaMalloc()\n");

         return 1;
      }
   }

   /* Other static variables. */
   new_detect->detections = NULL;
   new_detect->l_width = width;
   new_detect->l_height = height;

   return 0;
}

/* Detect objects in the given image. */
/* max_detections allows us to limit the amount of detections displayed.
 * At this time it just takes the first ones. I'd like to sort by confidence
 * in the future to pick the best ones.
 */
int detect_image(detect_net * new_detect, void *image, detect * my_detects, int max_detections)
{
   int n = 0;
   int a = 0;
   detectNet *net = (detectNet *) new_detect->detectNet_net;
   detectNet::Detection * detections = (detectNet::Detection *) new_detect->detections;
   int numDetections = 0;

   //const uint32_t overlayFlags = detectNet::OVERLAY_BOX | detectNet::OVERLAY_LABEL | detectNet::OVERLAY_CONFIDENCE;
   const uint32_t overlayFlags = 0;

#if 0
   /* Copy image data to CUDA memory for processing. */
   sem_wait(new_detect->v_mutex);
   cudaMemcpy(new_detect->d_image, image,
              new_detect->l_width * new_detect->l_height * sizeof(uchar4), cudaMemcpyHostToDevice);
   sem_post(new_detect->v_mutex);
#endif

   /* Detect */
   numDetections =
       net->Detect((uchar4 *) new_detect->d_image, new_detect->l_width, new_detect->l_height,
                   &detections, overlayFlags);
   if (numDetections > 0) {
      for (n = 0; n < numDetections; n++) {
         /*printf("%d, %s(%d), %f\n",
            n, net->GetClassDesc(detections[n].ClassID), detections[n].ClassID,
            detections[n].Confidence); */

         if (detections[n].Confidence > 0.50) {
            my_detects[a].active = 1;
            strncpy(my_detects[a].description, net->GetClassDesc(detections[n].ClassID), 255);
            my_detects[a].confidence = detections[n].Confidence;
            my_detects[a].left = detections[n].Left;
            my_detects[a].top = detections[n].Top;
            my_detects[a].width = detections[n].Width();
            my_detects[a].height = detections[n].Height();
            a++;
            if (a >= max_detections)
            {
               break;
            }
         }
      }
   }
   //cudaMemcpy(image, new_detect->d_image, new_detect->l_width * new_detect->l_height * sizeof(uchar4), cudaMemcpyDeviceToHost);

   return numDetections;
}

/* Clean up detection struct. */
void free_detect(detect_net * new_detect)
{
   detectNet *net = (detectNet *) new_detect->detectNet_net;

   cudaFree(new_detect->d_image);
   new_detect->d_image = NULL;

   new_detect->l_width = 0;
   new_detect->l_height = 0;

   SAFE_DELETE(net);
}
