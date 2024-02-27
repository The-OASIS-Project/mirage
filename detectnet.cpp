/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "detectNet.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

#define WIDTH 1440
#define HEIGHT 1440

bool signal_recieved = false;

int main( int argc, char** argv )
{
   int sockfd, newsockfd, portno;
   socklen_t clilen;
   char buffer[1024];
   struct sockaddr_in serv_addr, cli_addr;
   int w = 0;
   int detect_ready = 0;

	/*
	 * create detection network
	 */
	detectNet* net = detectNet::Create(argc, argv);
	if( !net )
	{
		LogError("detectnet:  failed to load detectNet model\n");
		return 0;
	}

	// parse overlay flags
	const uint32_t overlayFlags = detectNet::OverlayFlagsFromStr(cmdLine.GetString("overlay", "box,labels,conf"));
	
   // socket for hud communication
   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0)
   {
      perror("Error opening socket.");
      // cleanup
      exit(1);
   }

   bzero((char *) &serv_addr, sizeof(serv_addr));
   portno = 3000;

   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(portno);

   if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
   {
      perror("Error binding socket.");
      // cleanup
      exit(1);
   }

   listen(sockfd, 5);
   clilen = sizeof(cli_addr);

   printf("Ready for connection...\n");

   newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
   if (newsockfd < 0)
   {
      perror("Error on accept.");
      // cleanup
      exit(1);
   }

	/*
	 * processing loop
	 */
	while( !signal_recieved )
	{
		// capture next image image
		float3* image0 = NULL;

		if( !input0->Capture((void **)&image0, IMAGE_RGB32F, 1000) )
		{
			// check for EOS
			if( !input0->IsStreaming() )
				break; 

			LogError("detectnet:  failed to capture video frame\n");
			continue;
		}

		// capture next image image
		float3* image1 = NULL;

		if( !input1->Capture((void **)&image1, IMAGE_RGB32F, 1000) )
		{
			// check for EOS
			if( !input1->IsStreaming() )
				break; 

			LogError("detectnet:  failed to capture video frame\n");
			continue;
		}


		// detect objects in the frame
		detectNet::Detection* detections0 = NULL;
		detectNet::Detection* detections1 = NULL;
	
		const int numDetections0 = net->Detect(image0, input0->GetWidth(), input0->GetHeight(), &detections0, overlayFlags);
		const int numDetections1 = net->Detect(image1, input1->GetWidth(), input1->GetHeight(), &detections1, overlayFlags);
		
		if( (numDetections0 > 0) || (numDetections1 > 0) )
		{
			LogVerbose("%i objects detected on cam0\n", numDetections0);
			LogVerbose("%i objects detected on cam1\n", numDetections1);
		
         for (int n=0; n < numDetections0; n++) {
            for (int p=0; p < numDetections1; p++) {
               LogVerbose("%d, %d, %s(%d), %s(%d), %f, %f\n", n, p, net->GetClassDesc(detections0[n].ClassID), detections0[n].ClassID, net->GetClassDesc(detections1[p].ClassID), detections1[p].ClassID, detections0[n].Confidence, detections1[p].Confidence);
               if ((detections0[n].ClassID == detections1[p].ClassID) &&
                   (detections0[n].Confidence > 0.50) && (detections1[p].Confidence > 0.50) /* &&
                   (((detections0[n].Left * 0.85) < detections1[p].Left) && ((detections0[n].Left * 1.15) > detections1[p].Left)) &&
                   (((detections0[n].Top * 0.85) < detections1[p].Top) && ((detections0[n].Top * 1.15) > detections1[p].Top)) */
                  )
               {
                  /* LEFT */
                  snprintf(buffer, 1024, 
                        "{ \"device\": \"detect\", \"eye\": \"left\", \"id\": %d, \"description\": \"%s\", \"confidence\": %f, \"left\": %f, \"top\": %f, \"width\": %f, \"height\": %f }\n",
                        n, net->GetClassDesc(detections0[n].ClassID), detections0[n].Confidence, detections0[n].Left, detections0[n].Top, detections0[n].Width(), detections0[n].Height());
                  LogVerbose("%s", buffer);
                  w = write(newsockfd, buffer, strlen(buffer));
                  if (w == -1) {
                     printf("Write failed. Closing...\n");
                     signal_recieved = true;
                  }
				      //LogVerbose("detected obj %i  class #%u (%s)  confidence=%f\n",
                  //           n, detections0[n].ClassID, net->GetClassDesc(detections0[n].ClassID), detections0[n].Confidence);
				      //LogVerbose("bounding box %i  (%f, %f)  (%f, %f)  w=%f  h=%f\n",
                  //           n, detections0[n].Left, detections0[n].Top, detections0[n].Right, detections0[n].Bottom, detections0[n].Width(), detections0[n].Height());

                  /* RIGHT */
                  snprintf(buffer, 1024, 
                        "{ \"device\": \"detect\", \"eye\": \"right\", \"id\": %d, \"description\": \"%s\", \"confidence\": %f, \"left\": %f, \"top\": %f, \"width\": %f, \"height\": %f }\n",
                        n, net->GetClassDesc(detections1[p].ClassID), detections1[p].Confidence, detections1[p].Left, detections1[p].Top, detections1[p].Width(), detections1[p].Height());
                  LogVerbose("%s", buffer);
                  w = write(newsockfd, buffer, strlen(buffer));
                  if (w == -1) {
                     printf("Write failed. Closing...\n");
                     signal_recieved = true;
                  }
				      //LogVerbose("detected obj %i  class #%u (%s)  confidence=%f\n",
                  //           n, detections1[p].ClassID, net->GetClassDesc(detections1[p].ClassID), detections1[p].Confidence);
				      //LogVerbose("bounding box %i  (%f, %f)  (%f, %f)  w=%f  h=%f\n",
                  //           n, detections1[p].Left, detections1[p].Top, detections1[p].Right, detections1[p].Bottom, detections1[p].Width(), detections1[p].Height()); 

               }
            }
         }

#if 0
         int max = 0;

         if (numDetections0 > numDetections1)
         {
            max = numDetections0;
         } else {
            max = numDetections1;
         }

			for( int n=0; n < max; n++ )
			{
            if (n < numDetections0)
            {
               snprintf(buffer, 1024, "{ \"device\": \"detect\", \"eye\": \"left\", \"id\": %d, \"description\": \"%s\", \"confidence\": %f, \"left\": %f, \"top\": %f, \"width\": %f, \"height\": %f }", n, net->GetClassDesc(detections0[n].ClassID), detections0[n].Confidence, detections0[n].Left, detections0[n].Top, detections0[n].Width(), detections0[n].Height());
               w = write(newsockfd, buffer, strlen(buffer));
				   LogVerbose("detected obj %i  class #%u (%s)  confidence=%f\n", n, detections0[n].ClassID, net->GetClassDesc(detections0[n].ClassID), detections0[n].Confidence);
				   LogVerbose("bounding box %i  (%f, %f)  (%f, %f)  w=%f  h=%f\n", n, detections0[n].Left, detections0[n].Top, detections0[n].Right, detections0[n].Bottom, detections0[n].Width(), detections0[n].Height()); 
            }
            if (n < numDetections1)
            {
               snprintf(buffer, 1024, "{ \"device\": \"detect\", \"eye\": \"right\", \"id\": %d, \"description\": \"%s\", \"confidence\": %f, \"left\": %f, \"top\": %f, \"width\": %f, \"height\": %f }", n, net->GetClassDesc(detections0[n].ClassID), detections0[n].Confidence, detections0[n].Left, detections0[n].Top, detections0[n].Width(), detections0[n].Height());
               w = write(newsockfd, buffer, strlen(buffer));
				   LogVerbose("detected obj %i  class #%u (%s)  confidence=%f\n", n, detections1[n].ClassID, net->GetClassDesc(detections1[n].ClassID), detections1[n].Confidence);
				   LogVerbose("bounding box %i  (%f, %f)  (%f, %f)  w=%f  h=%f\n", n, detections1[n].Left, detections1[n].Top, detections1[n].Right, detections1[n].Bottom, detections1[n].Width(), detections1[n].Height()); 
            }
			}
#endif
		}	

		// render outputs
		if( output != NULL )
		{
         output->BeginRender();
         output->RenderImage(image0, WIDTH, HEIGHT, IMAGE_RGB32F, 0, 0);
         output->RenderImage(image1, WIDTH, HEIGHT, IMAGE_RGB32F, WIDTH, 0);
         output->EndRender();

			// update the status bar
			char str[256];
			sprintf(str, "TensorRT %i.%i.%i | %s | Network %.0f FPS", NV_TENSORRT_MAJOR, NV_TENSORRT_MINOR, NV_TENSORRT_PATCH, precisionTypeToStr(net->GetPrecision()), net->GetNetworkFPS());
			output->SetStatus(str);

			// check if the user quit
			if( !output->IsStreaming() )
				signal_recieved = true;
		}

      if (!detect_ready) {
         snprintf(buffer, 1024, 
                  "{ \"device\": \"detect\", \"ready\": 1 }\n");
         LogVerbose("%s", buffer);
         w = write(newsockfd, buffer, strlen(buffer));

         detect_ready = 1;
      }

		// print out timing info
		net->PrintProfilerTimes();
	}
	

	/*
	 * destroy resources
	 */
	LogVerbose("detectnet:  shutting down...\n");

   close(newsockfd);
   close(sockfd);
	
	SAFE_DELETE(net);

	LogVerbose("detectnet:  shutdown complete.\n");
	return 0;
}

