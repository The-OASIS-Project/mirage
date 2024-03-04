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
 * All contributions to this project are agreed to be licensed under the
 * GPLv3 or any later version. Contributions are understood to be
 * any modifications, enhancements, or additions to the project
 * and become the property of the original author Kris Kersey.
 */

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
