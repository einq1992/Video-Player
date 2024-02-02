
#include <stdio.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>

int main(int argc, char *argv[]) {
    AVFormatContext *pFormatCtx = NULL;

    avformat_open_input(&pFormatCtx, "../resources/videos/rose.mp4", NULL, NULL);
	avformat_find_stream_info(pFormatCtx, NULL);
    av_dump_format(pFormatCtx, 0, "../resources/videos/rose.mp4", 0);

	AVCodec *video_decoder, *audio_decoder;
    int video_index = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &video_decoder, 0);
    int audio_index = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &audio_decoder, 0);
    if (video_index < 0 && audio_decoder < 0) printf("No video or audio stream found!! ..\n");

    AVCodecContext *videoDecoderCtx = avcodec_alloc_context3(video_decoder);
	avcodec_parameters_to_context(videoDecoderCtx, pFormatCtx->streams[video_index]->codecpar);
	if (avcodec_open2(videoDecoderCtx, video_decoder, NULL) < 0) exit(-1);

    AVCodecContext *audioDecoderCtx = avcodec_alloc_context3(audio_decoder);
	avcodec_parameters_to_context(audioDecoderCtx, pFormatCtx->streams[audio_index]->codecpar);
	if (avcodec_open2(audioDecoderCtx, audio_decoder, NULL) < 0) exit(-1);

    struct SwsContext * sws_ctx = sws_getContext(videoDecoderCtx->width, videoDecoderCtx->height, videoDecoderCtx->pix_fmt, videoDecoderCtx->width, videoDecoderCtx->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, videoDecoderCtx->width, videoDecoderCtx->height, 32);
    uint8_t * buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
    AVFrame * pict = av_frame_alloc();
    av_image_fill_arrays(pict->data, pict->linesize, buffer, AV_PIX_FMT_YUV420P, videoDecoderCtx->width, videoDecoderCtx->height, 32);

    SwrContext *resampler = swr_alloc_set_opts(NULL, audioDecoderCtx->channel_layout, AV_SAMPLE_FMT_S16, 44100, audioDecoderCtx->channel_layout, audioDecoderCtx->sample_fmt, audioDecoderCtx->sample_rate, 0,  NULL);
    swr_init(resampler);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) return -1;
    SDL_Window* window = SDL_CreateWindow("Video Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, videoDecoderCtx->width, videoDecoderCtx->height, SDL_WINDOW_OPENGL);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, videoDecoderCtx->width, videoDecoderCtx->height);

    SDL_AudioSpec want, have;
    SDL_zero(want);
    SDL_zero(have);
    want.freq = 44100;
    want.channels = audioDecoderCtx->channels;
    want.format = AUDIO_S16SYS;
    SDL_AudioDeviceID audioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    SDL_PauseAudioDevice(audioDevice, 0);

    AVPacket *packet = av_packet_alloc();
	AVFrame *frame = av_frame_alloc();
    AVFrame *audioframe = av_frame_alloc();
	while ((av_read_frame(pFormatCtx, packet)) >= 0) {
        if (packet->stream_index == video_index) {
            if (avcodec_send_packet(videoDecoderCtx, packet) == 0) {
                while (avcodec_receive_frame(videoDecoderCtx, frame) == 0) {
                    SDL_UpdateYUVTexture(texture, NULL,
                                 frame->data[0], frame->linesize[0],
                                 frame->data[1], frame->linesize[1],
                                 frame->data[2], frame->linesize[2]);

                    SDL_RenderClear(renderer);
                    SDL_RenderCopy(renderer, texture, NULL, NULL);
                    SDL_RenderPresent(renderer);
                    SDL_Delay(1000/30);
                }
            }
		} else if (packet->stream_index == audio_index) {
            if (avcodec_send_packet(audioDecoderCtx, packet) == 0) {
                while (avcodec_receive_frame(audioDecoderCtx, frame) == 0) {
                    int dst_samples = frame->channels * av_rescale_rnd(swr_get_delay(resampler, frame->sample_rate) + frame->nb_samples, 44100, frame->sample_rate, AV_ROUND_UP);
                    uint8_t *audiobuf = NULL;
                    if (av_samples_alloc(&audiobuf, NULL, 1, dst_samples,AV_SAMPLE_FMT_S16, 1) < 0) break;
                    dst_samples = frame->channels * swr_convert( resampler,  &audiobuf,  dst_samples, (const uint8_t**) frame->data,  frame->nb_samples);
                    if (av_samples_fill_arrays(audioframe->data, audioframe->linesize, audiobuf, 1, dst_samples, AV_SAMPLE_FMT_S16, 1) < 0) break;
                    SDL_QueueAudio(audioDevice, audioframe->data[0], audioframe->linesize[0]); 
                }
            }
		}

        av_frame_unref(frame);
        av_packet_unref(packet);
	}

	av_frame_free(&frame);
	av_frame_free(&audioframe);
	av_packet_free(&packet);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}