// Copyright: 2018, Ableton AG, Berlin. All rights reserved.

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
@property(nonatomic) bool processInDriverThread;
@property(nonatomic) bool isWorkIntervalOn;
@property(nonatomic) double minimumLoad;
@property(nonatomic) int numSines;
@property(nonatomic, readonly) int maxNumSines;

- (void)playSineBurstFor:(double)duration additionalSines:(int)numAdditionalSines;
- (void)fetchMeasurements:(void (^)(struct DriveMeasurement))callback;

@end
