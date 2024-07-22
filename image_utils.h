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

#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H

#include <stdint.h>

/**
 * @struct ImageProcessParams
 * @brief Encapsulates parameters for processing and saving images.
 *
 * @var rgba_buffer
 * Pointer to the RGBA pixel data buffer.
 *
 * @var orig_width
 * Original width of the image in pixels.
 *
 * @var orig_height
 * Original height of the image in pixels.
 *
 * @var filename
 * The path and name of the file to save the image to.
 *
 * @var left_crop
 * Number of pixels to crop from the left edge of the image.
 *
 * @var top_crop
 * Number of pixels to crop from the top edge of the image.
 *
 * @var right_crop
 * Number of pixels to crop from the right edge of the image.
 *
 * @var bottom_crop
 * Number of pixels to crop from the bottom edge of the image.
 *
 * @var new_width
 * New width of the image after processing.
 *
 * @var new_height
 * New height of the image after processing.
 *
 * @var format_params
 * Union for format-specific parameters: JPEG quality or PNG compression level.
 */
typedef struct {
    const unsigned char *rgba_buffer;
    int orig_width, orig_height;
    const char *filename;
    int left_crop, top_crop, right_crop, bottom_crop;
    int new_width, new_height;
    union {
        int quality;      // Used for JPEG
        int compression;  // Used for PNG
    } format_params;
} ImageProcessParams;

/**
 * @enum ImageErrorCode
 * @brief Defines error codes for image processing functions.
 *
 * @var IMG_SUCCESS
 * Indicates successful operation.
 *
 * @var IMG_ERR_INVALID_PARAMS
 * Indicates invalid input parameters.
 *
 * @var IMG_ERR_INVALID_CROP
 * Indicates invalid crop values (e.g., crop dimensions exceed image dimensions).
 *
 * @var IMG_ERR_EXTENSION_NOT_FOUND
 * Indicates that the file extension was not found in the filename.
 *
 * @var IMG_ERR_UNSUPPORTED_FORMAT
 * Indicates an unsupported file format (not JPEG or PNG).
 *
 * @var IMG_ERR_INVALID_JPEG_QUALITY
 * Indicates an invalid JPEG quality value (outside 0-100 range).
 *
 * @var IMG_ERR_INVALID_PNG_COMPRESSION
 * Indicates an invalid PNG compression level (outside 0-9 range).
 *
 * @var IMG_ERR_FILE_OPEN_FAILED
 * Indicates failure to open the file for writing.
 *
 * @var IMG_ERR_MEMORY_ALLOCATION_FAILED
 * Indicates a failure in memory allocation during processing.
 *
 * @var IMG_ERR_GD_OPERATION_FAILED
 * Indicates a failure in an operation using the GD library.
 */
typedef enum {
    IMG_SUCCESS = 0,
    IMG_ERR_INVALID_PARAMS = -1,
    IMG_ERR_INVALID_CROP = -2,
    IMG_ERR_EXTENSION_NOT_FOUND = -3,
    IMG_ERR_UNSUPPORTED_FORMAT = -4,
    IMG_ERR_INVALID_JPEG_QUALITY = -5,
    IMG_ERR_INVALID_PNG_COMPRESSION = -6,
    IMG_ERR_FILE_OPEN_FAILED = -7,
    IMG_ERR_MEMORY_ALLOCATION_FAILED = -8,
    IMG_ERR_GD_OPERATION_FAILED = -9
} ImageErrorCode;

// Function prototype for saving RGBA data as a JPEG image
ImageErrorCode save_rgba_to_jpeg(const uint8_t *rgba_buffer,
                                 int width, int height, const char *filename, int quality);

// Function prototype to modify the RGBA data and then save as a JPEG or PNG image
ImageErrorCode process_and_save_image(const ImageProcessParams *params);

#endif // IMAGE_UTILS_H

