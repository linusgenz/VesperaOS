/**
 * @file jpeg.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 02.01.26.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef VESPERAOS_JPEG_H
#define VESPERAOS_JPEG_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Supported pixel formats
 */
typedef enum {
    PIXEL_FORMAT_RGB,      // 24-bit: R,G,B
    PIXEL_FORMAT_RGBA,     // 32-bit: R,G,B,A
    PIXEL_FORMAT_BGR,      // 24-bit: B,G,R
    PIXEL_FORMAT_BGRA,     // 32-bit: B,G,R,A
    PIXEL_FORMAT_BGRX,     // 32-bit: B,G,R,X (padding byte)
    PIXEL_FORMAT_GRAY,     // 8-bit grayscale
    PIXEL_FORMAT_GRAY_ALPHA // 16-bit: Gray,Alpha
} pixel_format_t;

/**
 * image_t structure
 */
typedef struct {
    uint8_t*     data;         // image_t pixel data
    size_t       width;        // image_t width in pixels
    size_t       height;       // image_t height in pixels
    size_t       stride;       // Bytes per scanline
    size_t       size;         // Total size of the image (not the File!)
    pixel_format_t  format;       // Pixel format
    uint8_t      channels;     // Number of color channels
    uint8_t      precision;    // Data precision (8 or 12 bits)
} image_t;

/**
 * JPEG loading options
 */
typedef struct {
    pixel_format_t  output_format;      // Desired output format (default: RGB)
    bool         fast_dct;           // Use fast but less accurate DCT (default: false)
    bool         do_fancy_upsampling;// High-quality upsampling (default: true)
    int          scale_num;          // Scale numerator (1-16, default: 1)
    int          scale_denom;        // Scale denominator (1,2,4,8, default: 1)
} jpeg_load_options_t;

/**
 * JPEG saving options
 */
typedef struct {
    int          quality;            // Quality 0-100 (default: 85)
    bool         progressive;        // Progressive encoding (default: false)
    bool         optimize_coding;    // Optimize Huffman tables (default: true)
    int          smoothing_factor;   // Smoothing 0-100 (default: 0)
    int          h_samp_factor;      // Horizontal sampling (1=4:4:4, 2=4:2:0, default: 2)
    int          v_samp_factor;      // Vertical sampling (1=4:4:4, 2=4:2:0, default: 2)
    uint8_t      data_precision;     // 8 or 12 bits (default: 8)
} jpeg_save_options_t;

/**
 * Error codes
 */
typedef enum {
    JPEG_OK = 0,
    JPEG_ERROR_INVALID_PARAM = -1,
    JPEG_ERROR_MEMORY = -2,
    JPEG_ERROR_FILE_OPEN = -3,
    JPEG_ERROR_FILE_READ = -4,
    JPEG_ERROR_FILE_WRITE = -5,
    JPEG_ERROR_DECODE = -6,
    JPEG_ERROR_ENCODE = -7,
    JPEG_ERROR_INVALID_FORMAT = -8,
    JPEG_ERROR_UNSUPPORTED = -9
} jpeg_error_t;

/* ========================================================================== */
/*                            FUNCTION DECLARATIONS                           */
/* ========================================================================== */

/**
 * Initialize default load options
 */
void jpeg_load_options_init(jpeg_load_options_t* opts);

/**
 * Initialize default save options
 */
void jpeg_save_options_init(jpeg_save_options_t* opts);

/**
 * Load JPEG from memory buffer
 *
 * @param jpeg_data  Input JPEG data
 * @param jpeg_size  Size of JPEG data in bytes
 * @param out_image  Output image structure
 * @param opts       Load options (NULL for defaults)
 * @return           JPEG_OK on success, error code otherwise
 */
int jpeg_load_from_memory(const uint8_t* jpeg_data, size_t jpeg_size,
                          image_t* out_image, const jpeg_load_options_t* opts);

/**
 * Load JPEG from file
 *
 * @param filename   Path to JPEG file
 * @param out_image  Output image structure
 * @param opts       Load options (NULL for defaults)
 * @return           JPEG_OK on success, error code otherwise
 */
int jpeg_load_from_file(const char* filename, image_t* out_image,
                        const jpeg_load_options_t* opts);

/**
 * Save image to JPEG in memory buffer
 *
 * @param image       Input image
 * @param out_data    Output buffer pointer (allocated by function)
 * @param out_size    Output buffer size
 * @param opts        Save options (NULL for defaults)
 * @return            JPEG_OK on success, error code otherwise
 */
int jpeg_save_to_memory(const image_t* image, uint8_t** out_data,
                        size_t* out_size, const jpeg_save_options_t* opts);

/**
 * Save image to JPEG file
 *
 * @param image       Input image
 * @param filename    Output file path
 * @param opts        Save options (NULL for defaults)
 * @return            JPEG_OK on success, error code otherwise
 */
int jpeg_save_to_file(const image_t* image, const char* filename,
                      const jpeg_save_options_t* opts);

/**
 * Get information about a JPEG without fully decoding it
 *
 * @param jpeg_data   Input JPEG data
 * @param jpeg_size   Size of JPEG data
 * @param width       Output width
 * @param height      Output height
 * @param channels    Output number of channels
 * @param precision   Output data precision (8 or 12)
 * @return            JPEG_OK on success, error code otherwise
 */
int jpeg_get_info(const uint8_t* jpeg_data, size_t jpeg_size,
                  uint32_t* width, uint32_t* height, uint32_t* channels, uint32_t* precision);

/**
 * Create an empty image with specified parameters
 *
 * @param width    image_t width
 * @param height   image_t height
 * @param format   Pixel format
 * @return         Allocated image or NULL on error
 */
image_t* image_create(size_t width, size_t height, pixel_format_t format);

/**
 * Clone an existing image
 *
 * @param src  Source image
 * @return     Cloned image or NULL on error
 */
image_t* image_clone(const image_t* src);

/**
 * Free image resources
 *
 * @param img  image_t to free
 */
void image_free(image_t* img);

/**
 * Get number of bytes per pixel for a format
 *
 * @param format  Pixel format
 * @return        Bytes per pixel
 */
int pixel_format_get_bpp(pixel_format_t format);

/**
 * Get number of channels for a format
 *
 * @param format  Pixel format
 * @return        Number of channels
 */
int pixel_format_get_channels(pixel_format_t format);

/**
 * Get human-readable error message
 *
 * @param error  Error code
 * @return       Error message string
 */
const char* jpeg_error_string(int error);

#endif //VESPERAOS_JPEG_H