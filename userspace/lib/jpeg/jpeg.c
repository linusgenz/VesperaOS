/**
 * @file jpeg.c
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

#include <fflags.h>
#include <jerror.h>
#include <jpeg/jpeg.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    jmp_buf env;
    int error_code;
} jpeg_error_context_t;

static void jpeg_output_message(j_common_ptr cinfo)
{
    //printf("jpeg_output_message");
   // char buffer[JMSG_LENGTH_MAX];

   // (*cinfo->err->format_message)(cinfo, buffer);
   // printf("[libjpeg] %s\n", buffer);
}

static void jpeg_error_exit(j_common_ptr cinfo)
{
    printf("jpeg_error_exit");
    struct jpeg_error_mgr* err = cinfo->err;
    err->output_message = jpeg_output_message;

    (*err->output_message)(cinfo);

    jpeg_error_context_t* ctx = cinfo->client_data;

    longjmp(ctx->env, 1);
}

static void jpeg_emit_message_stub(j_common_ptr cinfo, int msg_level)
{
}

static void jpeg_emit_message(j_common_ptr cinfo, int msg_level)
{
    struct jpeg_error_handler* err = (struct jpeg_error_handler*)cinfo->err;
    char buffer[JMSG_LENGTH_MAX];
  //  (*cinfo->err->format_message)(cinfo, buffer);

   // if (msg_level < 0)
    //    printf("[libjpeg warning] %s\n", buffer);
  //  else
    //    printf("[libjpeg info] %s\n", buffer);
}

// Utility functions

int pixel_format_get_bpp(pixel_format_t format)
{
    switch (format)
    {
    case PIXEL_FORMAT_RGB:
    case PIXEL_FORMAT_BGR:
        return 3;
    case PIXEL_FORMAT_RGBA:
    case PIXEL_FORMAT_BGRA:
    case PIXEL_FORMAT_BGRX:
        return 4;
    case PIXEL_FORMAT_GRAY:
        return 1;
    case PIXEL_FORMAT_GRAY_ALPHA:
        return 2;
    default:
        return 0;
    }
}

int pixel_format_get_channels(pixel_format_t format)
{
    switch (format)
    {
    case PIXEL_FORMAT_RGB:
    case PIXEL_FORMAT_BGR:
        return 3;
    case PIXEL_FORMAT_RGBA:
    case PIXEL_FORMAT_BGRA:
    case PIXEL_FORMAT_BGRX:
        return 4;
    case PIXEL_FORMAT_GRAY:
        return 1;
    case PIXEL_FORMAT_GRAY_ALPHA:
        return 2;
    default:
        return 0;
    }
}

const char* jpeg_error_string(int error)
{
    switch (error)
    {
    case JPEG_OK:
        return "Success";
    case JPEG_ERROR_INVALID_PARAM:
        return "Invalid parameter";
    case JPEG_ERROR_MEMORY:
        return "Memory allocation failed";
    case JPEG_ERROR_FILE_OPEN:
        return "Failed to open file";
    case JPEG_ERROR_FILE_READ:
        return "Failed to read file";
    case JPEG_ERROR_FILE_WRITE:
        return "Failed to write file";
    case JPEG_ERROR_DECODE:
        return "JPEG decode error";
    case JPEG_ERROR_ENCODE:
        return "JPEG encode error";
    case JPEG_ERROR_INVALID_FORMAT:
        return "Invalid format";
    case JPEG_ERROR_UNSUPPORTED:
        return "Unsupported operation";
    default:
        return "Unknown error";
    }
}

void jpeg_load_options_init(jpeg_load_options_t* opts)
{
    if (!opts) return;
    opts->output_format = PIXEL_FORMAT_BGRA;
    opts->fast_dct = false;
    opts->do_fancy_upsampling = true;
    opts->scale_num = 1;
    opts->scale_denom = 1;
}

void jpeg_save_options_init(jpeg_save_options_t* opts)
{
    if (!opts) return;
    opts->quality = 100;
    opts->progressive = false;
    opts->optimize_coding = true;
    opts->smoothing_factor = 0;
    opts->h_samp_factor = 2;
    opts->v_samp_factor = 2;
    opts->data_precision = 8;
}

// image_t management

image_t* image_create(size_t width, size_t height, pixel_format_t format)
{
    if (width == 0 || height == 0) return NULL;

    image_t* img = (image_t*)malloc(sizeof(image_t));
    if (!img) return NULL;

    img->width = width;
    img->height = height;
    img->format = format;
    img->channels = pixel_format_get_channels(format);
    img->precision = 8;
    img->stride = width * pixel_format_get_bpp(format);
    img->size = img->stride * img->height;
    img->data = (uint8_t*)malloc(img->stride * height);

    if (!img->data)
    {
        free(img);
        return NULL;
    }

    memset(img->data, 0, img->stride * height);
    return img;
}

image_t* image_clone(const image_t* src)
{
    if (!src || !src->data) return NULL;

    image_t* clone = image_create(src->width, src->height, src->format);
    if (!clone) return NULL;

    clone->precision = src->precision;
    clone->size = src->size;
    memcpy(clone->data, src->data, src->stride * src->height);

    return clone;
}

void image_free(image_t* img)
{
    if (!img) return;
    free(img->data);
    free(img);
}

// Color conversion

static void convert_rgb_to_format(const uint8_t* src, uint8_t* dst,
                                  size_t width, pixel_format_t format)
{
    size_t x;

    switch (format)
    {
    case PIXEL_FORMAT_RGB:
        memcpy(dst, src, width * 3);
        break;

    case PIXEL_FORMAT_RGBA:
        for (x = 0; x < width; x++)
        {
            dst[4 * x + 0] = src[3 * x + 0]; // R
            dst[4 * x + 1] = src[3 * x + 1]; // G
            dst[4 * x + 2] = src[3 * x + 2]; // B
            dst[4 * x + 3] = 255; // A
        }
        break;

    case PIXEL_FORMAT_BGR:
        for (x = 0; x < width; x++)
        {
            dst[3 * x + 0] = src[3 * x + 2]; // B
            dst[3 * x + 1] = src[3 * x + 1]; // G
            dst[3 * x + 2] = src[3 * x + 0]; // R
        }
        break;

    case PIXEL_FORMAT_BGRA:
        for (x = 0; x < width; x++)
        {
            dst[4 * x + 0] = src[3 * x + 2]; // B
            dst[4 * x + 1] = src[3 * x + 1]; // G
            dst[4 * x + 2] = src[3 * x + 0]; // R
            dst[4 * x + 3] = 255; // A
        }
        break;

    case PIXEL_FORMAT_BGRX:
        for (x = 0; x < width; x++)
        {
            dst[4 * x + 0] = src[3 * x + 2]; // B
            dst[4 * x + 1] = src[3 * x + 1]; // G
            dst[4 * x + 2] = src[3 * x + 0]; // R
            dst[4 * x + 3] = 0; // X
        }
        break;

    case PIXEL_FORMAT_GRAY:
        for (x = 0; x < width; x++)
        {
            // ITU-R BT.601 luminance formula
            dst[x] = (uint8_t)((299 * src[3 * x + 0] + 587 * src[3 * x + 1] + 114 * src[3 * x + 2] + 500) / 1000);
        }
        break;

    case PIXEL_FORMAT_GRAY_ALPHA:
        for (x = 0; x < width; x++)
        {
            dst[x] = (uint8_t)((299 * src[3 * x + 0] + 587 * src[3 * x + 1] + 114 * src[3 * x + 2] + 500) / 1000);
            dst[2 * x + 1] = 255; // A
        }
        break;
    }
}

static void convert_gray_to_format(const uint8_t* src, uint8_t* dst,
                                   size_t width, pixel_format_t format)
{
    size_t x;
    uint8_t gray;

    switch (format)
    {
    case PIXEL_FORMAT_RGB:
        for (x = 0; x < width; x++)
        {
            gray = src[x];
            dst[3 * x + 0] = gray;
            dst[3 * x + 1] = gray;
            dst[3 * x + 2] = gray;
        }
        break;

    case PIXEL_FORMAT_RGBA:
    case PIXEL_FORMAT_BGRA:
    case PIXEL_FORMAT_BGRX:
        for (x = 0; x < width; x++)
        {
            gray = src[x];
            dst[4 * x + 0] = gray;
            dst[4 * x + 1] = gray;
            dst[4 * x + 2] = gray;
            dst[4 * x + 3] = (format == PIXEL_FORMAT_BGRX) ? 0 : 255;
        }
        break;

    case PIXEL_FORMAT_BGR:
        for (x = 0; x < width; x++)
        {
            gray = src[x];
            dst[3 * x + 0] = gray;
            dst[3 * x + 1] = gray;
            dst[3 * x + 2] = gray;
        }
        break;

    case PIXEL_FORMAT_GRAY:
        memcpy(dst, src, width);
        break;

    case PIXEL_FORMAT_GRAY_ALPHA:
        for (x = 0; x < width; x++)
        {
            dst[2 * x + 0] = src[x];
            dst[2 * x + 1] = 255;
        }
        break;
    }
}

static void convert_format_to_rgb(const uint8_t* src, uint8_t* dst,
                                  size_t width, pixel_format_t format)
{
    size_t x;

    switch (format)
    {
    case PIXEL_FORMAT_RGB:
        memcpy(dst, src, width * 3);
        break;

    case PIXEL_FORMAT_RGBA:
        for (x = 0; x < width; x++)
        {
            dst[3 * x + 0] = src[4 * x + 0];
            dst[3 * x + 1] = src[4 * x + 1];
            dst[3 * x + 2] = src[4 * x + 2];
        }
        break;

    case PIXEL_FORMAT_BGR:
        for (x = 0; x < width; x++)
        {
            dst[3 * x + 0] = src[3 * x + 2];
            dst[3 * x + 1] = src[3 * x + 1];
            dst[3 * x + 2] = src[3 * x + 0];
        }
        break;

    case PIXEL_FORMAT_BGRA:
    case PIXEL_FORMAT_BGRX:
        for (x = 0; x < width; x++)
        {
            dst[3 * x + 0] = src[4 * x + 2];
            dst[3 * x + 1] = src[4 * x + 1];
            dst[3 * x + 2] = src[4 * x + 0];
        }
        break;

    case PIXEL_FORMAT_GRAY:
        for (x = 0; x < width; x++)
        {
            dst[3 * x + 0] = src[x];
            dst[3 * x + 1] = src[x];
            dst[3 * x + 2] = src[x];
        }
        break;

    case PIXEL_FORMAT_GRAY_ALPHA:
        for (x = 0; x < width; x++)
        {
            dst[3 * x + 0] = src[2 * x + 0];
            dst[3 * x + 1] = src[2 * x + 0];
            dst[3 * x + 2] = src[2 * x + 0];
        }
        break;
    }
}

// Jpeg loading

int jpeg_get_info(const uint8_t* jpeg_data, size_t jpeg_size,
                  uint32_t* width, uint32_t* height, uint32_t* channels, uint32_t* precision)
{
    if (!jpeg_data || jpeg_size == 0) return JPEG_ERROR_INVALID_PARAM;

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    jpeg_error_context_t ctx;

    cinfo.client_data = &ctx;
    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = jpeg_error_exit;
    jerr.emit_message = jpeg_emit_message;

    if (setjmp(ctx.env))
    {
        jpeg_destroy_decompress(&cinfo);
        return JPEG_ERROR_DECODE;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, jpeg_data, jpeg_size);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK)
    {
        jpeg_destroy_decompress(&cinfo);
        return JPEG_ERROR_DECODE;
    }

    if (width) *width = cinfo.image_width;
    if (height) *height = cinfo.image_height;
    if (channels) *channels = cinfo.num_components;
    if (precision) *precision = cinfo.data_precision;

    jpeg_destroy_decompress(&cinfo);
    return JPEG_OK;
}

int jpeg_load_from_memory(const uint8_t* jpeg_data, size_t jpeg_size,
                          image_t* out_image, const jpeg_load_options_t* opts)
{
    if (!jpeg_data || jpeg_size == 0 || !out_image)
        return JPEG_ERROR_INVALID_PARAM;

    jpeg_load_options_t default_opts;
    if (!opts)
    {
        jpeg_load_options_init(&default_opts);
        opts = &default_opts;
    }

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    jpeg_error_context_t ctx;
    JSAMPARRAY buffer = NULL;
    uint32_t row_stride;

    cinfo.client_data = &ctx;
    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = jpeg_error_exit;
    jerr.emit_message = jpeg_emit_message_stub;

    int r = setjmp(ctx.env);
    if (r)
    {
        jpeg_destroy_decompress(&cinfo);
        return JPEG_ERROR_DECODE;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, jpeg_data, jpeg_size);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK)
    {
        jpeg_destroy_decompress(&cinfo);
        return JPEG_ERROR_DECODE;
    }

    // Set decompression parameters
    cinfo.dct_method = opts->fast_dct ? JDCT_IFAST : JDCT_ISLOW;
    cinfo.do_fancy_upsampling = opts->do_fancy_upsampling ? TRUE : FALSE;
    cinfo.scale_num = opts->scale_num;
    cinfo.scale_denom = opts->scale_denom;

    // Determine output color space
    if (cinfo.num_components == 1)
    {
        cinfo.out_color_space = JCS_GRAYSCALE;
    }
    else
    {
        cinfo.out_color_space = JCS_RGB;
    }

    if (!jpeg_start_decompress(&cinfo))
    {
        jpeg_destroy_decompress(&cinfo);
        return JPEG_ERROR_DECODE;
    }

    // Allocate output image
    out_image->width = cinfo.output_width;
    out_image->height = cinfo.output_height;
    out_image->format = opts->output_format;
    out_image->channels = pixel_format_get_channels(opts->output_format);
    out_image->precision = cinfo.data_precision;
    out_image->stride = out_image->width * pixel_format_get_bpp(opts->output_format);
    out_image->size = out_image->stride * out_image->height;
    out_image->data = (uint8_t*)malloc(out_image->stride * out_image->height);

    if (!out_image->data)
    {
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        return JPEG_ERROR_MEMORY;
    }

    // Allocate scanline buffer
    row_stride = cinfo.output_width * cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)
        ((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    // Read scanlines and convert
    while (cinfo.output_scanline < cinfo.output_height)
    {
        jpeg_read_scanlines(&cinfo, buffer, 1);

        uint8_t* dst = out_image->data +
            (cinfo.output_scanline - 1) * out_image->stride;

        if (cinfo.out_color_space == JCS_GRAYSCALE)
        {
            convert_gray_to_format(buffer[0], dst, out_image->width,
                                   opts->output_format);
        }
        else
        {
            convert_rgb_to_format(buffer[0], dst, out_image->width,
                                  opts->output_format);
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return JPEG_OK;
}

int jpeg_load_from_file(const char* filename, image_t* out_image,
                        const jpeg_load_options_t* opts)
{
    if (!filename || !out_image) return JPEG_ERROR_INVALID_PARAM;

    FILE* fp = fopen(filename, "r");
    if (fp == NULL) return JPEG_ERROR_FILE_OPEN;
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    printf("size %u\n", size);

    uint8_t* buffer = malloc(size);

    if (!buffer)
    {
        fclose(fp);
        return JPEG_ERROR_MEMORY;
    }

    if ((size_t)fread(buffer, 1, size, fp) != size)
    {
        free(buffer);
        fclose(fp);
        return JPEG_ERROR_FILE_READ;
    }
    fclose(fp);

    int result = jpeg_load_from_memory(buffer, size, out_image, opts);
    free(buffer);

    return result;
}

// Jpeg saving

int jpeg_save_to_memory(const image_t* image, uint8_t** out_data,
                        size_t* out_size, const jpeg_save_options_t* opts)
{
    if (!image || !image->data || !out_data || !out_size)
        return JPEG_ERROR_INVALID_PARAM;

    jpeg_save_options_t default_opts;
    if (!opts)
    {
        jpeg_save_options_init(&default_opts);
        opts = &default_opts;
    }

    if (opts->data_precision != 8 && opts->data_precision != 12)
        return JPEG_ERROR_INVALID_PARAM;

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    jpeg_error_context_t ctx;
    JSAMPROW row_pointer[1];
    uint8_t* rgb_buffer = NULL;
    unsigned char* jpeg_buffer = NULL;
    unsigned long jpeg_size = 0;

    cinfo.client_data = &ctx;
    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = jpeg_error_exit;
    jerr.emit_message = jpeg_emit_message_stub;
    jerr.output_message = jpeg_output_message;

    if (setjmp(ctx.env))
    {
        if (jpeg_buffer) free(jpeg_buffer);
        if (rgb_buffer) free(rgb_buffer);
        jpeg_destroy_compress(&cinfo);
        return JPEG_ERROR_ENCODE;
    }

    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, &jpeg_buffer, &jpeg_size);

    // Set compression parameters
    cinfo.image_width = image->width;
    cinfo.image_height = image->height;
    cinfo.input_components = 3; // Always convert to RGB for encoding
    cinfo.in_color_space = JCS_RGB;
    cinfo.data_precision = opts->data_precision;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, opts->quality, TRUE);

    if (opts->progressive)
        jpeg_simple_progression(&cinfo);

    cinfo.optimize_coding = opts->optimize_coding ? TRUE : FALSE;
    cinfo.smoothing_factor = opts->smoothing_factor;
    cinfo.comp_info[0].h_samp_factor = opts->h_samp_factor;
    cinfo.comp_info[0].v_samp_factor = opts->v_samp_factor;

    jpeg_start_compress(&cinfo, TRUE);

    // Allocate RGB conversion buffer if needed
    if (image->format != PIXEL_FORMAT_RGB)
    {
        rgb_buffer = (uint8_t*)malloc(image->width * 3);
        if (!rgb_buffer)
        {
            jpeg_destroy_compress(&cinfo);
            return JPEG_ERROR_MEMORY;
        }
    }

    // Write scanlines
    for (size_t y = 0; y < image->height; y++)
    {
        const uint8_t* src = image->data + y * image->stride;

        if (image->format == PIXEL_FORMAT_RGB)
        {
            row_pointer[0] = (JSAMPROW)src;
        }
        else
        {
            convert_format_to_rgb(src, rgb_buffer, image->width, image->format);
            row_pointer[0] = rgb_buffer;
        }

        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    if (rgb_buffer) free(rgb_buffer);

    *out_data = jpeg_buffer;
    *out_size = jpeg_size;

    return JPEG_OK;
}

int jpeg_save_to_file(const image_t* image, const char* filename,
                      const jpeg_save_options_t* opts)
{
    if (!image || !filename) return JPEG_ERROR_INVALID_PARAM;

    uint8_t* buffer = NULL;
    size_t size = 0;

    int result = jpeg_save_to_memory(image, &buffer, &size, opts);
    if (result != JPEG_OK) return result;

    HANDLE hdl = open(filename, O_WRONLY | O_CREAT);
    if (hdl < 0)
    {
        free(buffer);
        return JPEG_ERROR_FILE_OPEN;
    }

    printf("writing to file %ld", size);
    size_t res = (size_t)write(hdl, buffer, size);
    if ( res != size)
    {
        printf("%ld %ld", res, size);
        free(buffer);
        close(hdl);
        return JPEG_ERROR_FILE_WRITE;
    }

    close(hdl);
    free(buffer);

    return JPEG_OK;
}
