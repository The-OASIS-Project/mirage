#ifndef FRAMERATETRACKER_H
#define FRAMERATETRACKER_H

#define FRAME_RATE_BUFFER_SIZE 120

typedef struct {
    double frameTimes[FRAME_RATE_BUFFER_SIZE];
    int currentIndex;
    int frameCount;
    double accumulatedTime;
    double minFrameRate;
    double maxFrameRate;
    double elapsedTime;
} FrameRateTracker;

void initializeFrameRateTracker(FrameRateTracker* tracker);
void updateFrameRateTracker(FrameRateTracker* tracker, double deltaTime);
double calculateAverageFrameRate(FrameRateTracker* tracker);

#endif /* FRAMERATETRACKER_H */
