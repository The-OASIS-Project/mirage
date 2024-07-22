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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "frame_rate_tracker.h"

void initializeFrameRateTracker(FrameRateTracker* tracker) {
    int i;
    for (i = 0; i < FRAME_RATE_BUFFER_SIZE; i++) {
        tracker->frameTimes[i] = 0.0;
    }
    tracker->currentIndex = 0;
    tracker->frameCount = 0;
    tracker->accumulatedTime = 0.0;
    tracker->minFrameRate = 9999.0; // Initialize with a high value
    tracker->maxFrameRate = 0.0;    // Initialize with a low value
    tracker->elapsedTime = 0.0;
}

void updateFrameRateTracker(FrameRateTracker* tracker, double deltaTime) {
    tracker->accumulatedTime += deltaTime;
    tracker->elapsedTime += deltaTime;
    tracker->frameCount++;

    if (tracker->frameCount > 1) {
        double frameRate = 1.0 / deltaTime;
        tracker->frameTimes[tracker->currentIndex] = frameRate;
        tracker->currentIndex = (tracker->currentIndex + 1) % FRAME_RATE_BUFFER_SIZE;

        if (tracker->frameCount > FRAME_RATE_BUFFER_SIZE) {
            tracker->accumulatedTime -= tracker->frameTimes[tracker->currentIndex];
        }

	// Update minimum and maximum frame rates
        if (frameRate < tracker->minFrameRate) {
            tracker->minFrameRate = frameRate;
        }
        if (frameRate > tracker->maxFrameRate) {
            tracker->maxFrameRate = frameRate;
        }
    }
}

double calculateAverageFrameRate(FrameRateTracker* tracker) {
    if (tracker->frameCount > 0) {
        double totalFrameRate = 0.0;
        int i;
        for (i = 0; i < FRAME_RATE_BUFFER_SIZE; i++) {
            totalFrameRate += tracker->frameTimes[i];
        }
        return totalFrameRate / FRAME_RATE_BUFFER_SIZE;
    }
    return 0.0;
}
