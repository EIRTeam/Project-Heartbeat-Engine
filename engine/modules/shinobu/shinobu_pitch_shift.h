#ifndef SHINOBU_PITCH_SHIFT_H
#define SHINOBU_PITCH_SHIFT_H
#include "miniaudio/miniaudio.h"

#define SPS_PI 3.14159265358979323846
#define SPS_TAU 6.2831853071795864769252867666

#ifdef __cplusplus
extern "C" {
#endif
enum {
	SMB_PS_MAX_FRAME_LENGTH = 8192
};

typedef enum {
	FFT_SIZE_256,
	FFT_SIZE_512,
	FFT_SIZE_1024,
	FFT_SIZE_2048,
	FFT_SIZE_4096,
	FFT_SIZE_MAX
} ma_sps_fft_size;

typedef struct {
	float gInFIFO[SMB_PS_MAX_FRAME_LENGTH];
	float gOutFIFO[SMB_PS_MAX_FRAME_LENGTH];
	float gFFTworksp[2 * SMB_PS_MAX_FRAME_LENGTH];
	float gLastPhase[SMB_PS_MAX_FRAME_LENGTH / 2 + 1];
	float gSumPhase[SMB_PS_MAX_FRAME_LENGTH / 2 + 1];
	float gOutputAccum[2 * SMB_PS_MAX_FRAME_LENGTH];
	float gAnaFreq[SMB_PS_MAX_FRAME_LENGTH];
	float gAnaMagn[SMB_PS_MAX_FRAME_LENGTH];
	float gSynFreq[SMB_PS_MAX_FRAME_LENGTH];
	float gSynMagn[SMB_PS_MAX_FRAME_LENGTH];
	int32_t gRover;
	float lastPitchShift;
} SMBPitchShift;

typedef struct {
	ma_node_config nodeConfig;
	ma_uint32 sampleRate;
	ma_sps_fft_size fftSize;
	int oversampling;
} ma_pitch_shift_node_config;

typedef struct {
	ma_node_base baseNode;
	int fft_size;
	float pitchScale;
	int oversampling;
	SMBPitchShift shiftL;
	SMBPitchShift shiftR;
	float sampleRate;
} ma_pitch_shift_node;

MA_API ma_result ma_pitch_shift_node_init(ma_node_graph *pNodeGraph, const ma_pitch_shift_node_config *pConfig, const ma_allocation_callbacks *pAllocationCallbacks, ma_pitch_shift_node *node);
MA_API ma_pitch_shift_node_config ma_pitch_shift_node_config_init(ma_uint32 sampleRate);
MA_API void ma_pitch_shift_node_set_pitch_scale(ma_pitch_shift_node *node, float pitchScale);
MA_API float ma_pitch_shift_node_get_pitch_scale(ma_pitch_shift_node *node);
MA_API void ma_pitch_shift_node_uninit(ma_pitch_shift_node *pPitchShiftNode, const ma_allocation_callbacks *pAllocationCallbacks);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MINIAUDIO_IMPLEMENTATION) || defined(MA_IMPLEMENTATION)

#include "shinobu_fft.h"

static void ma_smb_pitch_shift_init(SMBPitchShift *pitch_shift) {
	pitch_shift->gRover = 0;
	memset(pitch_shift->gInFIFO, 0, SMB_PS_MAX_FRAME_LENGTH * sizeof(float));
	memset(pitch_shift->gOutFIFO, 0, SMB_PS_MAX_FRAME_LENGTH * sizeof(float));
	memset(pitch_shift->gFFTworksp, 0, 2 * SMB_PS_MAX_FRAME_LENGTH * sizeof(double));
	memset(pitch_shift->gLastPhase, 0, (SMB_PS_MAX_FRAME_LENGTH / 2 + 1) * sizeof(double));
	memset(pitch_shift->gSumPhase, 0, (SMB_PS_MAX_FRAME_LENGTH / 2 + 1) * sizeof(double));
	memset(pitch_shift->gOutputAccum, 0, 2 * SMB_PS_MAX_FRAME_LENGTH * sizeof(double));
	memset(pitch_shift->gAnaFreq, 0, SMB_PS_MAX_FRAME_LENGTH * sizeof(double));
	memset(pitch_shift->gAnaMagn, 0, SMB_PS_MAX_FRAME_LENGTH * sizeof(double));
	pitch_shift->lastPitchShift = 1.0;
}

/* clang-format off */
/****************************************************************************
*
* NAME: smbPitchShift.cpp
* VERSION: 1.2
* HOME URL: http://blogs.zynaptiq.com/bernsee
* KNOWN BUGS: none
*
* SYNOPSIS: Routine for doing pitch shifting while maintaining
* duration using the Short Time Fourier Transform.
*
* DESCRIPTION: The routine takes a pitchShift factor value which is between 0.5
* (one octave down) and 2. (one octave up). A value of exactly 1 does not change
* the pitch. numSampsToProcess tells the routine how many samples in indata[0...
* numSampsToProcess-1] should be pitch shifted and moved to outdata[0 ...
* numSampsToProcess-1]. The two buffers can be identical (ie. it can process the
* data in-place). fftFrameSize defines the FFT frame size used for the
* processing. Typical values are 1024, 2048 and 4096. It may be any value <=
* MAX_FRAME_LENGTH but it MUST be a power of 2. osamp is the STFT
* oversampling factor which also determines the overlap between adjacent STFT
* frames. It should at least be 4 for moderate scaling ratios. A value of 32 is
* recommended for best quality. sampleRate takes the sample rate for the signal
* in unit Hz, ie. 44100 for 44.1 kHz audio. The data passed to the routine in
* indata[] should be in the range [-1.0, 1.0), which is also the output range
* for the data, make sure you scale the data accordingly (for 16bit signed integers
* you would have to divide (and multiply) by 32768).
*
* COPYRIGHT 1999-2015 Stephan M. Bernsee <s.bernsee [AT] zynaptiq [DOT] com>
*
* 						The Wide Open License (WOL)
*
* Permission to use, copy, modify, distribute and sell this software and its
* documentation for any purpose is hereby granted without fee, provided that
* the above copyright notice and this license appear in all source copies.
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF
* ANY KIND. See https://dspguru.com/wide-open-license/ for more information.
*
*****************************************************************************/

static void PitchShift(SMBPitchShift *smbPitchShift, float pitchShift, long numSampsToProcess, long fftFrameSize, long osamp, float sampleRate, const float *indata, float *outdata,int stride) {


	/*
		Routine smbPitchShift(). See top of file for explanation
		Purpose: doing pitch shifting while maintaining duration using the Short
		Time Fourier Transform.
		Author: (c)1999-2015 Stephan M. Bernsee <s.bernsee [AT] zynaptiq [DOT] com>
	*/

	double magn, phase, tmp, window, real, imag;
	double freqPerBin, expct;
	long i,k, qpd, index, inFifoLatency, stepSize, fftFrameSize2;
	unsigned long fftFrameBufferSize;

	/* set up some handy variables */
	fftFrameBufferSize = (unsigned long)fftFrameSize*sizeof(float);
	fftFrameSize2 = fftFrameSize/2;
	stepSize = fftFrameSize/osamp;
	freqPerBin = sampleRate/(double)fftFrameSize;
	expct = 2.*Math_PI*(double)stepSize/(double)fftFrameSize;
	inFifoLatency = fftFrameSize-stepSize;
	if (smbPitchShift->gRover == 0) { smbPitchShift->gRover = inFifoLatency;
}

	/* initialize our static arrays */

	/* main processing loop */
	for (i = 0; i < numSampsToProcess; i++){
		/* As long as we have not yet collected enough data just read in */
		smbPitchShift->gInFIFO[smbPitchShift->gRover] = indata[i*stride];
		outdata[i*stride] = smbPitchShift->gOutFIFO[smbPitchShift->gRover-inFifoLatency];
		smbPitchShift->gRover++;

		/* now we have enough data for processing */
		if (smbPitchShift->gRover >= fftFrameSize) {
			smbPitchShift->gRover = inFifoLatency;

			/* do windowing and re,im interleave */
			for (k = 0; k < fftFrameSize;k++) {
				window = -.5*cos(2.*Math_PI*(double)k/(double)fftFrameSize)+.5;
				smbPitchShift->gFFTworksp[2*k] = smbPitchShift->gInFIFO[k] * window;
				smbPitchShift->gFFTworksp[2*k+1] = 0.;
			}


			/* ***************** ANALYSIS ******************* */
			/* do transform */
			smbFft(smbPitchShift->gFFTworksp, fftFrameSize, -1);

			/* this is the analysis step */
			for (k = 0; k <= fftFrameSize2; k++) {
				/* de-interlace FFT buffer */
				real = smbPitchShift->gFFTworksp[2*k];
				imag = smbPitchShift->gFFTworksp[2*k+1];

				/* compute magnitude and phase */
				magn = 2.*sqrt(real*real + imag*imag);
				phase = atan2(imag,real);

				/* compute phase difference */
				tmp = phase - smbPitchShift->gLastPhase[k];
				smbPitchShift->gLastPhase[k] = phase;

				/* subtract expected phase difference */
				tmp -= (double)k*expct;

				/* map delta phase into +/- Pi interval */
				qpd = tmp/Math_PI;
				if (qpd >= 0) { qpd += qpd&1;
				} else { qpd -= qpd&1;
}
				tmp -= Math_PI*(double)qpd;

				/* get deviation from bin frequency from the +/- Pi interval */
				tmp = osamp*tmp/(2.*Math_PI);

				/* compute the k-th partials' true frequency */
				tmp = (double)k*freqPerBin + tmp*freqPerBin;

				/* store magnitude and true frequency in analysis arrays */
				smbPitchShift->gAnaMagn[k] = magn;
				smbPitchShift->gAnaFreq[k] = tmp;

			}

			/* ***************** PROCESSING ******************* */
			/* this does the actual pitch shifting */
			memset(smbPitchShift->gSynMagn, 0, fftFrameBufferSize);
			memset(smbPitchShift->gSynFreq, 0, fftFrameBufferSize);
			for (k = 0; k <= fftFrameSize2; k++) {
				index = k*pitchShift;
				if (index <= fftFrameSize2) {
					smbPitchShift->gSynMagn[index] += smbPitchShift->gAnaMagn[k];
					smbPitchShift->gSynFreq[index] = smbPitchShift->gAnaFreq[k] * pitchShift;
				}
			}

			/* ***************** SYNTHESIS ******************* */
			/* this is the synthesis step */
			for (k = 0; k <= fftFrameSize2; k++) {
				/* get magnitude and true frequency from synthesis arrays */
				magn = smbPitchShift->gSynMagn[k];
				tmp = smbPitchShift->gSynFreq[k];

				/* subtract bin mid frequency */
				tmp -= (double)k*freqPerBin;

				/* get bin deviation from freq deviation */
				tmp /= freqPerBin;

				/* take osamp into account */
				tmp = 2.*Math_PI*tmp/osamp;

				/* add the overlap phase advance back in */
				tmp += (double)k*expct;

				/* accumulate delta phase to get bin phase */
				smbPitchShift->gSumPhase[k] += tmp;
				phase = smbPitchShift->gSumPhase[k];

				/* get real and imag part and re-interleave */
				smbPitchShift->gFFTworksp[2*k] = magn*cos(phase);
				smbPitchShift->gFFTworksp[2*k+1] = magn*sin(phase);
			}

			/* zero negative frequencies */
			for (k = fftFrameSize+2; k < 2*fftFrameSize; k++) { smbPitchShift->gFFTworksp[k] = 0.;
}

			/* do inverse transform */
			smbFft(smbPitchShift->gFFTworksp, fftFrameSize, 1);

			/* do windowing and add to output accumulator */
			for(k=0; k < fftFrameSize; k++) {
				window = -.5*cos(2.*Math_PI*(double)k/(double)fftFrameSize)+.5;
				smbPitchShift->gOutputAccum[k] += 2.*window*smbPitchShift->gFFTworksp[2*k]/(fftFrameSize2*osamp);
			}
			for (k = 0; k < stepSize; k++) { smbPitchShift->gOutFIFO[k] = smbPitchShift->gOutputAccum[k];
}

			/* shift accumulator */
			memmove(smbPitchShift->gOutputAccum, smbPitchShift->gOutputAccum+stepSize, fftFrameBufferSize);

			/* move input FIFO */
			for (k = 0; k < inFifoLatency; k++) { smbPitchShift->gInFIFO[k] = smbPitchShift->gInFIFO[k+stepSize];
}
		}
	}
}

/* Godot code again */
/* clang-format on */

static void ma_pitch_shift_node_process_pcm_frames(ma_node *pNode, const float **ppFramesIn, ma_uint32 *pFrameCountIn, float **ppFramesOut, ma_uint32 *pFrameCountOut) {
	ma_pitch_shift_node *pPitchShift = (ma_pitch_shift_node *)pNode;

	float tolerance = 0.00001 * abs(pPitchShift->pitchScale);
	if (tolerance < 0.00001) {
		tolerance = 0.00001;
	}

	pFrameCountOut[0] = pFrameCountIn[0];

	// For pitch_scale 1.0 it's cheaper to just pass samples without processing them.
	if (abs(pPitchShift->pitchScale - 1.0f) < tolerance) {
		ma_copy_pcm_frames(ppFramesOut[0], ppFramesIn[0], pFrameCountIn[0], ma_format_f32, ma_node_get_output_channels(pNode, 0));
		return;
	}

	const float *in_l = ppFramesIn[0];
	const float *in_r = in_l + 1;

	float *out_l = ppFramesOut[0];
	float *out_r = out_l + 1;

	PitchShift(&pPitchShift->shiftL,
			pPitchShift->pitchScale,
			pFrameCountIn[0],
			pPitchShift->fft_size,
			pPitchShift->oversampling,
			pPitchShift->sampleRate,
			in_l,
			out_l,
			2);
	PitchShift(&pPitchShift->shiftR,
			pPitchShift->pitchScale,
			pFrameCountIn[0],
			pPitchShift->fft_size,
			pPitchShift->oversampling,
			pPitchShift->sampleRate,
			in_r,
			out_r,
			2);
}

static ma_node_vtable g_ma_pitch_shift_node_vtable = {
	ma_pitch_shift_node_process_pcm_frames,
	NULL,
	1, /* 1 input channel. */
	1, /* 1 output channel. */
	MA_NODE_FLAG_CONTINUOUS_PROCESSING
};

MA_API ma_pitch_shift_node_config ma_pitch_shift_node_config_init(ma_uint32 sampleRate) {
	ma_pitch_shift_node_config config;

	MA_ZERO_OBJECT(&config);
	config.nodeConfig = ma_node_config_init();
	config.fftSize = FFT_SIZE_2048;
	config.sampleRate = sampleRate;
	config.oversampling = 4;

	return config;
}

MA_API ma_result ma_pitch_shift_node_init(ma_node_graph *pNodeGraph, const ma_pitch_shift_node_config *pConfig, const ma_allocation_callbacks *pAllocationCallbacks, ma_pitch_shift_node *node) {
	ma_smb_pitch_shift_init(&node->shiftL);
	ma_smb_pitch_shift_init(&node->shiftR);
	node->pitchScale = 1.0f;
	static const int fft_sizes[FFT_SIZE_MAX] = { 256, 512, 1024, 2048, 4096 };
	node->fft_size = fft_sizes[pConfig->fftSize];
	node->oversampling = pConfig->oversampling;
	node->sampleRate = pConfig->sampleRate;

	ma_node_config baseConfig;

	baseConfig = pConfig->nodeConfig;

	baseConfig.vtable = &g_ma_pitch_shift_node_vtable;

	ma_uint32 inputChannels[1];
	ma_uint32 outputChannels[1];

	inputChannels[0] = 2;
	outputChannels[0] = 2;

	baseConfig.pInputChannels = inputChannels;
	baseConfig.pOutputChannels = outputChannels;

	ma_result result = ma_node_init(pNodeGraph, &baseConfig, pAllocationCallbacks, &node->baseNode);

	if (result != MA_SUCCESS) {
		return result;
	}

	return MA_SUCCESS;
}

MA_API void ma_pitch_shift_node_set_pitch_scale(ma_pitch_shift_node *node, float pitchScale) {
	c89atomic_exchange_f32(&node->pitchScale, pitchScale);
}

MA_API float ma_pitch_shift_node_get_pitch_scale(ma_pitch_shift_node *node) {
	return c89atomic_load_f32((float *)&node->pitchScale);
}

MA_API void ma_pitch_shift_node_uninit(ma_pitch_shift_node *pPitchShiftNode, const ma_allocation_callbacks *pAllocationCallbacks) {
	ma_node_uninit(pPitchShiftNode, pAllocationCallbacks);
}

#endif
