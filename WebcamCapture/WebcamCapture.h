#pragma once

extern "C"
{
  #define __STDC_CONSTANT_MACROS
  #include <libavdevice\avdevice.h>
  #include <libavfilter\avfilter.h>
}

#include <string>
#include <xutility>
#include "Noncopyable.h"


class WebcamCapture : Noncopyable
{
public:
  WebcamCapture(uint32_t duration_sec, const std::string &output_filename, const std::string &camera_name, const std::string &mic_name = std::string());
  ~WebcamCapture();

  int Work();

  enum status
  {
    SUCCESS,
    INVALID
  };
  status Status() const { return status_; };
 
 private:
   typedef struct FilteringContext
   {
     AVFilterContext *buffersink_ctx;
     AVFilterContext *buffersrc_ctx;
     AVFilterGraph   *filter_graph;
   } FilteringContext;
 
   typedef struct StreamContext
   {
     AVCodecContext *dec_ctx;
     AVCodecContext *enc_ctx;
   } StreamContext;
 
   typedef int (*dec_func_ptr)(AVCodecContext *, AVFrame *, int *, const AVPacket *);
   typedef int (*enc_func_ptr)(AVCodecContext *, AVPacket *, const AVFrame *, int *);
 
   int flush_filters();
   int open_input_file();
   int open_output_file();
   int init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx,
     AVCodecContext *enc_ctx, const char *filter_spec);
   int init_filters();
   int encode_write_frame(AVFrame *filtered_frame, unsigned int stream_index, int *frame_decoded);
   int filter_encode_write_frame(AVFrame *frame, unsigned int stream_index);
   int flush_encoder(unsigned int stream_index);
 
 private:
   AVPacket         *packet_;
   AVFrame          *frame_;
   AVFormatContext  *ifmt_ctx_;
   AVInputFormat    *input_format_;
   AVFormatContext  *ofmt_ctx_;
   FilteringContext *filter_ctx_;
   StreamContext    *stream_ctx_;
 
   status status_;
 
   std::string camera_name_;
   std::string mic_name_;
   std::string output_filename_;
   uint32_t packet_pts_;
   uint32_t duration_sec_;
};

