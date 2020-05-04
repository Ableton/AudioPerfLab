/*
 * Copyright (c) 2019 Ableton AG, Berlin
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#import <UIKit/UIKit.h>

#define MAX_NUM_THREADS 8
struct DriveMeasurement
{
  double hostTime;
  double duration;
  int numFrames;
  int cpuNumbers[MAX_NUM_THREADS];
  int numActivePartialsProcessed[MAX_NUM_THREADS];
};

@interface Engine : NSObject

@property(nonatomic) int preferredBufferSize;
@property(nonatomic, readonly) double sampleRate;
@property(nonatomic) int numWorkerThreads;
@property(nonatomic) int numBusyThreads;
@property(nonatomic) double busyThreadPeriod;
@property(nonatomic) double busyThreadCpuUsage;
@property(nonatomic) bool processInDriverThread;
@property(nonatomic) bool isWorkIntervalOn;
@property(nonatomic) double minimumLoad;
@property(nonatomic) int numSines;
@property(nonatomic, readonly) int maxNumSines;

- (void)playSineBurstFor:(double)duration additionalSines:(int)numAdditionalSines;
- (void)fetchMeasurements:(void (^)(struct DriveMeasurement))callback;

@end
