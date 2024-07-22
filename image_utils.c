/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * By contributing to this project, you agree to license your contributions
 * under the GPLv3 (or any later version) or any future licenses chosen by
 * the project author(s). Contributions include any modifications,
 * enhancements, or additions to the project. These contributions become
 * part of the project and are adopted by the project author(s).
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <gd.h>

#include "image_utils.h"

/**
 * Saves an RGBA buffer as a JPEG image.
 * Note: Ultimately this is a convenience and an early function, the other function
 *       can do this with more setup. I've kept it for convenience.
 *
 * @param rgba_buffer The RGBA pixel data buffer.
 * @param width The width of the image in pixels.
 * @param height The height of the image in pixels.
 * @param filename The path and name of the file to save the image to.
 * @param quality The quality of the JPEG image to be saved (0-100).
 *
 * @return An error code from ImageErrorCode indicating the result of the operation.
 */
ImageErrorCode save_rgba_to_jpeg(const uint8_t *rgba_buffer, int width, int height,
                                 const char *filename, int quality) {
   // Validate input perameters 
   if (!rgba_buffer || width <= 0 || height <= 0 || !filename || quality < 0 || quality > 100) {
      return IMG_ERR_INVALID_PARAMS;
   }

   // Create GD image
   gdImagePtr img = gdImageCreateTrueColor(width, height);
   if (!img) {
      return IMG_ERR_GD_OPERATION_FAILED;
   }

   // Copy RGBA buffer to the GD image
   for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
         int offset = (y * width + x) * 4;
         uint8_t r = rgba_buffer[offset];
         uint8_t g = rgba_buffer[offset + 1];
         uint8_t b = rgba_buffer[offset + 2];
         uint8_t a = rgba_buffer[offset + 3];
         int color = gdImageColorAllocateAlpha(img, r, g, b, (127 - a / 2));
         gdImageSetPixel(img, x, y, color);
      }
   }

   // Save the image as JPEG
   FILE *outFile = fopen(filename, "wb");
   if (!outFile) {
      gdImageDestroy(img);
      return IMG_ERR_FILE_OPEN_FAILED;
   }
   gdImageJpeg(img, outFile, quality);
   fclose(outFile);
   gdImageDestroy(img);

   return IMG_SUCCESS;
}

/**
 * Converts a string to lowercase. Helper funcion. Not exported.
 *
 * @param str Pointer to the string to be converted.
 */
void str_tolower(char *str) {
    for ( ; *str; ++str) {
       *str = tolower(*str);
    }
}

/**
 * Processes an RGBA buffer, performs cropping and resizing, and saves the result to a file.
 * The output format (JPEG or PNG) is determined by the file extension. Quality or compression level is
 * specified according to the format.
 *
 * @param params Pointer to an ImageProcessParams structure containing the processing parameters.
 *
 * @return An error code from ImageErrorCode indicating the result of the operation.
 */
ImageErrorCode process_and_save_image(const ImageProcessParams *params) {
   // Validate input parameters
   if (!params || !params->rgba_buffer || params->orig_width <= 0 || params->orig_height <= 0 ||
      params->new_width <= 0 || params->new_height <= 0 ||
      params->left_crop < 0 || params->right_crop < 0 ||
      params->top_crop < 0 || params->bottom_crop < 0 ||
      params->left_crop + params->right_crop >= params->orig_width ||
      params->top_crop + params->bottom_crop >= params->orig_height) {
      return IMG_ERR_INVALID_PARAMS;
   }

   // Determine output format based on the file extension
   char *ext = strrchr(params->filename, '.');
   if (!ext) {
      return IMG_ERR_EXTENSION_NOT_FOUND;
   }

   char lower_ext[5];
   strncpy(lower_ext, ext, sizeof(lower_ext));
   str_tolower(lower_ext);

   int is_jpeg = strcmp(lower_ext, ".jpg") == 0 || strcmp(lower_ext, ".jpeg") == 0;
   int is_png = strcmp(lower_ext, ".png") == 0;

   if (!is_jpeg && !is_png) {
      return IMG_ERR_UNSUPPORTED_FORMAT;
   }

   if (is_jpeg && (params->format_params.quality < 0 || params->format_params.quality > 100)) {
      return IMG_ERR_INVALID_JPEG_QUALITY;
   }

   if (is_png && (params->format_params.compression < 0 || params->format_params.compression > 9)) {
      return IMG_ERR_INVALID_PNG_COMPRESSION;
   }

   // Create a GD image from the RGBA buffer
   gdImagePtr srcImg = gdImageCreateTrueColor(params->orig_width, params->orig_height);
   if (!srcImg) {
      return IMG_ERR_GD_OPERATION_FAILED;
   }

   // Populate the image with pixels from the RGBA buffer
   for (int y = 0; y < params->orig_height; ++y) {
      for (int x = 0; x < params->orig_width; ++x) {
         int idx = (y * params->orig_width + x) * 4;
         int color = gdImageColorAllocateAlpha(srcImg, params->rgba_buffer[idx], params->rgba_buffer[idx + 1],
                                               params->rgba_buffer[idx + 2], 127 - (params->rgba_buffer[idx + 3] >> 1));
          gdImageSetPixel(srcImg, x, y, color);
      }
   }

    // Perform cropping and resizing
    gdImagePtr finalImg = gdImageCreateTrueColor(params->new_width, params->new_height);
    if (!finalImg) {
        gdImageDestroy(srcImg);
        return IMG_ERR_MEMORY_ALLOCATION_FAILED;
    }
    gdImageCopyResampled(finalImg, srcImg, 0, 0, params->left_crop, params->top_crop,
                         params->new_width, params->new_height,
                         params->orig_width - (params->left_crop + params->right_crop),
                         params->orig_height - (params->top_crop + params->bottom_crop));

    // Save the final image based on the determined format
    FILE *outFile = fopen(params->filename, "wb");
    if (!outFile) {
        gdImageDestroy(srcImg);
        gdImageDestroy(finalImg);
        return IMG_ERR_FILE_OPEN_FAILED;
    }

    if (is_jpeg) {
        gdImageJpeg(finalImg, outFile, params->format_params.quality);
    } else if (is_png) {
        gdImagePngEx(finalImg, outFile, params->format_params.compression);
    }

    fclose(outFile);
    gdImageDestroy(srcImg);
    gdImageDestroy(finalImg);

    return IMG_SUCCESS; // Indicate success
}
