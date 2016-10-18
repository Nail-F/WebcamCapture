#include "WebcamCapture.h"

extern "C"
{
#include <libavfilter\buffersrc.h>
#include <libavfilter\buffersink.h>
#include <libavutil\time.h>
}
#include <chrono>
#include <vector>
#include <sstream>

WebcamCapture::WebcamCapture(uint32_t duration_sec, const std::string &output_filename, const std::string &camera_name, const std::string &mic_name)
  : status_(SUCCESS)
  , packet_in_(NULL)
  , packet_out_(NULL)
  , frame_(NULL)
  , ifmt_ctx_(NULL)
  , input_format_(NULL)
  , ofmt_ctx_(NULL)
  , filtered_frame_(NULL)
  , camera_name_(camera_name)
  , mic_name_(mic_name)
  , output_filename_(output_filename)
  , packet_pts_(0)
  , duration_sec_(duration_sec)
{
  av_register_all();
  avfilter_register_all();
  avdevice_register_all();

  if ((open_input_file() < 0) ||
    (open_output_file() < 0) ||
    (init_filters() < 0))
  {
    status_ = INVALID;
  }
}

WebcamCapture::~WebcamCapture()
{
  if (ofmt_ctx_ && status_ == SUCCESS)
  {
    flush_filters();
    av_write_trailer(ofmt_ctx_);
  }
  if (packet_in_)
  {
    av_packet_unref(packet_in_);
  }
  if (packet_out_)
  {
    av_packet_unref(packet_in_);
  }
  if (frame_)
  {
    av_frame_free(&frame_);
  }
  if (filtered_frame_)
  {
    av_frame_free(&filtered_frame_);
  }
  if (ifmt_ctx_)
  {
    for (int i = 0; i < ifmt_ctx_->nb_streams; i++)
    {
//       avcodec_free_context(&stream_ctx_[i].dec_ctx);
//        if (ofmt_ctx_ && ofmt_ctx_->nb_streams > i && ofmt_ctx_->streams[i] && stream_ctx_[i].enc_ctx)
//        {
//          avcodec_free_context(&stream_ctx_[i].enc_ctx);
//        }
      if (filter_ctx_ && filter_ctx_[i].filter_graph)
      {
        avfilter_graph_free(&filter_ctx_[i].filter_graph);
      }
    }
    avformat_close_input(&ifmt_ctx_);
  }
  av_free(filter_ctx_);
  av_free(stream_ctx_);
  if (ofmt_ctx_ && !(ofmt_ctx_->oformat->flags & AVFMT_NOFILE))
  {
    avio_closep(&ofmt_ctx_->pb);
  }
  if (ofmt_ctx_)
  {
    avformat_free_context(ofmt_ctx_);
  }
}

int WebcamCapture::flush_filters()
{
  int ret = 0;
  /* flush filters and encoders */
  for (int i = 0; i < ifmt_ctx_->nb_streams; i++)
  {
    /* flush filter */
    if (!filter_ctx_[i].filter_graph)
    {
      continue;
    }
    ret = filter_encode_write_frame(NULL, i);
    if (ret < 0)
    {
      av_log(NULL, AV_LOG_ERROR, "Flushing filter failed\n");
      break;
    }

    /* flush encoder */
    ret = flush_encoder(i);
    if (ret < 0)
    {
      av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
      break;
    }
  }

  if (ret < 0)
  {
    av_log(NULL, AV_LOG_ERROR, "Error occurred: %d\n", ret);
  }


  return ret;
}

int WebcamCapture::open_input_file()
{
  int ret = 0;

  AVDictionary *av_option = 0;
  av_dict_set(&av_option, "rtbufsize", "1000000000", NULL);
  input_format_ = av_find_input_format("dshow");
  ifmt_ctx_ = avformat_alloc_context();

  std::string device_name = "video=";
  device_name.append(camera_name_);
  if (!mic_name_.empty())
  {
    device_name.append(":audio=");
    device_name.append(mic_name_);
  }
  //test incoming file:
  //device_name = "d:\\WORK\\1.avi"; input_format = NULL;
  if ((ret = avformat_open_input(&ifmt_ctx_, device_name.c_str(), input_format_, &av_option)) < 0)
  {
    av_dict_free(&av_option);
    char buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(ret, buf, sizeof(buf));
    av_log(NULL, AV_LOG_ERROR, "Cannot open input source '%s' with error '%s'\n", device_name.c_str(), buf);
    return ret;
  }
  av_dict_free(&av_option);

  if ((ret = avformat_find_stream_info(ifmt_ctx_, NULL)) < 0)
  {
    av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
    return ret;
  }

  stream_ctx_ = (StreamContext*)av_mallocz_array(ifmt_ctx_->nb_streams, sizeof(*stream_ctx_));
  if (!stream_ctx_)
  {
    return AVERROR(ENOMEM);
  }

  for (int i = 0; i < ifmt_ctx_->nb_streams; i++)
  {
    AVStream *stream = ifmt_ctx_->streams[i];
    AVCodec *dec = avcodec_find_decoder(stream->codec->codec_id);
    AVCodecContext *codec_ctx;
    if (!dec)
    {
      av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream #%u\n", i);
      return AVERROR_DECODER_NOT_FOUND;
    }
    codec_ctx = stream->codec;
    if (!codec_ctx)
    {
      av_log(NULL, AV_LOG_ERROR, "Failed to allocate the decoder context for stream #%u\n", i);
      return AVERROR(ENOMEM);
    }
    //     if ((ret = avcodec_parameters_to_context(codec_ctx, stream->codec)) < 0)
    //     {
    //       av_log(NULL, AV_LOG_ERROR, "Failed to copy decoder parameters to input decoder context "
    //         "for stream #%u\n", i);
    //       return ret;
    //     }
    /* Reencode video & audio and remux subtitles etc. */
    if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
      || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
    {
      if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
      {
        codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx_, stream, NULL);
      }
      /* Open decoder */
      ret = avcodec_open2(codec_ctx, dec, NULL);
      if (ret < 0)
      {
        av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
        return 0;
      }
    }
    stream_ctx_[i].dec_ctx = codec_ctx;
  }

  av_dump_format(ifmt_ctx_, 0, device_name.c_str(), 0);
  return 0;
}

int WebcamCapture::open_output_file()
{
  AVStream *out_stream;
  AVCodecContext *dec_ctx, *enc_ctx;
  AVCodec *encoder;
  int ret;
  unsigned int i;

  ofmt_ctx_ = NULL;
  avformat_alloc_output_context2(&ofmt_ctx_, NULL, NULL, output_filename_.c_str());
  if (!ofmt_ctx_)
  {
    av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
    return AVERROR_UNKNOWN;
  }
                                      
  for (i = 0; i < ifmt_ctx_->nb_streams; i++)
  {
    out_stream = avformat_new_stream(ofmt_ctx_, NULL);
    if (!out_stream) {
      av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
      return AVERROR_UNKNOWN;
    }

    dec_ctx = stream_ctx_[i].dec_ctx;

    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
      || dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
    {
      /* in this example, we choose transcoding to same codec */
      encoder = avcodec_find_encoder(dec_ctx->codec_id);
      if (!encoder) {
        av_log(NULL, AV_LOG_FATAL, "Necessary encoder not found\n");
        return AVERROR_INVALIDDATA;
      }
      enc_ctx = out_stream->codec;

      if (!enc_ctx)
      {
        av_log(NULL, AV_LOG_FATAL, "Failed to allocate the encoder context\n");
        return AVERROR(ENOMEM);
      }
      /* In this example, we transcode to same properties (picture size,
      * sample rate etc.). These properties can be changed for output
      * streams easily using filters */
      if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        enc_ctx->height = dec_ctx->height;
        enc_ctx->width = dec_ctx->width;
        enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
        /* take first format from list of supported formats */
        if (encoder->pix_fmts)
        {
          enc_ctx->pix_fmt = encoder->pix_fmts[0];
        }
        else
        {
          enc_ctx->pix_fmt = dec_ctx->pix_fmt;
        }
        /* video time_base can be set to whatever is handy and supported by encoder */
        enc_ctx->time_base = av_inv_q(dec_ctx->framerate);
      } else {
        enc_ctx->sample_rate = dec_ctx->sample_rate;
        if (dec_ctx->channels && !dec_ctx->channel_layout)
        {
          enc_ctx->channels = dec_ctx->channels;
          enc_ctx->channel_layout = av_get_default_channel_layout(enc_ctx->channels);
        }
        else if (dec_ctx->channel_layout)
        {
          enc_ctx->channels = av_get_channel_layout_nb_channels(dec_ctx->channel_layout);
          enc_ctx->channel_layout = dec_ctx->channel_layout;
        }
        else
        {
          enc_ctx->channel_layout = AV_CH_LAYOUT_MONO;
          enc_ctx->channels =  1;
        }
        /* take first format from list of supported formats */
        enc_ctx->sample_fmt = encoder->sample_fmts[0];
        enc_ctx->time_base.num = 1;
        enc_ctx->time_base.den = enc_ctx->sample_rate;
      }

      /* Third parameter can be used to pass settings to encoder */
      ret = avcodec_open2(enc_ctx, encoder, NULL);
      if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream #%u\n", i);
        return ret;
      }
      //         if ((ret = avcodec_parameters_from_context(out_stream->codec, enc_ctx)) < 0) {
      //           av_log(NULL, AV_LOG_ERROR, "Failed to copy encoder parameters to output stream #%u\n", i);
      //           return ret;
      //         }
      if (ofmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER)
      {
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
      }
      out_stream->time_base = enc_ctx->time_base;
      stream_ctx_[i].enc_ctx = enc_ctx;
    } else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN) {
      av_log(NULL, AV_LOG_FATAL, "Elementary stream #%d is of unknown type, cannot proceed\n", i);
      status_ = INVALID;
      return AVERROR_INVALIDDATA;
    } else {
      /* if this stream must be remuxed */
      //       ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
      //       if (ret < 0) {
      //         av_log(NULL, AV_LOG_ERROR, "Copying parameters for stream #%u failed\n", i);
      //         return ret;
      //       }
    }
  }
  av_dump_format(ofmt_ctx_, 0, output_filename_.c_str(), 1);

  if (!(ofmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
    ret = avio_open(&ofmt_ctx_->pb, output_filename_.c_str(), AVIO_FLAG_WRITE);
    if (ret < 0) {
      av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", output_filename_.c_str());
      return ret;
    }
  }

  /* init muxer, write output file header */
  ret = avformat_write_header(ofmt_ctx_, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
    return ret;
  }

  return 0;
}

int WebcamCapture::init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx, AVCodecContext *enc_ctx, const char *filter_spec)
{
  char args[512];
  int ret = 0;
  AVFilter *buffersrc = NULL;
  AVFilter *buffersink = NULL;
  AVFilterContext *buffersrc_ctx = NULL;
  AVFilterContext *buffersink_ctx = NULL;
  AVFilterInOut *outputs = avfilter_inout_alloc();
  AVFilterInOut *inputs  = avfilter_inout_alloc();
  AVFilterGraph *filter_graph = avfilter_graph_alloc();

  if (!outputs || !inputs || !filter_graph)
  {
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
  {
    buffersrc = avfilter_get_by_name("buffer");
    buffersink = avfilter_get_by_name("buffersink");
    if (!buffersrc || !buffersink)
    {
      av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
      ret = AVERROR_UNKNOWN;
      goto end;
    }

    sprintf_s(args, sizeof(args),
      "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
      dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
      dec_ctx->time_base.num, dec_ctx->time_base.den,
      dec_ctx->sample_aspect_ratio.num,
      dec_ctx->sample_aspect_ratio.den);

    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
      args, NULL, filter_graph);
    if (ret < 0)
    {
      av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
      goto end;
    }

    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
      NULL, NULL, filter_graph);
    if (ret < 0)
    {
      av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
      goto end;
    }

    ret = av_opt_set_bin(buffersink_ctx, "pix_fmts",
      (uint8_t*)&enc_ctx->pix_fmt, sizeof(enc_ctx->pix_fmt),
      AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
      av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
      goto end;
    }
  } 
  else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
  {
    buffersrc = avfilter_get_by_name("abuffer");
    buffersink = avfilter_get_by_name("abuffersink");
    if (!buffersrc || !buffersink)
    {
      av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
      ret = AVERROR_UNKNOWN;
      goto end;
    }

    if (!dec_ctx->channel_layout)
    {
      dec_ctx->channel_layout = av_get_default_channel_layout(dec_ctx->channels);
    }
    sprintf_s(args, sizeof(args),
      "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%""I64x",
      dec_ctx->time_base.num, dec_ctx->time_base.den, dec_ctx->sample_rate,
      av_get_sample_fmt_name(dec_ctx->sample_fmt),
      dec_ctx->channel_layout);
    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
      args, NULL, filter_graph);
    if (ret < 0)
    {
      av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
      goto end;
    }

    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
      NULL, NULL, filter_graph);
    if (ret < 0)
    {
      av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
      goto end;
    }

    ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
      (uint8_t*)&enc_ctx->sample_fmt, sizeof(enc_ctx->sample_fmt),
      AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
      av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
      goto end;
    }

    ret = av_opt_set_bin(buffersink_ctx, "channel_layouts",
      (uint8_t*)&enc_ctx->channel_layout,
      sizeof(enc_ctx->channel_layout), AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
      av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
      goto end;
    }

    ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
      (uint8_t*)&enc_ctx->sample_rate, sizeof(enc_ctx->sample_rate),
      AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
      av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
      goto end;
    }
  }
  else
  {
    ret = AVERROR_UNKNOWN;
    goto end;
  }

  /* Endpoints for the filter graph. */
  outputs->name       = av_strdup("in");
  outputs->filter_ctx = buffersrc_ctx;
  outputs->pad_idx    = 0;
  outputs->next       = NULL;

  inputs->name       = av_strdup("out");
  inputs->filter_ctx = buffersink_ctx;
  inputs->pad_idx    = 0;
  inputs->next       = NULL;

  if (!outputs->name || !inputs->name)
  {
    ret = AVERROR(ENOMEM);
    goto end;
  }

  if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec,
    &inputs, &outputs, NULL)) < 0)
  {
    goto end;
  }

  if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
  {
    goto end;
  }

  /* Fill FilteringContext */
  fctx->buffersrc_ctx = buffersrc_ctx;
  fctx->buffersink_ctx = buffersink_ctx;
  fctx->filter_graph = filter_graph;

end:
  avfilter_inout_free(&inputs);
  avfilter_inout_free(&outputs);

  return ret;
}

int WebcamCapture::init_filters()
{
  const char *filter_spec;
  unsigned int i;
  int ret;
  filter_ctx_ = (FilteringContext*)av_malloc_array(ifmt_ctx_->nb_streams, sizeof(*filter_ctx_));
  if (!filter_ctx_)
  {
    return AVERROR(ENOMEM);
  }

  for (i = 0; i < ifmt_ctx_->nb_streams; i++) {
    filter_ctx_[i].buffersrc_ctx  = NULL;
    filter_ctx_[i].buffersink_ctx = NULL;
    filter_ctx_[i].filter_graph   = NULL;
    if (!(ifmt_ctx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO
      || ifmt_ctx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO))
    {
      continue;
    }

    if (ifmt_ctx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      filter_spec = "null"; /* passthrough (dummy) filter for video */
    }
    else
    {
      filter_spec = "anull"; /* passthrough (dummy) filter for audio */
    }
    ret = init_filter(&filter_ctx_[i], /*ifmt_ctx_->streams[i]->codec*/ stream_ctx_[i].dec_ctx,
      /*ofmt_ctx_->streams[i]->codec*/stream_ctx_[i].enc_ctx, filter_spec);
    if (ret)
    {
      return ret;
    }
  }
  return 0;
}

int WebcamCapture::Work()
{
  int ret = 0;
  int frame_decoded = 0;
  packet_in_ = new AVPacket;
  av_init_packet(packet_in_);

  av_log(NULL, AV_LOG_INFO, "Start capture the frames!\n");

  auto now = std::chrono::steady_clock::now();
  auto start = now;
  auto until = now + std::chrono::seconds(duration_sec_);

  int cnt_in = 0;
  auto one_second = now + std::chrono::seconds(1);
  while (now < until)
  {
    now = std::chrono::steady_clock::now();

    //show process
    if (now > one_second)
    {
      av_log(NULL, AV_LOG_INFO, ".");
      one_second = now + std::chrono::seconds(1);
    }

    if ((ret = av_read_frame(ifmt_ctx_, packet_in_)) < 0)
    {
      break;
    }
    int stream_index = packet_in_->stream_index;

    AVMediaType type = ifmt_ctx_->streams[stream_index]->codec->codec_type;
    av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n",
      stream_index);

    if (filter_ctx_[stream_index].filter_graph)
    {
      av_log(NULL, AV_LOG_DEBUG, "Going to reencode & filter the frame\n");
      frame_ = av_frame_alloc();
      if (!frame_)
      {
        ret = AVERROR(ENOMEM);
        break;
      }



      packet_in_->dts = packet_in_->pts = cnt_in;

      auto delta = (now - start).count();
      packet_in_->dts = packet_in_->pts = delta;
      av_packet_rescale_ts(packet_in_,
      ifmt_ctx_->streams[stream_index]->time_base,
      ifmt_ctx_->streams[stream_index]->codec->time_base);

      if (type == AVMEDIA_TYPE_VIDEO)
      {
        ++cnt_in;
      }
      dec_func_ptr dec_func = (type == AVMEDIA_TYPE_VIDEO) ? avcodec_decode_video2 : avcodec_decode_audio4;
      ret = dec_func(stream_ctx_[stream_index].dec_ctx, frame_, &frame_decoded, packet_in_);

      if (ret < 0)
      {
        av_frame_free(&frame_);
        av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
        break;
      }

      if (frame_decoded)
      {
        frame_->pts = av_frame_get_best_effort_timestamp(frame_);
        ret = filter_encode_write_frame(frame_, stream_index);
        av_frame_free(&frame_);
        if (ret < 0)
        {
          break;
        }
      }
      else
      {
        av_frame_free(&frame_);
      }
    }
    else
    {
      /* remux this frame without reencoding */
      av_packet_rescale_ts(packet_in_,
        ifmt_ctx_->streams[stream_index]->time_base,
        ofmt_ctx_->streams[stream_index]->time_base);

      ret = av_interleaved_write_frame(ofmt_ctx_, packet_in_);
      if (ret < 0)
      {
        break;
      }
    }
    av_packet_unref(packet_in_);
  }
  av_log(NULL, AV_LOG_INFO, "\nStop!\n");

  ret = flush_filters();

  if (ret < 0)
  {
    av_log(NULL, AV_LOG_ERROR, "Error occurred: %d\n", ret);
  }
  status_ = (ret < 0) ? INVALID : SUCCESS;
  return ret;
}

int cnt_out = 0;
int WebcamCapture::encode_write_frame(AVFrame *filtered_frame, unsigned int stream_index, int *frame_decoded)
{
  int ret = 0;
  int frame_decoded_local = 0;
  AVMediaType type = ifmt_ctx_->streams[stream_index]->codec->codec_type;
  enc_func_ptr enc_func = (type == AVMEDIA_TYPE_VIDEO) ? avcodec_encode_video2 : avcodec_encode_audio2;

  if (!frame_decoded)
  {
    frame_decoded = &frame_decoded_local;
  }

  av_log(NULL, AV_LOG_DEBUG, "Encoding frame\n");
  /* encode filtered frame */
  packet_out_ = new AVPacket;
  av_init_packet(packet_out_);
  packet_out_->data = NULL;
  packet_out_->size = 0;
  ret = enc_func(stream_ctx_[stream_index].enc_ctx, packet_out_, filtered_frame, frame_decoded);

  packet_out_->pts = filtered_frame->pkt_pts;
  packet_out_->dts = filtered_frame->pkt_dts;
  av_frame_free(&filtered_frame);
  if (ret < 0)
  {
    return ret;
  }
  if (!(*frame_decoded))
  {
    return 0;
  }

  /* prepare packet for muxing */
  packet_out_->stream_index = stream_index;
  av_packet_rescale_ts(packet_out_,
                       ofmt_ctx_->streams[stream_index]->codec->time_base,
                       ofmt_ctx_->streams[stream_index]->time_base);

  //av_log(NULL, AV_LOG_INFO, "%3.0d) out_packet[%d] %lld, %lld\n", cnt_out, stream_index, out_packet.pts, out_packet.dts);
  
  av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");
  if (type == AVMEDIA_TYPE_VIDEO)
  {
    ++cnt_out;
  }

  /* mux encoded frame */
  ret = av_interleaved_write_frame(ofmt_ctx_, packet_out_);

  av_packet_unref(packet_out_);
  return ret;
}

int WebcamCapture::filter_encode_write_frame(AVFrame *frame, unsigned int stream_index)
{
  int ret;

  av_log(NULL, AV_LOG_DEBUG, "Pushing decoded frame to filters\n");
  /* push the decoded frame into the filtergraph */
  ret = av_buffersrc_add_frame_flags(filter_ctx_[stream_index].buffersrc_ctx,
    frame, 0);
  if (ret < 0)
  {
    av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
    return ret;
  }

  /* pull filtered frames from the filtergraph */
  while (1)
  {
    filtered_frame_ = av_frame_alloc();
    if (!filtered_frame_)
    {
      ret = AVERROR(ENOMEM);
      break;
    }
    av_log(NULL, AV_LOG_DEBUG, "Pulling filtered frame from filters\n");
    ret = av_buffersink_get_frame(filter_ctx_[stream_index].buffersink_ctx,
      filtered_frame_);
    if (ret < 0)
    {
      /* if no more frames for output - returns AVERROR(EAGAIN)
      * if flushed and no more frames for output - returns AVERROR_EOF
      * rewrite retcode to 0 to show it as normal procedure completion
      */
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      {
        ret = 0;
      }
      av_frame_free(&filtered_frame_);
      break;
    }

    filtered_frame_->pict_type = AV_PICTURE_TYPE_NONE;
    ret = encode_write_frame(filtered_frame_, stream_index, NULL);
    if (ret < 0)
    {
      break;
    }
  }

  return ret;
}

int WebcamCapture::flush_encoder(unsigned int stream_index)
{
  int ret;
  int got_frame;

  if (!(stream_ctx_[stream_index].enc_ctx->codec->capabilities &
    AV_CODEC_CAP_DELAY))
  {
    return 0;
  }

  av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);
  while (1)
  {
    ret = encode_write_frame(NULL, stream_index, &got_frame);
    if (ret < 0)
    {
      break;
    }
    if (!got_frame)
    {
      return 0;
    }
  }
  return ret;
}
